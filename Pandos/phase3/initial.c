/**************************************************************************** 
 *
 * This module adds several important components to the development of our
 * Pandos operating system. First off, this module contains the entry point
 * for Phase 2: main(). This function will, among other tasks, initialize
 * the various Phase 1 data structures (ie: the ASL, free list of semaphore descriptors,
 * and the process queue that we will use to hold processes that are 
 * ready to be executed), initialize four words in the BIOS data page (ie: one
 * word each for the general exception handler, a corresponding stack pointer, the 
 * TLB-Refill handler, and a corresponding stack pointer), and create one
 * process on the ReadyQueue so that the Scheduler can begin. 
 * 
 * We also define & initialize the global variables needed in Phase 2 of development, 
 * and we implement the general exception handler in this module. The general exception
 * handler passes the handling of interrupts along to the device interrupt handler
 * and it passes the handling of all exceptions to the appropriate function
 * in the exceptions.c module to handle the particular type of exception.
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
#include "/usr/include/umps3/umps/libumps.h"

/* function declarations */
extern void test(); /* function declaration for test(), which will be defined in the test file for this module */
extern void uTLB_RefillHandler(); /* function declaration for uTLB_RefillHandler(), which will be defined in exceptions.c */
HIDDEN void generalExceptionHandler(); /* function declaration for the internal function that is responsible for handling general exceptions */

/* declaring global variables */
pcb_PTR ReadyQueue; /* pointer to the tail of a queue of pcbs that are in the "ready" state */
pcb_PTR currentProc; /* pointer to the pcb that is in the "running" state */
int procCnt; /* integer indicating the number of started, but not yet terminated, processes */
int softBlockCnt; /* integer indicating the number of started, but not yet terminated, processes that're in the "blocked" state" */
int deviceSemaphores[MAXDEVICECNT]; /* array of integer semaphores that correspond to each external (sub) device, plus one semd for the Pseudo-clock, located 
									at the last index of the array (PCLOCKIDX). Note that this array will be implemented so that terminal device semaphores are last and terminal device semaphores
									associated with a read operation in the array come before those associated with a write operation. */
cpu_t start_tod; /* the value on the time of day clock that the Current Process begins executing at */
state_PTR savedExceptState; /* a pointer to the saved exception state */

/* Internal function that is responsible for handling general exceptions. For interrupts, processing is passed along to 
the device interrupt handler. For TLB exceptions, processing is passed along to the TLB exception handler, and for
program traps, processing is passed along to the Program Trap exception handler. Finally, for exception code 8
(SYSCALL) events, processing is passed along to the SYSCALL exception handler. */
void generalExceptionHandler(){
	/* declaring local variables */
	state_t *oldState; /* the saved exception state for Processor 0 */
	int exceptionReason; /* the exception code */

	/* initializing local variables */
	oldState = (state_t *) BIOSDATAPAGE; /* getting the saved exception state at the start of the BIOS Data Page */
	exceptionReason = ((oldState->s_cause) & GETEXCEPCODE) >> CAUSESHIFT; /* initializing the exception code so that it matches the exception code stored in the .ExcCode field in the Cause register */

	if (exceptionReason == INTCONST){ /* if the exception code is 0 */
		intTrapH(); /* calling the Nucleus' device interrupt handler function */
	}
	if (exceptionReason <= TLBCONST){ /* if the exception code is between 1 and 3 (inclusive) */
		tlbTrapH(); /* calling the Nucleus' TLB exception handler function */
	}
	if (exceptionReason == SYSCONST){ /* if the exception code is 8 */
		sysTrapH(); /* calling the Nucleus' SYSCALL exception handler function */
	}
	pgmTrapH(); /* calling the Nucleus' Program Trap exception handler function because the exception code is not 0-3 or 8 */
}

/* Function that represents the entry point of our program. It initializes the phase 1 data
* structures (such as the ASL, the free list of pcbs, and the process queue that we 
* will use to hold processes that are ready to be executed), initializes four words in the 
* BIOS data page (i.e., one word each for the general exception handler, a corresponding stack 
* pointer, the TLB-Refill handler, and a corresponding stack pointer), and creates one process
* and calls the Scheduler on it. The function also initializes the global variables for this
* module. */
int main(){
	/* declaring local variables */
	pcb_PTR p; /* a pointer to the process that we will instantiate in this function */
	passupvector_t *procVec; /* a pointer to the Process 0 Pass Up Vector that we will initialize in this function */
	memaddr ramtop; /* the address of the last RAM frame */
	devregarea_t *temp; /* device register area that we can we use to determine the last RAM frame */
	
	/* initializing global variables, except for start_tod, curr_tod, and savedExceptState, which will be initialized later. */
	ReadyQueue = mkEmptyProcQ(); /* initializng the ReadyQueue's tail pointer to be NULL */
	currentProc = NULL; /* setting the pointer to the pcb that is in the "running" state to NULL */
	procCnt = INITIALPROCCNT; /* setting the number of started, but not yet terminated, processes to 0 */
	softBlockCnt = INITIALSFTBLKCNT; /* setting the number of started, but not yet terminated, processes that're in the "blocked" state to 0 */

	int i;
	for (i = 0; i < MAXDEVICECNT; i++){
		/* initializing the device semaphores */
		deviceSemaphores[i] = INITIALDEVSEMA4;
	}

	/* initializing the free list of semaphore descriptors (along with the dummy nodes for the ASL) 
	and the pcbFree list */
	initPcbs(); /* initializing the pcbFree list */
	initASL(); /* initializing the semdFree list and the ASL's dummy nodes */

	/* initializing the Processor 0 Pass Up Vector */
	procVec = (passupvector_t *) PASSUPVECTOR; /* initializing procVec to be a pointer to the address of the Process 0 Pass Up Vector */
	procVec->tlb_refll_handler = (memaddr) uTLB_RefillHandler; /* initializing the address for handling TLB-Refill events */
	procVec->tlb_refll_stackPtr = PROC0STACKPTR; /* initializing the stack pointer for handling Nucleus TLB-Refill events */
	procVec->exception_handler = (memaddr) generalExceptionHandler; /* initializing the address for handling general exceptions */
	procVec->exception_stackPtr = PROC0STACKPTR; /* initializing the stack pointer for handling general exceptions */

	/* loading the system-wide interval timer with 100 (INITIALINTTIMER) milliseconds, before a Pseudo-Clock tick occurs. */
	LDIT(INITIALINTTIMER); /* invoking the macro function that handles setting the system-wide interval timer with a given value */

	/* instantiating a single process so we can call the Scheduler on it */
	p = allocPcb(); /* instantiating the process */

	if (p != NULL){ /* if the pcbFree list was not empty */
	
		/* initializing temp in order to then set p's stack pointer to the address of the top of RAM */
		temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
		ramtop = temp->rambase + temp->ramsize; /* initializing ramtop to the address of the top of RAM */

		/* initializing the Processor State that is a part of p */
		p->p_s.s_sp = ramtop; /* setting p's stack pointer to the address of the last RAM frame */
		p->p_s.s_pc = (memaddr) test; /* assigning the PC to the address of test */
		p->p_s.s_t9 = (memaddr) test; /* assigning the address of test to register t9 */
		p->p_s.s_status = ALLOFF | IEPON | PLTON | IMON; /* enabling interrupts, setting kernel-mode to on and enabling PLT */

		/* placing p into the Ready Queue and incrementing the Process Count */
		insertProcQ(&ReadyQueue, p); /* inserting p into the Ready Queue */
		procCnt++; /* incrementing the Process Count */

		/* calling the Scheduler function to begin executing a new process */
		switchProcess();
		return (0); 
	}
	PANIC(); /* invoking the PANIC() function to stop the system and print a warning message on terminal 0 */
	return (0);
}
