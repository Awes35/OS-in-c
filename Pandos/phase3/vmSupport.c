/**************************************************************************** 
 *
 * This module implements the TLB exception handler (i.e., The Pager) and contains
 * the functions for reading and writing flash devices. Furthermore, the module
 * also initializes both the Swap Pool table and the accompanying semaphore.
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
HIDDEN void mutex(int opCode); /* function declaration for the function that is responsible for gaining or losing mutual exclusion over the Swap Pool table */
HIDDEN void flashOperation(int readOrWrite); /* function declaration for the function that is responsible for reading or writing to a */

/* declaring variables that are global to this module */
int swapSem; /* mutual exclusion semaphore that controls access to the Swap Pool data structure */
swap_t swapPoolTbl[2 * UPROCMAX]; /* the Swap Pool data structure/table */

/* Function that is responsible for gaining or losing mutual exclusion over the Swap Pool table. Note that the integer parameter
indicates whether one wishes to gain (1) or lose (0) mutual exclusion. More specifically, this function issues a SYS 3 (a P operation)
on the Swap Pool semaphore if one wishes to gain mutual exclusion, and the function issues a SYS 4 (a V operation) if one wishes to
relinquish mutual exclusion over the Swap Pool table. */
mutex(int opCode){
	if (opCode == TRUE){ /* if the parameter passed in is 1, meaning one wishes to gain mutual exclusion */
		SYSCALL(SYSNUM3, &swapSem, 0, 0); /* issuing a SYS 3 (i.e., performing a P operation) on the Swap Pool semaphore */
	}
	else{ /* the parameter passed in is 0, meaning one wishes to lose mutual exclusion */
		SYSCALL(SYSNUM4, &swapSem, 0, 0); /* issuing a SYS 4 (i.e., performing a V operation) on the Swap Pool semaphore */
	}
}

/* Function that initializes the Swap Pool table and the Swap Pool semaphore (i.e., the variables that are global
to this module) */
void initSwapStructs(){
	swapSem = 1; /* initializing the Swap Pool semaphore to 1, since it will be used for the purpose of mutual exclusion */

	/* initializing the Swap Pool table */
	int i;
	for (i = 0; j < (2 * UPROCMAX); i++){
		swapPoolTbl[i].asid = EMPTYFRAME; /* initializing the ASID to -1, since all frames are currently unoccupied */
	}
}

/* Function that handles page faults that are passed up by the Nucleus. Note that this function utilizes a FIFO page replacement
algorithm. */
void vmTlbHandler(){
	/* declaring local variables */
	state_t oldState; /* a pointer to the saved exception state responsible for the TLB exception */
	support_t *curProcSupportStruct; /* a pointer to the Current Process' Support Structure */
	int exceptionCode; /* the exception code */
	int missingPgNo; /* the missing page number, as indicated in the saved exception state's EntryHi field */
	static int frameNo; /* the frame number used to satisfy a page fault */
	memaddr frameAddr; /* the address of the frame selected by the page replacement algorithm to satisfy the page fault */

	curProcSupportStruct = SYSCALL(SYS8NUM, 0, 0, 0); /* obtaining a pointer to the Current Process' Support Structure */
	oldState = curProcSupportStruct->sup_exceptState[PGFAULTEXCEPT]; /* initializing oldState to the state found in the Current Process' Support Structure for TLB exceptions */
	exceptionCode = ((oldState.s_cause) & GETEXCEPCODE) >> CAUSESHIFT; /* initializing the exception code so that it matches the exception code stored in the .ExcCode field in the Cause register */

	if (exceptionCode == TLBMODEXCCODE){ /* if the exception code indicates that a TLB-Modification exception occurred */
		programTrapHandler(); /* invoking the function that handles program traps in phase 3 */
	}

	mutex(TRUE); /* calling the internal helper function to gain mutual exclusion over the Swap Pool table */

	/* determining the mising page number found in the saved exception state's EntryHI field */
	missingPgNo = ((oldState.s_entryHI) & GETVPN) >> VPNSHIFT; /* initializing the missing page number to the VPN specified in the EntryHI field of the saved exception state */
	missingPgNo = missingPgNo % ENTRIESPERPG; /* using the hash function to determine the page number of the missing TLB entry from the VPN calculated in the previous line */

	frameNo = (frameNo + 1) % SWAPPOOLSIZE; /* selecting a frame to satisfy the page fault, as determined by Pandos' FIFO page replacement algorithm */
	frameAddr = ()
}



