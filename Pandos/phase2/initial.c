/**************************************************************************** 
 *
 * This module adds several important components to the development of our
 * Pandos operating system. First off, this module contains the entry point
 * for our program (i.e., main()). main() will, among other tasks, initialize
 * the Phase 1 data structures (i.e., the ASL, free list of semaphore descriptors,
 * and the process queue that we will use to hold processes that are 
 * ready to be executed), initialize four words in the BIOS data page (i.e., one
 * word each for the general exception handler, a corresponding stack pointer, the 
 * TLB-Refill handler, and a corresponding stack pointer), and create one
 * process and call the scheduler on it. We also define the global variables
 * (and initialize them in main()) needed in Phase 2 of development, and we 
 * implement the general exception handler in this module.
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 *
 ****************************************************************************/

#include "../h/types.h"
#include "../h/const.h"
#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

/* function declarations */
extern void test(); /* function declaration for test(), which will be defined in the test file for this module */
extern void uTLB_RefillHandler(); /* function declaration for uTLB_RefillHandler(), which will be defined in exceptions.c */
HIDDEN void generalExceptionHandler(); /* function declaration for the internal function that is responsible for handling general exceptions */

/* declaring global variables */
pcb_PTR ReadyQueue; /* pointer to the tail of a queue of pcbs that are in the "ready" state */
pcb_PTR currentProc; /* pointer to the pcb that is in the "running" state */
int procCnt; /* integer indicating the number of started, but not yet terminated, processes */
int softBlockCnt; /* integer indicating the number of started, but not yet terminated, processes that're in the "blocked" state" */
semd_PTR deviceSemaphores[MAXDEVICECNT]; /* array of integer semaphores that correspond to each external (sub) device, plus one semd for the Pseudo-clock */

/* Internal function that is responsible for handling general exceptions. For interrupts, processing is passed along to 
 the device interrupt handler. For TLB exceptions, processing is passed along to the TLB exception handler, and for
 program traps, processing is passed along to the Program Trap exception handler. Finally, for exception code 8
 (SYCALL) events, processing is passed along to the SYSCALL exception handler. */
void generalExceptionHandler(){
	/* declaring local variables */
	state_t *oldState; /* the saved execption state for Processor 0 */
	int exceptionReason; /* the exception code */

	/* initializing local variables */
	oldState = (state_t *) BIOSDATAPAGE; /* initializing the saved exception state to the address of the start of the BIOS Data Page */
	exceptionReason = oldState->s_cause; 
	}

/* Function that represents the entry point of our program. It initializes the phase 1 data
 * structures (i.e., the ASL, free list of semaphore descriptors, and the process queue that we 
 * we will use to hold processes that are ready to be executed), initializes four words in the 
 * BIOS data page (i.e., one word each for the general exception handler, a corresponding stack 
 * pointer, the TLB-Refill handler, and a corresponding stack pointer), and creates one process
 * and calls the scheduler on it. The function also initializes the global variables for this
 * module. */
int main(){
	/* declaring local variables */
	pcb_PTR p; /* a pointer to the process that we will instantiate in this function */
	passupvector_t *procVec; /* a pointer to the Process 0 Pass Up Vector that we will initialize in this function */
	unsigned int ramtop; /* the last RAM frame */
 	devregarea_t *temp; /* devregarea that we can we use to determine the last RAM frame */

	/* initializing global variables */
	ReadyQueue = mkEmptyProcQ(); /* initializing the ReadyQueue's tail pointer to be NULL */
	currentProc = NULL; /* setting the pointer to the pcb that is in the "running" state to NULL */
	procCnt = 0; /* setting the number of started, but not yet terminated, processes to 0 */
	softBlockCnt = 0; /* setting the number of started, but not yet terminated, processes that're in the "blocked" state to 0 */

	int i;
	for (i = 0; i < 49; i++){
		/* initializing the device semaphores */
		deviceSemaphores[i] = 0;
	}

	/* initializing the free list of semaphore descriptors (along with the dummy nodes for the ASL) 
	and the pcbFree list */
	initPcbs(); /* initializing the pcbFree list */
	initASL(); /* initializing the semdFree list and the ASL's dummy nodes */

	/* initializing the Processor 0 Pass Up Vector */
	procVec = (passupvector_t *) PASSUPVECTOR; /* initializing procVec to be a pointer to the address of the Process 0 Pass Up Vector */
	procVec->tlb_refll_handler = (memaddr) uTLB_RefillHandler; /* initializing the address for handling TLB-Refill events */
	procVec->tlb_refll_stackPtr = (memaddr) PROC0STACKPTR; /* initializing the stack pointer for handling Nucleus TLB-Refill events */
	procVec->exception_handler = (memaddr) generalExceptionHandler; /* initializing the address for handling general exceptions */
	procVec->exception_stackPtr = (memaddr) PROC0STACKPTR; /* initializing the stack pointer for handling general exceptions */

	/* loading the system-wide interval timer with 100 (INITIALINTTIMER) milliseconds. */
	LDIT(INITIALINTTIMER); /* invoking the macro function that handles setting the system-wide interval timer with a given value */

	/* instantiating a single process so we can call the scheduler on it */
	p = allocPcb(); /* instantiating the process */
	/* initializing temp in order to then set p's stack pointer to the address of the top of RAM */
 	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
 	ramtop = temp->rambase + temp->ramsize; /* initializing ramptop to the address of the top of RAM */
 	p->p_s.s_sp = (memaddr) ramtop; /* setting p's stack pointer to the address of the last RAM frame for its stack */
	p->p_s.s_pc = (memaddr) test; /* assigning the PC to the address of test */
	p->p_s.s_t9 = (memaddr) test; /* assigning the address of test to register t9 */

	/* FINISH INSTANTIATION OF THE PROCESS */
	/* initializing the processor state that is part of the pcb */
	/* NOTE: setting the "previous" bits which will become current after LDST */
	p->p_s.s_entryHI=NULL;
	p->p_s.s_cause=NULL;
	p->p_s.s_status=NULL;
	for (int i=0; i<STATEREGNUM; i++){
		p->p_s.s_reg[i]=0;
	}
	/* process needs to have interrupts enabled */
	/* IEc (bit 0) = 1 */
	IEp (bit 2) = 1
	/* process needs to have kernel-mode on */
	/* KUc (bit 1) = 0 */
	KEp (bit 3) = 0
	/* process needs to have the processor Local Timer enabled */
	TE (bit 27) = 1 

	/* status reg = 0x0800000c */
	
	

	/* placing p into the Ready Queue and incrementing the Process Count */
	insertProcQ(&ReadyQueue, p); /* inserting p into the Ready Queue */
	procCnt++; /* incrementing the Process Count */

	/* calling the scheduler */
	scheduler();
	return (0);
}
