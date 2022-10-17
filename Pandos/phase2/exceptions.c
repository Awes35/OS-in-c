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
cpu_t *curr_tod; /* A pointer to the current value of the Time of Day clock */
state_PTR stateSYS; /* the processor state associated with the syscall */
int currSYS; /* the number of the syscall that we are addressing */
state_PTR savedExceptState; /* a pointer to the saved exception state */

/* Function that handles SYS 1. In other words, this internal function creates a new process. The function
allocates a new pcb and, if allocPcb() returns NULL (i.e., there are no more free pcbs), an error code of -1
is placed/returned in the caller's v0. Otherwise, the function initializes the fields of the new pcb appropriately
before placing/returning the value 0 in the caller's v0 */
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
}

/* Function that handles SYS 2. In other words, this internal function terminates the executing process and causes it to cease to 
exist. In addition, all progeny of this process are terminated as well. To accomplish this latter task, we will utilize head
recursion and invoke removeChild(). */
void terminateProcess(){
	/* declaring local variables */
	int *currProcSem; /* a pointer to the semaphore associated with the Current Process */

	/* initializing local variables */
	currProcSem = currentProc->p_semAdd; /* initializing currProcSem to the Current Process' pointer to its semaphore */

	/* terminating all progeny of the Current Process by utilizing head recursion */
	while (removeChild(currentProc) != NULL){ /* while there are progeny of the Current Process */
		terminateProcess();
	}

	if (outBlocked(currentProc) != NULL){ /* if the terminated process is blocked on a semaphore */
		softBlockCnt--; /* decrementing the number of started, but not yet terminated, processes that are in the "blocked" state */
		/* INCREMENT THE VALUE OF THE SEMAPHORE */
	}
	
	/* destoying the executing process and causing it to cease to exist */
	freePcb(currentProc); /* returning currentProc onto the pcbFree list */
	procCnt--; /* decrementing the number of started, but not yet terminated, processes */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
	/* ADD BULLET POINTS FROM 3.9 IN GREEN BOOK HERE */
}

/* Function that handles SYS 5*/
void waitForIO(){
	/* Declaring local variables */
	int lineNum; /* the interupt line number */
	int deviceNum; /* the device number associated with the specified interrupt line */
	int index; /* the index in deviceSemaphores associated with the device requesting I/O */
	
	/* initializing local variables */
	lineNum = savedExceptState->s_a1; /* initializing the line number of the interrupt to the contents of a1 at the time of the syscall */
	deviceNum = savedExceptState->s_a2; /* initializing the device number */
	index = ((lineNum - OFFSET) * DEVPERINT) + deviceNum; /* initializing the index in deviceSemaphores associated with the device requesting I/O */
	/* IS THAT LAST LINE CORRECT...EACH TERMINAL DEVICE HAS 2 SEMAPHORES ASSOCIATED WITH IT AND I DIDN'T ACCOUNT FOR THAT */

	/* CALL FUNCTION FOR SYSCALL 3 FOR WAITING */
	softBlockCnt++; /* incrementing the soft block count, since a new process has been placed in the "blocked" state */
	/* PLACE SOMETHING IN CALLER'S V0 */
	blockCurr(deviceSemaphores[index]);
}

/* Function that handles SYS 6 */
void getCPUTime(){
	savedExceptState->s_v0 = currentProc->p_time + (&curr_tod - &start_tod);
}

/* Function that handles the steps needed for blocking a process. The function copies the saved processor state into the Current
Process' pcb, updates the accumulated CPU time for the Current Process, blocks the Current Process and calls the Scheduler so that the
CPU can begin executing the next process */
void blockCurr(int *sem){
	moveState(savedExceptState, &(currentProc->p_s)); /* copying the saved processor state into the Current Process' pcb by calling moveState() */
	currentProc->p_time = currentProc->p_time + (&curr_tod - &start_tod); /* updating the accumulated CPU time for the Current Process */
	insertBlocked(sem, currentProc); /* blocking the Current Process on the ASL */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
}

/* Function that represents the entry point in this module when handling SYSCALL events */
void sysTrapH(){

	/* initializing global variables */
	curr_tod = (cpu_t *) TODLOADDR; 
	stateSYS = currentProc->p_s.s_a1; /* initializing the processor state associated with the syscall to excState */
	currSYS = currentProc->p_s.s_a0; /* initializing the number of the syscall that we are addressing */
	savedExceptState = (state_PTR) BIOSDATAPAGE; /* initialzing the saved exception state to the state stored at the start of the BIOS Data Page */

	savedExceptState->s_pc = savedExceptState->s_pc + PCINCREMENT; /* incrementing the value of the PC in the saved exception state by 4 */

	if (currentProc->p_s.s_status & USERPON != ALLOFF){ /* if the process was executing in user mode when the syscall was requested */
		(((state_t *) BIOSDATAPAGE)->s_cause) = (((state_t *) BIOSDATAPAGE)->s_cause) & RESINSTRCODE; /* setting the Cause.ExcCode bits in the stored exception state to RI (10) */
		pgmTrapH();
	}

	/* enumerating the possible syscall numbers and passing control to the appropriate internal function to handle the syscall */
	switch (currSYS){
		
		case 1: /* if the syscall number is 1 */
			createProcess();

		case 2: /* if the syscall number is 2 */
			terminateProcess();

		case 5: /* if the syscall number is 5 */
			waitForIO();

		case 6: /* if the syscall number is 6 */
			getCPUTime();
	}
}

/* Function that handles TLB exceptions */
void tlbTrapH(){
	setENTRYHI(KUSEG);
	setENTRYLO(KSEG0);
	TLBWR();
	LDST((state_PTR) BIOSDATAPAGE);
}

