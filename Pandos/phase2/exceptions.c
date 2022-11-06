/**************************************************************************** 
 *
 * This module serves as the exception handler module to define exception-handling functions that are called
 * by the General Exception Handler. Namely, this module defines a function for handling program trap events-- pgmTrapH(), 
 * for TLB trap events-- tlbTrapH(), and for SYSCALL exception events-- sysTrapH()...in addition to all of the 
 * specific helper functions for various SYSCALL numbers. This module is also responsible for handling
 * Pass Up or Die events.
 * 
 * While the General Exception Handler can directly call pgmTrapH(), tlbTrapH(), and sysTrapH(), 
 * most of the functions within this module pertain to SYSCALL exceptions, and thus the
 * sysTrapH() function is the most developed entry point to this module. It first performs a few
 * checks, such as ensuring the requesting process was not in user mode when the SYSCALL was made
 * and confirming the requested SYSCALL will be uniquely handled (only SYS1-8). It then
 * passes control to the appropriate internal SYSCALL handler function.
 * 
 * 
 * Note that for the purposes of this phase of development, the CPU time used by the 
 * Current Process to handle the SYSCALL request is charged to the Current Process.
 * This is true regardless of whether the Current Process continues executing after the SYSCALL
 * request is handled, because once we return control to the Current Process, the start_tod variable (the 
 * value that the process started executing at) is reset.
 * 
 * We decided on this timing policy as we believe charging such CPU
 * time to the requesting process is the most logical way to account for the CPU time
 * used in this module, as the Current Process, after all, was the process that
 * requested that part of its quantum (and CPU time) be spent handling the SYSCALL request.
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
#include "/usr/include/umps3/umps/libumps.h"

/* function declarations */
HIDDEN void blockCurr(int *sem);
HIDDEN void createProcess(state_PTR stateSYS, support_t *suppStruct);
HIDDEN void terminateProcess(pcb_PTR proc);
HIDDEN void waitOp(int *sem);
HIDDEN void signalOp(int *sem);
HIDDEN void waitForIO(int lineNum, int deviceNum, int readBool);
HIDDEN void getCPUTime();
HIDDEN void waitForPClock();
HIDDEN void getSupportData();

/* declaring variables that are global to this module */
int sysNum; /* the number of the SYSCALL that we are addressing */
cpu_t curr_tod; /* variable to hold the current TOD clock value */

/* Function that copies the saved exception located at the start of the BIOS Data Page to the Current Process' pcb so that
it contains the updated processor state after an exception (or interrupt) is handled */
void updateCurrPcb(){
	moveState(savedExceptState, &(currentProc->p_s)); /* invoking the function that copies the saved processor state into the Current Process' pcb by calling moveState() */	
}

/* Function that handles the steps needed for blocking a process. The function updates the accumulated CPU time 
for the Current Process, and blocks the Current Process on the ASL. */
void blockCurr(int *sem){
	STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
	currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
	insertBlocked(sem, currentProc); /* blocking the Current Process on the ASL */
	currentProc = NULL; /* setting currentProc to NULL because the old process is now blocked */ 
}

/* Function that handles SYS1 events. In other words, this internal function creates a new process. The function
allocates a new pcb and, if allocPcb() returns NULL (i.e., there are no more free pcbs), an error code of -1
is placed/returned in the caller's v0. Otherwise, the function initializes the fields of the new pcb appropriately
before placing/returning the value 0 in the caller's v0. Finally, it calls the function to load the Current State's
(updated) processor state into the CPU so it can continue executing. */
void createProcess(state_PTR stateSYS, support_t *suppStruct){
	/* declaring local variables */
	pcb_PTR newPcb; /* a pointer to the pcb that we will allocate and, if it is not initialized to NULL, place on the Ready Queue */

	/* initialing local variables */
	newPcb = allocPcb(); /* calling allocPcb() in order to allocate a newPcb that we can return to the caller */

	if (newPcb != NULL){ /* if the newly allocated pcb is not NULL, meaing there are enough resources to create a new process */
		/* initializing the fields of newPcb */
		moveState(stateSYS, &(newPcb->p_s)); /* initializing newPcb's processor state to that which currently lies in a1 */
		newPcb->p_supportStruct = suppStruct; /* initializing newPcb's supportStruct field based on the parameter currently in register a2 */
		newPcb->p_time = INITIALACCTIME; /* initializing newPcb's p_time field to 0, since it has not yet accumualted any CPU time */
		newPcb->p_semAdd = NULL; /* initializing the pointer to newPcb's semaphore, which is set to NULL because newPcb is not in the "blocked" state */
		insertChild(currentProc, newPcb); /* initializing newPcb's process tree fields by making it a child of the Current Process */
		insertProcQ(&ReadyQueue, newPcb); /* inserting newPcb onto the Ready Queue */
		currentProc->p_s.s_v0 = SUCCESSCONST; /* placing the value 0 in the caller's v0 because the allocation was completed successfully */
		procCnt++; /* incrementing the number of started, but not yet terminated, processes by one */
	}

	else{ /* there are no more free pcbs */
		currentProc->p_s.s_v0 = ERRORCONST; /* placing an error code of -1 in the caller's v0 */
	}

	STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
	currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
	switchContext(currentProc); /* returning control to the Current Process by loading its (updated) processor state */
	
}

/* Function that handles SYS2 events and the "Die" portion of "Pass Up or Die." In other words, this internal function terminates the
executing process and causes it to cease to exist. In addition, all progeny of this process are terminated as well. To accomplish
this latter task, we will utilize head recursion, removing the children of the parameter proc one at a time, starting with proc's first
child and ending with proc's last child. The function also determines if proc is blocked on the ASL, in the Ready Queue or is
CurrentProc. Once it does so, it removes proc from the ASL/ReadyQueue or detaches it from its parent (in the case that proc = CurrentProc).
Finally, the function calls freePcb(proc) to officially destroy the process before decrementing the Process Count. The Scheduler is then invoked
in the entry point for this module, sysTrapH() in order to refrain from calling the Scheduler in this function many times in the event
that proc has at least one child. */
void terminateProcess(pcb_PTR proc){ 
	/* declaring local variables */
	int *procSem; /* a pointer to the semaphore associated with the Current Process */

	/* initializing local variables */
	procSem = proc->p_semAdd; /* initializing procSem to the process' pointer to its semaphore */

	/* terminating all progeny of the Current Process by utilizing head recursion */
	while (!(emptyChild(proc))){ /* while the process that will be terminated still has children */
		terminateProcess(removeChild(proc)); /* making the recursive call to continue removing the requesting process' children from it */
	}

	/* The requesting process now has no children. We now need to determine if the requesting process is the running process, sitting on
	the Ready Queue or is blocked. */
	
	if (proc == currentProc){ /* if proc is the running process */
		outChild(proc); /* detaching the Current Process from its parent */
	}
	else if (procSem != NULL){ /* if proc is blocked (and is on the ASL) */
		outBlocked(proc); /* removing proc from the ASL */
		if (!(procSem >= &deviceSemaphores[FIRSTDEVINDEX] && procSem <= &deviceSemaphores[PCLOCKIDX])){ /* if proc is not blocked on a device semaphore */
			(*(procSem))++; /* incrementing the val of sema4*/
		}
		else{
			softBlockCnt--; /* decrementing the number of started, but not yet terminated, processes that are in the "blocked" state */
		}
	} 
	else{ /* proc is on the Ready Queue */
		outProcQ(&ReadyQueue, proc); /* removing proc from the Ready Queue */
	}
	freePcb(proc); /* returning proc onto the pcbFree list (and, therefore, destroying it) */
	procCnt--; /* decrementing the number of started, but not yet terminated, processes */
	proc = NULL; /* setting the process pointer to NULL, since it has been destroyed */
}

/* Function that handles a SYS3 event. This is a (sometimes) blocking syscall.
Based on value of the semaphore, will either: 
- Block the Current Process & call the Scheduler so that the CPU can begin executing the next process
- Return to the Current Process & continue executing */
void waitOp(int *sem){
	(*sem)--; /* decrement the semaphore's value by 1 */
	if(*sem < SEMA4THRESH){ /* if value of semaphore is less than 0, means process must be blocked. */
		blockCurr(sem); /* block the Current Process on the ASL */
		switchProcess(); /* calling the Scheduler to begin executing the next process */
	}

	/* else, return to Current Process */
	STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
	currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
	switchContext(currentProc); /* returning control to the Current Process by loading its (updated) processor state */
}

/* Function that handles a SYS4 event.
This function essentially requests the Nucleus to perform a V op on a semaphore.
Using the physical address of the semaphore in the 'sem' pointer, which will then be V'ed.
If the value of the semaphore indicates no blocking processes, then return to the Current Process (to be resumed). */
void signalOp(int *sem){
	(*sem)++; /* increment the semaphore's value by 1 */
	if(*sem <= SEMA4THRESH){ /* if value of semaphore indicates a blocking process */ 
		pcb_PTR temp = removeBlocked(sem); /* make semaphore not blocking, ie: make it not blocking on the ASL */
		insertProcQ(&ReadyQueue, temp); /* add process' PCB to the ReadyQueue */
	}

	/* returning to the Current Process */
	STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
	currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
	switchContext(currentProc); /* returning control to the Current Process by loading its (updated) processor state */
}

/* Internal function that handles SYS5 events. The function handles requests for I/O. The primary tasks accomplished in the
function include locating the index of the semaphore associated with the device requesting I/O in deviceSemaphores[] and performing a 
P operation on that semaphore so that the Current Process is blocked on the ASL. Note that, as mentioned in the initial.c module,
the deviceSemaphores[] array is initialized so that terminal device semaphores are last and terminal device semaphores associated
with a read operation in the array come before those associated with a write operation. */
void waitForIO(int lineNum, int deviceNum, int readBool){
	/* Declaring local variables */
	int index; /* the index in deviceSemaphores associated with the device requesting I/O */
	
	/* initializing the index in deviceSemaphores associated with the device requesting I/O. Note that if the device is a terminal device,
	the next line initializes index to the semaphore associated with a read operation for that device */
	index = ((lineNum - OFFSET) * DEVPERINT) + deviceNum; 
	if (lineNum == LINE7 && readBool != TRUE){ /* if the device is a terminal device and the device is waiting for a write operation */
		index += DEVPERINT; /* adding 8 to index, since the semaphore associated with a read operation comes 8 indices before that associated with a write operation for a given device */
	}

	softBlockCnt++; /* incrementing the soft block count, since a new process has been placed in the "blocked" state */
	(deviceSemaphores[index])--; /* decrement the semaphore's value by 1 */
	blockCurr(&deviceSemaphores[index]); /* block the Current Process on the ASL */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
}

/* Internal function that handles SYS6 events. The function places the accumulated processor time used by the requesting process 
in the caller's v0 register. Afterward, the function calls the function to load the Current State's (updated) processor 
state into the CPU so it can continue executing. */
void getCPUTime(){
	STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
	currentProc->p_s.s_v0 = currentProc->p_time + (curr_tod - start_tod); /* placing the accumulated processor time used by the requesting process in v0 */
	currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
	switchContext(currentProc); /* returning control to the Current Process by loading its (updated) processor state */
}

/* Function that handles SYS7 events. This is always a blocking syscall, since the Pseudo-clock semaphore (which is located at
the last index of the deviceSemaphores array, identified by constant PCLOCKIDX) is a synchronization semaphore. 
SYS7 is used to transition the Current Process from the running state to a blocked state, and then calls the Scheduler 
so that the CPU can begin executing the next process. More specifically, this function performs a P (waitOp) on the
Nucleus Psuedo-clock semaphore, which is V'ed every INITIALINTTIMER (100 milliseconds) by the Nucleus. */
void waitForPClock(){
	(deviceSemaphores[PCLOCKIDX])--; /* decrement the semaphore's value by 1 */
	blockCurr(&deviceSemaphores[PCLOCKIDX]); /* should always block the Current Process on the ASL */
	softBlockCnt++; /* incrementing the number of started, but not yet terminated, processes that are in a "blocked" state */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
}

/* Function that handles SYS8 events. This returns a pointer to Current Process' support_t structure by loading it 
into the calling process' v0 register. This may be NULL if the Current Process' support_t structure
was not initialized during process creation. Once it places the pointer to the Current Process' support_t structure in v0,
it returns control back to the Current Process so that it can continue executing (after charging the
Current Process with the CPU time needed to handle the SYSCALL request). */
void getSupportData(){
	currentProc->p_s.s_v0 = (int)(currentProc->p_supportStruct); /* place Current Process' supportStruct in v0 */
	STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
	currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
	switchContext(currentProc); /* returning control to the Current Process (resume execution) */
}

/* Function that performs a standard Pass Up or Die operation using the provided index value. If the Current Process' p_supportStruct is
NULL, then the exception is handled as a SYS2; the Current Process and all its progeny are terminated. (This is the "die" portion of "Pass
Up or Die.") On the other hand, if the Current Process' p_supportStruct is not NULL, then the handling of the exception is "passed up." In
this case, the saved exception state from the BIOS Data Page is copied to the correct sup_exceptState field of the Current Process, and 
a LDCXT is performed using the fields from the proper sup_exceptContext field of the Current Process. */
void passUpOrDie(int exceptionCode){ 
	if (currentProc->p_supportStruct != NULL){
		moveState(savedExceptState, &(currentProc->p_supportStruct->sup_exceptState[exceptionCode])); /* copying the saved exception state from the BIOS Data Page directly to the correct sup_exceptState field of the Current Process */
		STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
		currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
		LDCXT(currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_stackPtr, currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_status,
		currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_pc); /* performing a LDCXT using the fields from the correct sup_exceptContext field of the Current Process */
	}
	else{
		/* the Current Process' p_support_struct is NULL, so we handle it as a SYS2: the Current Process and all its progeny are terminated */
		terminateProcess(currentProc); /* calling the termination function that "kills" the Current Process and all of its children */
		currentProc = NULL; /* setting the Current Process pointer to NULL */
		switchProcess(); /* calling the Scheduler to begin executing the next process */
	}
}

/* Function that represents the entry point into this module when handling SYSCALL events. This function's tasks include, but are not
limited to, incrementing the value of the PC in the stored exception state (to avoid an infinite loop of SYSCALLs), checking to see if an
attempt was made to request a SYSCALL while the system was in user mode (if so, the function handles this case as it would a Program Trap
exception), and checking to see what SYSCALL number was requested so it can invoke an internal helper function to handle that specific
SYSCALL. If an invalid SYSCALL number was provided (i.e., the SYSCALL number requested was nine or above), we invoke the internal
function that performs a standard Pass Up or Die operation using the GENERALEXCEPT index value.  */
void sysTrapH(){
	/* initializing variables that are global to this module, as well as savedExceptState */ 
	savedExceptState = (state_PTR) BIOSDATAPAGE; /* initializing the saved exception state to the state stored at the start of the BIOS Data Page */
	sysNum = savedExceptState->s_a0; /* initializing the SYSCALL number variable to the correct number for the exception */

	savedExceptState->s_pc = savedExceptState->s_pc + WORDLEN;

	/* Perform checks to make sure we want to proceed with handling the SYSCALL (as opposed to pgmTrapH) */
	if (((savedExceptState->s_status) & USERPON) != ALLOFF){ /* if the process was executing in user mode when the SYSCALL was requested */
		savedExceptState->s_cause = (savedExceptState->s_cause) & RESINSTRCODE; /* setting the Cause.ExcCode bits in the stored exception state to RI (10) */
		pgmTrapH(); /* invoking the internal function that handles program trap events */
	}
	
	if ((sysNum<SYS1NUM) || (sysNum > SYS8NUM)){ /* check if the SYSCALL number was not 1-8 (we'll punt & avoid uniquely handling it) */
		pgmTrapH(); /* invoking the internal function that handles program trap events */
	} 
	
	updateCurrPcb(currentProc); /* copying the saved processor state into the Current Process' pcb  */
	
	/* enumerating the sysNum values (1-8) and passing control to the respective function to handle it */
	switch (sysNum){ 
		case SYS1NUM: /* if the sysNum indicates a SYS1 event */
			/* a1 should contain the processor state associated with the SYSCALL */
			/* a2 should contain the (optional) support struct, which may be NULL */
			createProcess((state_PTR) (currentProc->p_s.s_a1), (support_t *) (currentProc->p_s.s_a2)); /* invoking the internal function that handles SYS1 events */

		case SYS2NUM: /* if the sysNum indicates a SYS2 event */
			terminateProcess(currentProc); /* invoking the internal function that handles SYS2 events */
			currentProc = NULL; /* setting the Current Process pointer to NULL */
			switchProcess(); /* calling the Scheduler to begin executing the next process */
		
		case SYS3NUM: /* if the sysNum indicates a SYS3 event */
			/* a1 should contain the addr of semaphore to be P'ed */
			waitOp((int *) (currentProc->p_s.s_a1)); /* invoking the internal function that handles SYS3 events */
		
		case SYS4NUM: /* if the sysNum indicates a SYS4 event */
			/* a1 should contain the addr of semaphore to be V'ed */
			signalOp((int *) (currentProc->p_s.s_a1)); /* invoking the internal function that handles SYS4 events */

		case SYS5NUM: /* if the sysNum indicates a SYS5 event */
			/* a1 should contain the interrupt line number of the interrupt at the time of the SYSCALL */ 
			/* a2 should contain the device number associated with the specified interrupt line */
			/* a3 should contain TRUE or FALSE, indicating if waiting for a terminal read operation */
			waitForIO(currentProc->p_s.s_a1, currentProc->p_s.s_a2, currentProc->p_s.s_a3); /* invoking the internal function that handles SYS5 events */

		case SYS6NUM: /* if the sysNum indicates a SYS6 event */
			getCPUTime(); /* invoking the internal function that handles SYS6 events */
		
		case SYS7NUM: /* if the sysNum indicates a SYS7 event */
			waitForPClock(); /* invoking the internal function that handles SYS 7 events */
		
		case SYS8NUM: /* if the sysNum indicates a SYS8 event */
			getSupportData(); /* invoking the internal function that handles SYS 8 events */
		
	}
}

/* Function that handles TLB exceptions. The function invokes the internal helper function that performs a standard Pass Up or Die
operation using the PGFAULTEXCEPT index value. */
void tlbTrapH(){
	passUpOrDie(PGFAULTEXCEPT); /* performing a standard Pass Up or Die operation using the PGFAULTEXCEPT index value */
}

/* Function that handles Program Trap exceptions. The function invokes the internal helper function that performs a standard Pass Up or Die
operation using the GENERALEXCEPT index value. */
void pgmTrapH(){
	passUpOrDie(GENERALEXCEPT); /* performing a standard Pass Up or Die operation using the GENERALEXCEPT index value */
}
