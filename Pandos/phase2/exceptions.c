/**************************************************************************** 
 *
 * This module serves as the exception handler module for TLB-Refill events
 * and contains the implementation of the functions responsible for SYSCALL
 * exception handling. The module also addresses program traps and the Pass Up
 * or Die scenario.
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
HIDDEN void createProcess();
HIDDEN void terminateProcess();
HIDDEN void blockCurr(int *sem);
HIDDEN void waitForIO();
HIDDEN void getCPUTime();

/* declaring global variables */
state_PTR stateSYS; /* the processor state associated with the SYSCALL */
int currSYS; /* the number of the SYSCALL that we are addressing */
state_PTR savedExceptState; /* a pointer to the saved exception state */
cpu_t curr_tod; /* the current value on the Time of Day clock */

/* Function that handles SYS1 events. In other words, this internal function creates a new process. The function
allocates a new pcb and, if allocPcb() returns NULL (i.e., there are no more free pcbs), an error code of -1
is placed/returned in the caller's v0. Otherwise, the function initializes the fields of the new pcb appropriately
before placing/returning the value 0 in the caller's v0. Finally, the function copies the saved exception state from
the BIOS Data Page into the Current Process' processor state and then calls the function to load the Current State's
(updated) processor state into the CPU so it can continue executing. */
void createProcess(){
	/* declaring local variables */
	pcb_PTR newPcb; /* a pointer to the pcb that we will allocate and, if it is not initialized to NULL, place on the Ready Queue */

	/* initialing local variables */
	newPcb = allocPcb(); /* calling allocPcb() in order to allocate a newPcb that we can return to the caller */

	if (newPcb != NULL){ /* if the newly allocated pcb is not NULL, meaing there are enough resources to create a new process */

		/* initializing the fields of newPcb */
		newPcb->p_supportStruct = stateSYS->s_a2; /* initializing newPcb's supportStruct field based on the parameter currently in register a2 */
		(newPcb->p_s) = *stateSYS; /* initializing newPcb's processor state to that which currently lies in a1 */
		newPcb->p_time = INITIALACCTIME; /* initializing newPcb's p_time field to 0, since it has not yet accumualted any CPU time */
		newPcb->p_semAdd = NULL; /* initializing the pointer to newPcb's semaphore, which is set to NULL because newPcb is not in the "blocked" state */
		insertChild(currentProc, newPcb); /* initializing newPcb's process tree fields by making it a child of the Current Process */
		insertProcQ(&ReadyQueue, newPcb); /* inserting newPcb onto the Ready Queue */
		savedExceptState->s_v0 = SUCCESSCONST; /* placing the value 0 in the caller's v0 because the allocation was completed successfully */

		procCnt++; /* incrementing the number of started, but not yet terminated, processes by one */
	}

	else{ /* there are no more free pcbs */
		savedExceptState->s_v0 = ERRORCONST; /* placing an error code of -1 in the caller's v0 */
	}

	moveState((state_PTR) BIOSDATAPAGE, &(currentProc->p_s)); /* moving the updated saved exception state from the BIOS Data Page into the Current Process' processor state */
	switchContext(currentProc); /* returning control to the Current Process by loading its (updated) processor state */
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
	pcb_PTR terminateProc; /* a pointer to the pcb that will be terminated */

	/* initializing local variables */
	procSem = proc->p_semAdd; /* initializing currProcSem to the Current Process' pointer to its semaphore */
	terminateProc = proc; /* initializing the process we want to terminate to curProc */

	/* terminating all progeny of the Current Process by utilizing head recursion */
	while (!(emptyChild(proc))){ /* while the process that will be terminated still has children */
		terminateProcess(removeChild(proc)); /* making the recursive call to continue removing the requesting process' children from it */
	}

	/* The requesting process now has no children. We now need to determine if the requesting process is the running process, sitting on
	the Ready Queue or is blocked. */
	
	if (proc == currentProc){ /* if proc is the running process */
		outChild(proc); /* detaching the Current Process from its parent */
	}
	else if (proc->p_semAdd != NULL){ /* if proc is blocked (and is on the ASL) */
		outBlocked(proc); /* removing proc from the ASL */
		if (!(procSem >= &deviceSemaphores[FIRSTDEVINDEX] && procSem <= &deviceSemaphores[LASTDEVINDEX])){ /* if proc is not blocked on a device semaphore */
			/* INCREMENT VALUE OF THE SEMA4 */
		}
		softBlockCnt--; /* decrementing the number of started, but not yet terminated, processes that are in the "blocked" state */
	} 
	else{ /* proc is on the Ready Queue */
		outProcQ(&ReadyQueue, proc); /* removing proc from the Ready Queue */
	}
	
	freePcb(proc); /* returning proc onto the pcbFree list (and, therefore, destroying it) */
	procCnt--; /* decrementing the number of started, but not yet terminated, processes */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
}

/* Internal function that handles SYS5 events. The function handles requests for I/O. The primary tasks accomplished in the
function include locating the index of the semaphore associated with the device requesting I/O in deviceSemaphores[] and performing a 
P operation on that semaphore so that the Current Process is blocked on the ASL. Note that, as mentioned in the initial.c module,
the deviceSemaphores[] array is initialized so that terminal device semaphores are last and terminal device semaphores associated
with a read operation in the array come before those associated with a write operation. */
void waitForIO(){
	/* Declaring local variables */
	int lineNum; /* the interupt line number */
	int deviceNum; /* the device number associated with the specified interrupt line */
	int index; /* the index in deviceSemaphores associated with the device requesting I/O */
	int readBool; /* a TRUE or FALSE variable signifying whether the device is waiting for a terminal read operation */
	
	/* initializing local variables */
	lineNum = savedExceptState->s_a1; /* initializing the line number of the interrupt to the contents of a1 at the time of the SYSCALL */
	deviceNum = savedExceptState->s_a2; /* initializing the device number */
	readBool = savedExceptState->s_a3; /* initializing the variable for whether or not the device is waiting for a terminal read operation */
	/* initializing the index in deviceSemaphores associated with the device requesting I/O. Note that if the device is a terminal device,
	the next line initializes index to the semaphore associated with a read operation for that device */
	index = ((lineNum - OFFSET) * DEVPERINT) + deviceNum; 
	if (lineNum == 7 && readBool != TRUE){ /* if the device is a terminal device and the device is waiting for a write operation */
		index += DEVPERINT; /* adding 8 to index, since the semaphore associated with a read operation comes 8 indices before that associated with a write operation for a given device */
	}

	/* CALL HELPER FUNCTION FOR SYSCALL 3 FOR WAITING--SHOULD BE THE WAIT() FUNCTION FROM CLASS */
	softBlockCnt++; /* incrementing the soft block count, since a new process has been placed in the "blocked" state */
	blockCurr(deviceSemaphores[index]); /* WE MAY NOT NEED THIS LINE, DEPENDING ON WHAT THE WAIT() FUNCTION LOOK LIKE */
}

/* Internal function that handles SYS6 events. The function places the accumulated processor time used by the requesting process 
in the caller's v0 register. Afterward, the function copies the saved exception state from the BIOS Data Page into the Current
Process' processor state and then calls the function to load the Current State's (updated) processor state into the CPU so it can continue
executing. */
void getCPUTime(){
	savedExceptState->s_v0 = currentProc->p_time + (curr_tod - start_tod); /* placing the accumulated processor time used by the requesting process in v0 */
	moveState((state_PTR) BIOSDATAPAGE, &(currentProc->p_s)); /* moving the updated saved exception state from the BIOS Data Page into the Current Process' processor state */
	switchContext(currentProc); /* returning control to the Current Process by loading its (updated) processor state */
}

/* Function that handles the steps needed for blocking a process. The function copies the saved processor state into the Current
Process' pcb, updates the accumulated CPU time for the Current Process, blocks the Current Process and calls the Scheduler so that the
CPU can begin executing the next process. */
void blockCurr(int *sem){
	moveState(savedExceptState, &(currentProc->p_s)); /* copying the saved processor state into the Current Process' pcb by calling moveState() */
	currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated CPU time for the Current Process */
	insertBlocked(sem, currentProc); /* blocking the Current Process on the ASL */
	currentProc = NULL; /* setting currentProc to NULL because the old process is now blocked */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
}

/* Function that performs a standard Pass Up or Die operation using the provided index value. If the Current Process' p_supportStruct is
NULL, then the exception is handled as a SYS2; the Current Process and all its progeny are terminated. (This is the "die" portion of "Pass
Up or Die.") On the other hand, if the Current Process' p_supportStruct is not NULL, then the handling of the exception is "passed up." In
this case, the saved exception state ffrom the BIOS Data Page is copied to the correct sup_exceptState field of the Current Process, and 
a LDCXT is performed using the fields from the proper sup_exceptContext field of the Current Process. */
void passUpOrDie(int exceptionCode){
	if (currentProc->p_supportStruct != NULL){
		moveState((state_PTR) BIOSDATAPAGE, &(currentProc->p_supportStruct->sup_exceptionState[exceptionCode])); /* copying the saved exception state from the BIOS Data Page to the correct sup_exceptState field of the Current Process */
		LDCXT(currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_stackPtr, currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_status,
		currentProc->p_supportStruct->sup_exceptContext[exceptionCode].c_pc); /* performing a LDCXT using the fields from the correct sup_exceptContext field of the Current Process */
	}
	/* the Current Process' p_support_struct is NULL, so we handle it as a SYS2: the Current Process and all its progeny are terminated */
	terminateProcess(currentProc); /* calling the termination function that "kills" the Current Process and all of its children */

}

/* Function that represents the entry point in this module when handling SYSCALL events. This function's tasks include, but are not
limited to, incrementing the value of the PC in the stored exception state (to avoid an infinite loop of SYSCALLs), checking to see if an
attempt was made to request a SYSCALL while the system was in user mode (if so, the function handles this case as it would a Program Trap
exception), and checking to see what SYSCALL number was requested so it can invoke an internal helper function to handle that specific
SYSCALL. If an invalid SYSCALL number was provided (i.e., the SYSCALL number requested was nine or above), we invoke the internal
function that performs a standard Pass Up or Die operation using the GENERALEXCEPT index value.  */
void sysTrapH(){

	/* initializing global variables */ 
	stateSYS = currentProc->p_s.s_a1; /* initializing the processor state associated with the SYSCALL to excState */
	currSYS = currentProc->p_s.s_a0; /* initializing the number of the SYSCALL that we are addressing */
	savedExceptState = (state_PTR) BIOSDATAPAGE; /* initialzing the saved exception state to the state stored at the start of the BIOS Data Page */
	STCK(curr_tod); /* initializing curr_tod with the current value on the Time of Day clock */

	savedExceptState->s_pc = savedExceptState->s_pc + PCINCREMENT; /* incrementing the value of the PC in the saved exception state by 4 */

	if (currentProc->p_s.s_status & USERPON != ALLOFF){ /* if the process was executing in user mode when the SYSCALL was requested */
		(((state_t *) BIOSDATAPAGE)->s_cause) = (((state_t *) BIOSDATAPAGE)->s_cause) & RESINSTRCODE; /* setting the Cause.ExcCode bits in the stored exception state to RI (10) */
		pgmTrapH();
	}

	/* enumerating the possible SYSCALL numbers and passing control to the appropriate internal function to handle the SYSCALL */
	switch (currSYS){
		
		case 1: /* if the SYSCALL number is 1 */
			createProcess(); /* invoking the internal function that handles SYS1 events */

		case 2: /* if the SYSCALL number is 2 */
			terminateProcess(currentProc); /* invoking the internal function that handles SYS2 events */

		case 5: /* if the SYSCALL number is 5 */
			waitForIO(); /* invoking the internal function that handles SYS5 events */

		case 6: /* if the SYSCALL number is 6 */
			getCPUTime(); /* invoking the internal function that handles SYS6 events */

		default: /* if the SYSCALL number was 9 or above */
			passUpOrDie(GENERALEXCEPT); /* performing a standard Pass Up or Die operation using the GENERALEXCEPT index value */
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

/* Function that handles TLB-Refill events. We will replace/overwrite the contents of this function once we begin implementing
Phase 3. */
void uTLB_RefillHandler(){
	setENTRYHI(KUSEG);
	setENTRYLO(KSEG0);
	TLBWR();
	LDST((state_PTR) BIOSDATAPAGE);
}

