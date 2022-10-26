/**************************************************************************** 
 *
 * This module serves as the exception handler module for TLB-Refill events
 * and contains the implementation of the functions responsible for SYSCALL
 * exception handling. The module also addresses program traps and the Pass Up
 * or Die scenario. MENTION THE TIME SPENT HANDLING SYSCALLS (NON-BLOCKING 
 * SYSCALLS IN PARTICULAR, IS CHARGED TO CURRPROC)
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 *
 ****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../phase2/initial.c"

/* function declarations */
HIDDEN void blockCurr(int *sem, pcb_PTR proc);
HIDDEN void createProcess(state_PTR stateSYS, support_t *suppStruct);
HIDDEN void terminateProcess(pcb_PTR proc);
HIDDEN void waitOp(int *sem, pcb_PTR proc);
HIDDEN void signalOp(int *sem, pcb_PTR proc);
HIDDEN void waitForIO(int lineNum, int deviceNum, int readBool, pcb_PTR proc);
HIDDEN void getCPUTime(pcb_PTR proc);
HIDDEN void waitForPClock(pcb_PTR proc);
HIDDEN void getSupportData(pcb_PTR proc);

/* declaring global variables */
int sysNum; /* the number of the SYSCALL that we are addressing */
state_PTR savedExceptState; /* a pointer to the saved exception state */
cpu_t curr_tod; /* variable to hold the current TOD clock value */

/* Function that transfers data to the Current Process' pcb */
void updateCurrPCB(pcb_PTR proc){
	moveState(savedExceptState, &(proc->p_s)); /* copying the saved processor state into the Current Process' pcb by calling moveState() */
}

/* Function that handles the steps needed for blocking a process. The function copies the saved processor state into the Current
Process' pcb, updates the accumulated CPU time for the Current Process, and blocks the Current Process on the ASL. */
void blockCurr(int *sem, pcb_PTR proc){
	/* initializing curr_tod */
	STCK(curr_tod); /* initializing curr_tod with the current value on the Time of Day clock */

	updateCurrPCB(proc); /* calling the function that copies the saved processor state into the Current Process' pcb */
	proc->p_time = proc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
	insertBlocked(sem, proc); /* blocking the Current Process on the ASL */
	proc = NULL; /* setting currentProc to NULL because the old process is now blocked */ 
}

/* Function that handles SYS1 events. In other words, this internal function creates a new process. The function
allocates a new pcb and, if allocPcb() returns NULL (i.e., there are no more free pcbs), an error code of -1
is placed/returned in the caller's v0. Otherwise, the function initializes the fields of the new pcb appropriately
before placing/returning the value 0 in the caller's v0. Finally, the function copies the saved exception state from
the BIOS Data Page into the Current Process' processor state and then calls the function to load the Current State's
(updated) processor state into the CPU so it can continue executing. */
void createProcess(pcb_PTR proc, state_PTR stateSYS, support_t *suppStruct){
	/* declaring local variables */
	pcb_PTR newPcb; /* a pointer to the pcb that we will allocate and, if it is not initialized to NULL, place on the Ready Queue */

	/* initialing local variables */
	newPcb = allocPcb(); /* calling allocPcb() in order to allocate a newPcb that we can return to the caller */

	if (newPcb != NULL){ /* if the newly allocated pcb is not NULL, meaing there are enough resources to create a new process */
		/* initializing the fields of newPcb */
		newPcb->p_supportStruct = suppStruct; /* initializing newPcb's supportStruct field based on the parameter currently in register a2 */
		(newPcb->p_s) = *stateSYS; /* initializing newPcb's processor state to that which currently lies in a1 */
		newPcb->p_time = INITIALACCTIME; /* initializing newPcb's p_time field to 0, since it has not yet accumualted any CPU time */
		newPcb->p_semAdd = NULL; /* initializing the pointer to newPcb's semaphore, which is set to NULL because newPcb is not in the "blocked" state */
		insertChild(proc, newPcb); /* initializing newPcb's process tree fields by making it a child of the Current Process */
		insertProcQ(&ReadyQueue, newPcb); /* inserting newPcb onto the Ready Queue */
		savedExceptState->s_v0 = SUCCESSCONST; /* placing the value 0 in the caller's v0 because the allocation was completed successfully */
		procCnt++; /* incrementing the number of started, but not yet terminated, processes by one */
	}

	else{ /* there are no more free pcbs */
		savedExceptState->s_v0 = ERRORCONST; /* placing an error code of -1 in the caller's v0 */
	}

	updateCurrPCB(proc); /* moving the updated saved exception state from the BIOS Data Page into the Current Process' processor state */
	switchContext(proc); /* returning control to the Current Process by loading its (updated) processor state */
}

/* Function that handles a SYS2 events and the "Die" portion of "Pass Up or Die." In other words, this internal function terminates the
executing process and causes it to cease to exist. In addition, all progeny of this process are terminated as well. To accomplish
this latter task, we will utilize head recursion, removing the children of the parameter proc one at a time, starting proc's first
child and ending with proc's last child. The function also determines if proc is blocked on the ASL, in the Ready Queue or is
CurrentProc. Once it does so, it removes proc from the ASL/ReadyQueue or detaches it from its parent (in the case that proc = CurrentProc).
Finally, the function calls freePcb(proc) to officially destroy the process before decrementing the Process Count and calling the 
Scheduler so another process can begin executing. */
void terminateProcess(pcb_PTR proc){
	/* declaring local variables */
	int *procSem; /* a pointer to the semaphore associated with the Current Process */

	/* initializing local variables */
	procSem = proc->p_semAdd; /* initializing currProcSem to the Current Process' pointer to its semaphore */

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
		if (!(procSem >= &deviceSemaphores[FIRSTDEVINDEX] && procSem <= &deviceSemaphores[LASTDEVINDEX])){ /* if proc is not blocked on a device semaphore */
			(*(procSem))++; /* incrementing the val of sema4*/
		}
		softBlockCnt--; /* decrementing the number of started, but not yet terminated, processes that are in the "blocked" state */
	} 
	else{ /* proc is on the Ready Queue */
		outProcQ(&ReadyQueue, proc); /* removing proc from the Ready Queue */
	}
	freePcb(proc); /* returning proc onto the pcbFree list (and, therefore, destroying it) */
	procCnt--; /* decrementing the number of started, but not yet terminated, processes */
	currentProc = NULL; /* setting the Current Process to NULL, since it has been destroyed */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
}

/* Function that handles a SYS3 event. This is a (sometimes) blocking syscall.
Based on value of the semaphore, will either: 
- Block the Current Process & call the Scheduler so that the CPU can begin executing the next process
- Return to the Current Process & continue executing */
void waitOp(int *sem, pcb_PTR proc){
	(*sem)--; /* decrement the semaphore's value by 1 */
	if(*sem < SEMA4THRESH){ /* if value of semaphore is less than 0, means proc must be blocked. */
		blockCurr(sem, proc); /* block the Current Process on the ASL */
		switchProcess(); /* calling the Scheduler to begin executing the next process */
	}

	/* else, return to current proc */
	updateCurrPCB(proc); /* update the Current Process' processor state before resuming process' execution */
	switchContext(proc); /* returning control to the Current Process by loading its (updated) processor state */
}


/* Function that handles a SYS4 event.
This function essentially requests the Nucleus to perform a V op on a semaphore.
Using the physical address of the semaphore in the 'sem' pointer, which will then be V'ed.
If the value of the semaphore indicates no blocking processes, then return to the Current Process (to be resumed). */
void signalOp(int *sem, pcb_PTR proc){
	(*sem)++; /* increment the semaphore's value by 1 */
	if(*sem <= SEMA4THRESH){ /* if value of semaphore indicates a blocking process */ 
		pcb_PTR temp = removeBlocked(&sem); /* make semaphore not blocking, ie: make it not blocking on the ASL */
		insertProcQ(&ReadyQueue, temp); /* add process' PCB to the ReadyQueue */
	}

	/* returning to the Current Process */
	updateCurrPCB(proc); /* update the Current Process' processor state before resuming process' execution */
	switchContext(proc); /* returning control to the Current Process by loading its (updated) processor state */
}


/* Internal function that handles SYS5 events. The function handles requests for I/O. The primary tasks accomplished in the
function include locating the index of the semaphore associated with the device requesting I/O in deviceSemaphores[] and performing a 
P operation on that semaphore so that the Current Process is blocked on the ASL. Note that, as mentioned in the initial.c module,
the deviceSemaphores[] array is initialized so that terminal device semaphores are last and terminal device semaphores associated
with a read operation in the array come before those associated with a write operation. */
void waitForIO(int lineNum, int deviceNum, int readBool, pcb_PTR proc){
	/* Declaring local variables */
	int index; /* the index in deviceSemaphores associated with the device requesting I/O */
	
	/* initializing the index in deviceSemaphores associated with the device requesting I/O. Note that if the device is a terminal device,
	the next line initializes index to the semaphore associated with a read operation for that device */
	index = ((lineNum - OFFSET) * DEVPERINT) + deviceNum; 
	if (lineNum == LINE7 && readBool != TRUE){ /* if the device is a terminal device and the device is waiting for a write operation */
		index += DEVPERINT; /* adding 8 to index, since the semaphore associated with a read operation comes 8 indices before that associated with a write operation for a given device */
	}

	softBlockCnt++; /* incrementing the soft block count, since a new process has been placed in the "blocked" state */
	
	blockCurr(&deviceSemaphores[index], proc); /* block the Current Process on the ASL */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
}

/* Internal function that handles SYS6 events. The function places the accumulated processor time used by the requesting process 
in the caller's v0 register. Afterward, the function copies the saved exception state from the BIOS Data Page into the Current
Process' processor state and then calls the function to load the Current State's (updated) processor state into the CPU so it can continue
executing. */
void getCPUTime(pcb_PTR proc){
	/* initializing curr_tod */
	STCK(curr_tod); /* initializing curr_tod with the current value on the Time of Day clock */

	savedExceptState->s_v0 = proc->p_time + (curr_tod - start_tod); /* placing the accumulated processor time used by the requesting process in v0 */
	updateCurrPCB(proc); /* update the Current Process' processor state before resuming process' execution */
	switchContext(proc); /* returning control to the Current Process by loading its (updated) processor state */
}

/* Function that handles SYS7 events. This is always a blocking syscall, since the Pseudo-clock semaphore (which is located at
the last index of the deviceSemaphores array, identified by constant PCLOCKIDX) is a synchronization semaphore. 
SYS7 is used to transition the Current Process from the running state to a blocked state, and then calls the Scheduler 
so that the CPU can begin executing the next process. More specifically, this function performs a P (waitOp) on the
Nucleus Psuedo-clock semaphore, which is V'ed every INITIALINTTIMER (100) milliseconds by the Nucleus. */
void waitForPClock(pcb_PTR proc){
	(deviceSemaphores[PCLOCKIDX])--; /* decrement the semaphore's value by 1 */
	blockCurr(&deviceSemaphores[PCLOCKIDX], proc); /* should always block the Current Process on the ASL */
	softBlockCnt++; /* incrementing the number of started, but not yet terminated, processes that are in a "blocked" state */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
}


/* Function that handles SYS8 events. This returns a pointer to Current Process' support_t structure, to 
be loaded into the calling process' v0 register, which may be NULL if the Current Process' support_t structure
was not initialized when created. */
/* This provides Support Level access to relevant PCB fields while hiding the Level 3 (and Level 2) PCB fields */
void getSupportData(pcb_PTR proc){
	savedExceptState->s_v0 = (proc->p_supportStruct); /* place Current Process' supportStruct in v0 */
	updateCurrPCB(proc); /* update the Current Process' processor state before resuming process' execution */
	switchContext(proc); /* returning control to the Current Process (resume execution) */
}


/* Function that performs a standard Pass Up or Die operation using the provided index value. If the Current Process' p_supportStruct is
NULL, then the exception is handled as a SYS2; the Current Process and all its progeny are terminated. (This is the "die" portion of "Pass
Up or Die.") On the other hand, if the Current Process' p_supportStruct is not NULL, then the handling of the exception is "passed up." In
this case, the saved exception state ffrom the BIOS Data Page is copied to the correct sup_exceptState field of the Current Process, and 
a LDCXT is performed using the fields from the proper sup_exceptContext field of the Current Process. */
void passUpOrDie(pcb_PTR proc, int exceptionCode){
	if (proc->p_supportStruct != NULL){
		moveState(savedExceptState, &(proc->p_supportStruct->sup_exceptionState[exceptionCode])); /* copying the saved exception state from the BIOS Data Page to the correct sup_exceptState field of the Current Process */
		LDCXT(proc->p_supportStruct->sup_exceptContext[exceptionCode].c_stackPtr, proc->p_supportStruct->sup_exceptContext[exceptionCode].c_status,
		proc->p_supportStruct->sup_exceptContext[exceptionCode].c_pc); /* performing a LDCXT using the fields from the correct sup_exceptContext field of the Current Process */
	}
	/* the Current Process' p_support_struct is NULL, so we handle it as a SYS2: the Current Process and all its progeny are terminated */
	terminateProcess(proc); /* calling the termination function that "kills" the Current Process and all of its children */

}

/* Function that represents the entry point in this module when handling SYSCALL events. This function's tasks include, but are not
limited to, incrementing the value of the PC in the stored exception state (to avoid an infinite loop of SYSCALLs), checking to see if an
attempt was made to request a SYSCALL while the system was in user mode (if so, the function handles this case as it would a Program Trap
exception), and checking to see what SYSCALL number was requested so it can invoke an internal helper function to handle that specific
SYSCALL. If an invalid SYSCALL number was provided (i.e., the SYSCALL number requested was nine or above), we invoke the internal
function that performs a standard Pass Up or Die operation using the GENERALEXCEPT index value.  */
void sysTrapH(){
	/* initializing global variables, except for curr_tod, which will be initialized later */ 
	sysNum = currentProc->p_s.s_a0; /* initializing the number of the SYSCALL that we are addressing */
	savedExceptState = (state_PTR) BIOSDATAPAGE; /* initializing the saved exception state to the state stored at the start of the BIOS Data Page */

	savedExceptState->s_pc = savedExceptState->s_pc + PCINCREMENT; /* incrementing the value of the PC in the saved exception state by 4 */

	if (currentProc->p_s.s_status & USERPON != ALLOFF){ /* if the process was executing in user mode when the SYSCALL was requested */
		(((state_t *) BIOSDATAPAGE)->s_cause) = (((state_t *) BIOSDATAPAGE)->s_cause) & RESINSTRCODE; /* setting the Cause.ExcCode bits in the stored exception state to RI (10) */
		pgmTrapH();
	}

	/* enumerating the possible SYSCALL numbers and passing control to the appropriate internal function to handle the SYSCALL */
	switch (sysNum){
		case SYS1NUM: /* if the SYSCALL number is 1 */
			createProcess(currentProc, currentProc->p_s.s_a1, currentProc->p_s.s_a2); /* invoking the internal function that handles SYS1 events */
			/* a1 should contain the processor state associated with the SYSCALL */
			/* a2 should contain the (optional) support struct, which may be NULL */

		case SYS2NUM: /* if the SYSCALL number is 2 */
			terminateProcess(currentProc); /* invoking the internal function that handles SYS2 events */
		
		case SYS3NUM: /* if the SYSCALL number is 3 */
			waitOp(currentProc->p_s.s_a1, currentProc); /* invoking the internal function that handles SYS3 events */
			/* a1 should contain the addr of semaphore to be P'ed */
		
		case SYS4NUM: /* if the SYSCALL number is 4 */
			signalOp(currentProc->p_s.s_a1, currentProc); /* invoking the internal function that handles SYS4 events */
			/* a1 should contain the addr of semaphore to be V'ed */

		case SYS5NUM: /* if the SYSCALL number is 5 */
			waitForIO(currentProc->p_s.s_a1, currentProc->p_s.s_a2, currentProc->p_s.s_a3, currentProc); /* invoking the internal function that handles SYS5 events */
			/* a1 should contain the interrupt line number of the interrupt at the time of the SYSCALL */ 
			/* a2 should contain the device number associated with the specified interrupt line */
			/* a3 should contain TRUE or FALSE, indicating if waiting for a terminal read operation */

		case SYS6NUM: /* if the SYSCALL number is 6 */
			getCPUTime(currentProc); /* invoking the internal function that handles SYS6 events */
		
		case SYS7NUM: /* if the SYSCALL number is 7 */
			waitForPClock(currentProc); /* invoking the internal function that handles SYS 7 events */
		
		case SYS8NUM: /* if the SYSCALL number is 8 */
			getSupportData(currentProc); /* invoking the internal function that handles SYS 8 events */

		default: /* if the SYSCALL number was 9 or above */
			passUpOrDie(currentProc, GENERALEXCEPT); /* performing a standard Pass Up or Die operation using the GENERALEXCEPT index value */
	}
}

/* Function that handles TLB exceptions. The function invokes the internal helper function that performs a standard Pass Up or Die
operation using the PGFAULTEXCEPT index value. */
void tlbTrapH(){
	passUpOrDie(currentProc, PGFAULTEXCEPT); /* performing a standard Pass Up or Die operation using the PGFAULTEXCEPT index value */
}

/* Function that handles Program Trap exceptions. The function invokes the internal helper function that performs a standard Pass Up or Die
operation using the GENERALEXCEPT index value. */
void pgmTrapH(){
	passUpOrDie(currentProc, GENERALEXCEPT); /* performing a standard Pass Up or Die operation using the GENERALEXCEPT index value */
}

/* Function that handles TLB-Refill events. We will replace/overwrite the contents of this function once we begin implementing
Phase 3. */
void uTLB_RefillHandler(){
	setENTRYHI(KUSEG);
	setENTRYLO(KSEG0);
	TLBWR();
	LDST((state_PTR) BIOSDATAPAGE);
}
