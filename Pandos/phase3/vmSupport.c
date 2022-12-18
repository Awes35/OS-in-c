/**************************************************************************** 
 *
 * This module implements the TLB exception handler (i.e., The Pager) and contains
 * the functions for reading and writing flash devices. The module
 * also initializes both the Swap Pool table and the accompanying semaphore. 
 * Furthermore, the module contains several helper functions that are called
 * by the Pager. For example, the module contains a helper function that
 * turns interrupts in the Status register on and off, a helper function that
 * is responsible for gaining or releasing mutual exclusion and a helper function
 * that is responsible for returning control back to a particular process. In 
 * short, this module is responsible for handling page faults and initializing
 * virtual memory in phase 3.
 *  
 * Written by: Kollen Gruizenga and Jake Heyser
 ****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h" 
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* function declarations */
HIDDEN void flashOperation(int readOrWrite, int pid, memaddr frameAddress, int missingPgNum); /* function declaration for the function that is responsible for reading or writing to a flash device */

/* declaring variables that are global to this module */
int swapSem; /* mutual exclusion semaphore that controls access to the Swap Pool data structure */
HIDDEN swap_t swapPoolTbl[MAXFRAMECNT]; /* the Swap Pool data structure/table */

/* Function that turns interrupts in the Status register on or off, as indicated by the function's parameter. If the caller wishes to
turn interrupts on, then the value 1 is passed into the function, whereas if the caller wishes to disable interrupts, then the value 0
is passed into the function. */
void setInterrupts(int onOrOff){
	if (onOrOff == TRUE){ /* the caller wishes to enable interrupts */
		setSTATUS(getSTATUS() | IECON); /* enabling interrupts for the Status register, as dictated by the function's parameter */
	} 
	else{ /* the caller wishes to disable interrupts */
		setSTATUS(getSTATUS() & IECOFF); /* disabling interrupts for the Status register, as dictated by the function's parameter */
	}
}

/* Function that is responsible for gaining or losing mutual exclusion. Note that the integer parameter indicates whether one wishes to
gain (1) or lose (0) mutual exclusion. More specifically, this function issues a SYS 3 (a P operation) on the semaphore whose pointer is 
passed into the function as a parameter if one wishes to gain mutual exclusion, and the function issues a SYS 4 (a V operation) on the
semaphore whose pointer is passed into the function if one wishes to relinquish mutual exclusion. */
void mutex(int opCode, int *semaphore){
	if (opCode == TRUE){ /* if the parameter passed in is 1, meaning one wishes to gain mutual exclusion */
		SYSCALL(SYS3NUM, (unsigned int) semaphore, 0, 0); /* issuing a SYS 3 (i.e., performing a P operation) on the desired semaphore */
	}
	else{ /* the parameter passed in is 0, meaning one wishes to lose mutual exclusion */
		SYSCALL(SYS4NUM, (unsigned int) semaphore, 0, 0); /* issuing a SYS 4 (i.e., performing a V operation) on the desired semaphore */
	}
}

/* Function that is responsible for reading or writing to process pid's flash device/backing store. More specifically, this function
performs five key steps. First off, the function gains mutual exclusion over the appropriate device's device register. Next, it writes
the device's DATA0 field with the particular frame's starting address (as indicated by the parameter frameAddress). Then, the function
writes the device's COMMAND field with the device block number and the command to read or write (as indicated by the parameter readOrWrite).
Then, the function issues a SYS 5 with the appropriate parameters to block the I/O requesting process process until the operation
completes before releasing mutual exclusion over the device's device register. */
void flashOperation(int readOrWrite, int pid, memaddr frameAddress, int missingPgNum){
	/* declaring local variables */
	devregarea_t *temp; /* device register area that we can use to read and write the process pid's flash device */
	int index; /* the index in devreg (and in devSemaphores) of the flash device associated with process pid */
	int blockNum; /* the appropriate block number of the flash device */
	int statusCode; /* the status code from the device register associated with the device that corresponds to the highest-priority interrupt */

	/* initializing local variables, except for statusCode, which will be initialized later */
	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */		
	index = ((FLASHINT - OFFSET) * DEVPERINT) + (pid - 1); /* initializing the index in devreg (and in devSemaphores) of process pid's flash device/backing store */
	blockNum = missingPgNum; /* initializing the device block number that we will place in the COMMAND field of the correct flash device later on */

	mutex(TRUE, (int *) &(devSemaphores[index])); /* calling the function that gains mutual exclusion exclusion over process pid's flash device's device register */
	temp->devreg[index].d_data0 = frameAddress; /* writing the flash device's DATA0 field with the selected frame number's starting address */
	setInterrupts(FALSE); /* calling the function that disables interrupts in order to write the COMMAND field and issue the SYS 5 atomically */
	
	if (readOrWrite == TRUE){ /* if the caller wishes to read from the flash device */
		temp->devreg[index].d_command = READBLK | (blockNum << BLKNUMSHIFT); /* writing the device's COMMAND field with the device block number and the command to read */
	}
	else{ /* the caller wishes to write to the flash device */
		temp->devreg[index].d_command = WRITEBLK | (blockNum << BLKNUMSHIFT); /* writing the device's COMMAND field with the device block number and the command to write */
	}

	SYSCALL(SYS5NUM, LINE4, (pid - 1), readOrWrite); /* issuing the SYS 5 call to block the I/O requesting process until the operation completes */
	setInterrupts(TRUE); /* calling the function that enables interrupts for the Status register, since the atomically-executed steps have now been completed */
	statusCode = temp->devreg[index].d_status; /* setting the status code from the device register associated with process pid's flash device */
	mutex(FALSE, (int *) &(devSemaphores[index])); /* calling the function that releases mutual exclusion over process pid's flash device's device register */
	
	if (statusCode != READY){ /* if the read or write operation led to an error status */
		mutex(FALSE, (int *) &swapSem); /* calling the internal helper function to make sure we release mutual exclusion over the Swap Pool table */
		programTrapHandler(); /* invoking the function that handles program traps in phase 3 */
	}
}

/* Function that initializes the Swap Pool table and the Swap Pool semaphore (i.e., the variables that are global
to this module). Since the Swap Pool semaphore is used for mutual exclusion, the function initializes the semaphore to 1,
and the function initializes every entry's ASID in the Swap Pool table to -1, since none of the frames in the Swap Pool
are curretly occupied. */
void initSwapStructs(){
	swapSem = 1; /* initializing the Swap Pool semaphore to 1, since it will be used for the purpose of mutual exclusion */

	/* initializing the Swap Pool table */
	int i;
	for (i = 0; i < MAXFRAMECNT; i++){
		swapPoolTbl[i].asid = EMPTYFRAME; /* initializing the ASID to -1, since all frames are currently unoccupied */
	}
}

/* Function that returns control back to a particular process whose processor state is returnState. This function is used
primarily as a debugging function. */
void switchUContext(state_PTR returnState){
	LDST(returnState); /* returning control back to the desired process */
}

/* Function that handles page faults that are passed up by the Nucleus. Note that this function utilizes a FIFO page replacement
algorithm. Now, more specifically, the function obtains a pointer to the Current Process' Support Structure, determines the cause
of the TLB exception, and then, if the cause is a TLB-Modification exception, passes control to the phase 3 function that handles
Program Traps. Next, the function gains mutual exclusion over the Swap Pool table, determines the missing page number, selects
a frame from the Swap Pool using a FIFO page replacement algorithm to satisfy the page fault, and determines if the selected frame
is occupied. If it is occupied, the function then updates the correct process' Page Table, updates the TLB (if needed), and then
updates the correct process' backing store. Next, the function reads the contents of the Current Process' backing store's correct logical
page into the frame previously selected, updates the Swap Pool table, updates the Current Process' Page Table, and then updates the
TLB. Finally, the function releases mutual exclusion over the Swap Pool table before returning control back to the Current Process
to retry the instruction that caused the page fault. */
void vmTlbHandler(){
	/* declaring local variables */
	state_PTR savedState; /* a pointer to the saved exception state responsible for the TLB exception */
	support_t *curProcSupportStruct; /* a pointer to the Current Process' Support Structure */
	memaddr frameAddr; /* the address of the frame selected by the page replacement algorithm to satisfy the page fault */
	int exceptionCode; /* the exception code */
	int missingPgNo; /* the missing page number, as indicated in the saved exception state's EntryHi field */
	HIDDEN int frameNo; /* the frame number used to satisfy a page fault */

	curProcSupportStruct = (support_t *) SYSCALL(SYS8NUM, 0, 0, 0); /* obtaining a pointer to the Current Process' Support Structure */
	savedState = &(curProcSupportStruct->sup_exceptState[PGFAULTEXCEPT]); /* initializing savedState to the state found in the Current Process' Support Structure for TLB exceptions */
	exceptionCode = ((savedState->s_cause) & GETEXCEPCODE) >> CAUSESHIFT; /* initializing the exception code so that it matches the exception code stored in the .ExcCode field in the Cause register */

	if (exceptionCode == TLBMODEXCCODE){ /* if the exception code indicates that a TLB-Modification exception occurred */
		programTrapHandler(); /* invoking the function that handles Program Traps in phase 3 */
	}

	mutex(TRUE, (int *) &swapSem); /* calling the internal helper function to gain mutual exclusion over the Swap Pool table */

	/* determining the mising page number found in the saved exception state's EntryHI field */
	missingPgNo = ((savedState->s_entryHI) & GETVPN) >> VPNSHIFT; /* initializing the missing page number to the VPN specified in the EntryHI field of the saved exception state */
	missingPgNo = missingPgNo % ENTRIESPERPG; /* using the hash function to determine the page number of the missing TLB entry from the VPN calculated in the previous line */

	frameNo = (frameNo + 1) % MAXFRAMECNT; /* selecting a frame to satisfy the page fault, as determined by Pandos' FIFO page replacement algorithm */
	frameAddr = SWAPPOOLADDR + (frameNo * PAGESIZE); /* calculating the frameNo's starting address */

	if (swapPoolTbl[frameNo].asid != EMPTYFRAME){ /* if the frame selected by the page replacement algorithm is occupied */
		setInterrupts(FALSE); /* calling the function that disables interrupts for the Status register so we can update the page table entry and its cached counterpart in the TLB atomically */
		swapPoolTbl[frameNo].ownerProc->entryLO = (swapPoolTbl[frameNo].ownerProc->entryLO) & VBITOFF; /* updating the page table for the process occupying the frame by marking the entry as not valid */
		TLBCLR(); /* erasing all of the entries in the TLB to ensure cache consistency */
		setInterrupts(TRUE); /* calling the function that enables interrupts for the Status register, since the atomically-executed steps have now been completed */
		flashOperation(WRITE, swapPoolTbl[frameNo].asid, frameAddr, swapPoolTbl[frameNo].pgNo); /* calling the internal helper function to update the correct process' backing store */
	}

	flashOperation(READ, curProcSupportStruct->sup_asid, frameAddr, missingPgNo); /* calling the internal helper function to read the contents of the Current Process' missing page number into frame frameNo */

	/* updating the Swap Pool table to reflect the frame's new contents */
	swapPoolTbl[frameNo].pgNo = missingPgNo; /* updating the page number field for the appropriate frame's entry in the Swap Pool table */
	swapPoolTbl[frameNo].asid = curProcSupportStruct->sup_asid; /* updating the ASID field for the appropriate frame's entry in the Swap Pool table */
	swapPoolTbl[frameNo].ownerProc = &(curProcSupportStruct->sup_privatePgTbl[missingPgNo]); /* updating the ownerProc field in the appropriate frame's entry in the Swap Pool table */

	setInterrupts(FALSE); /* calling the function that disables interrupts for the Status register so we can update the page table entry and the TLB atomically */

	/* updating the appropriate Page Table entry for the Current Process */
	curProcSupportStruct->sup_privatePgTbl[missingPgNo].entryLO = frameAddr | VBITON | DBITON; /* ensuring the V and D bits are on and that the PFN field of the appropriate Page Table entry for the Current Process is updated */

	TLBCLR(); /* erasing all of the entries in the TLB to ensure cache consistency */
	setInterrupts(TRUE); /* calling the function that enables interrupts for the Status register, since the atomically-executed steps have now been completed */
	mutex(FALSE, (int *) &swapSem); /* calling the internal helper function to release mutual exclusion over the Swap Pool table */
	switchUContext(savedState); /* calling the internal helper function to return control to the Current Process to retry the instruction that caused the page fault */
}
