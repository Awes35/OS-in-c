/**************************************************************************** 
 *
 * This module is responsible for implementing the test() function, declaring 
 * and initializing the phase 3 global variables, which include an array of
 * device semaphores and the masterSemaphore responsible for ensuring test()
 * comes to a more graceful conclusion by calling HALT() instead of PANIC().
 * In slightly greater detail, the module initializes the processor state
 * for the instantiator process "test," initializes the Support Structure
 * for the U-proc, and launches UPROCMAX processes. This module (via test())
 * also invokes the function in vmSupport.c that is responsible for initializing
 * virtual memory (i.e., the Swap Pool table and the accompanying semaphore).
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
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* function declarations */
HIDDEN void initProcessorState(state_PTR newState); /* function declaration for the function that is responsible for initializing the processor state for a U-proc */

/* declaring the phase 3 global variables */
int masterSemaphore; /* semaphore to be V'd and P'd by test as a means to ensure test terminates in a way so that the PANIC() function is not called */
int devSemaphores[MAXIODEVICES]; /* array of mutual exclusion semaphores; each potentially sharable peripheral I/O device has one
									semaphore defined for it. Note that this array will be implemented so that terminal device semaphores
									are last and terminal device semaphores	associated with a read operation in the array come before those
									associated with a write operation. */

/* Function that initializes the processor state (which is passed into the function as a parameter) for a U-proc. This function sets the address of the
state's PC (and t9 register) to 0x8000.00B0, which is the address of the start of the .text section, sets the SP to 0xC000.0000, and sets the Status register
for user-mode with all interrupts and the processor Local Timer enabled. */
void initProcessorState(state_PTR newState){
	newState->s_pc = newState->s_t9 = (memaddr) UPROCPC; /* initializing the U-proc's PC (and the contents of the U-proc's t9 register) to 0x800000B0 */
	newState->s_sp = (memaddr) UPROCSP; /* initializing the U-proc's stack pointer to 0xC0000000 */
	newState->s_status = ALLOFF | USERPON | IEPON | PLTON | IMON; /* initializing the U-procs' status register so that interrupts are enabled, user-mode is on and the PLT is enabled */
}

/* Function that represents the instantiator process. The function initializes the Phase 3 global variables, calls the function in 
vmSupport.c that initializes virtual memory (including the Swap Pool table and the Swap Pool semaphore), initializes the processor state
for the U-proc, initializes the Support Structure for the U-proc, launches UPROCMAX processes, and terminates after all of its U-proc
"children" processes conclude */
void test(){
	/* declaring local variables */
	int pid; /* the process ID (which is equivalent to the ASID) of the process that is instantiated */
	static support_t supportStructArr[UPROCMAX + 1]; /* static array of UPROCMAX + 1 Support Strucures that will allow one to obtain the
													address of the next unused Support Structure that needs to be initialized */
	int returnCode; /* the value that is returned from SYS1 when launching a new U-proc */
	state_t initialState; /* the processor state for a U-proc, which will be initialized in this module */

	/* initializing the I/O device semaphores to 1, since they will be used for the purpose of mutual exclusion */
	int i;
	for (i = 0; i < MAXIODEVICES; i++){
		devSemaphores[i] = 1; 
	}
	
	initSwapStructs(); /* calling the function in the vmSupport.c module that initializes virtual memory */
	initProcessorState(&initialState); /* calling the internal function that initializes the processor state of a given U-proc */

	/* initializing UPROCMAX U-procs */
	for (pid = 1; pid < UPROCMAX + 1; pid++){

		/* completing the initialization of the processor state for the U-proc using the process' unique ID*/
		initialState.s_entryHI = ALLOFF | (pid << ASIDSHIFT); /* initializing the U-proc's EntryHi.ASID field to the process' unique ID */

		/* initializing the Support Structure for the U-proc */
		supportStructArr[pid].sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr) vmTlbHandler; /* initializing the PC for handling page fault excpetions to the address of this phase's TLB handler */
		supportStructArr[pid].sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr) vmGeneralExceptionHandler; /* initializing the PC for handling exceptions that are not related to page faults to the
																											address of this phase's general exception handler */
		supportStructArr[pid].sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | IEPON | PLTON | IMON; /* enabling interrupts, setting kernel-mode to on and enabling PLT */
		supportStructArr[pid].sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | IEPON | PLTON | IMON; /* enabling interrupts, setting kernel-mode to on and enabling PLT */
		supportStructArr[pid].sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (unsigned int) &(supportStructArr[pid].sup_stackTLB[TOPOFSTACK]); /* setting the SP field for handling page fault exceptions to the address
																																of the top of the stack reserved for handling TLB exceptions */
		supportStructArr[pid].sup_exceptContext[GENERALEXCEPT].c_stackPtr = (unsigned int) &(supportStructArr[pid].sup_stackGen[TOPOFSTACK]); /* setting the SP field for handling non-page fault exceptions to the address
																																of the top of the stack reserved for handling such exceptions */																												
		supportStructArr[pid].sup_asid = pid; /* initializing the U-proc's ASID */

		/* initializing the Page Table for the U-proc */
		int j;
		for (j = 0; j < ENTRIESPERPG; j++){
			supportStructArr[pid].sup_privatePgTbl[j].entryHI = ALLOFF | ((VPNSTART + j) << VPNSHIFT) | (pid << ASIDSHIFT); /* initializing the EntryHI fields in the U-proc's Page Table */
			supportStructArr[pid].sup_privatePgTbl[j].entryLO = ALLOFF | DBITON; /* initializing the EntryLo fields in the U-proc's Page Table so that the G and V bits are off and the D bit is on */
		}

		supportStructArr[pid].sup_privatePgTbl[ENTRIESPERPG - 1].entryHI = ALLOFF | (STACKPGVPN << VPNSHIFT) | (pid << ASIDSHIFT); /* (re)initializing the stack page's EntryHI fields in the U-proc's Page Table */

		returnCode = SYSCALL(SYS1NUM, (unsigned int) (&initialState), (unsigned int) &(supportStructArr[pid]), 0); /* issuing the SYS 1 to launch the new U-proc and assigning the function's return value to returnCode */

		if (returnCode != SUCCESSCONST){ /* if the new U-proc was not launched successfully */
			SYSCALL(SYS2NUM, 0, 0, 0); /* terminate the process */
		}
	}

	masterSemaphore = 0; /* initializing the master semaphore to 0, since it will be used for synchronization */

	/* performing a P operation on masterSemaphore UPROCMAX number of times in order to contribute to a more graceful conclusion of test() */
	int k;
	for (k = 0; k < UPROCMAX; k++){ 
		SYSCALL(SYS3NUM, (unsigned int) &masterSemaphore, 0, 0); /* performing a P operation on masterSemaphore */	
	}

	SYSCALL(SYS2NUM, 0, 0, 0); /* terminating the instantiator process, since all of its U-proc "children" have completed */
}
