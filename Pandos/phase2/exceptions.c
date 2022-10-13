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

/* declaring global variables */
state_PTR stateSYS; /* the processor state associated with the syscall */
int currSYS; /* the number of the syscall that we are addressing */
cpu_t curr_tod; /* the current value on the time of day clock */
cpu_t start_tod; /* the value on the time of day clock that the given process began executing at */

/* Function that handles SYS 1. In other words, this internal function creates a new process. The function
allocates a new pcb and, if allocPcb() returns NULL (i.e., there are no more free pcbs), an error code of -1
is placed/returned in the caller's v0. Otherwise, the function initializes the fields of the new pcb appropriately
before placing/returning the value 0 in the caller's v0 */
void createProcess(){
	/* declaring local variables */
	pcb_PTR newPcb; /* a pointer to the pcb that we will allocate and, if it is not initialized to NULL, place on the Ready Queue */

	newPcb = allocPcb(); /* calling allocPcb() in order to allocate a newPcb that we can return to the caller */
	if (newPcb == NULL){ /* if there are no more free pcbs */
		stateSYS->s_v0 = ERRORCONST; /* placing an error code of -1 in the caller's v0 */
	}
	else{ /* newPcb is not NULL */
		/* initializing the fields of newPcb */
		newPcb->p_supportStruct = stateSYS->s_a2; /* initializing newPcb's supportStruct field based on the parameter currently in register a2 */
		(newPcb->p_s) = *stateSYS; /* initializing newPcb's processor state to that which currently lies in a1 */
		newPcb->p_time = INITIALACCTIME; /* initializing newPcb's p_time field to 0, since it has not yet accumualted any CPU time */
		newPcb->p_semAdd = NULL; /* initializing the pointer to newPcb's semaphore, which is set to NULL because newPcb is not in the "blocked" state */
		insertChild(currentProc, newPcb); /* initializing newPcb's process tree fields by making it a child of the Current Process */
		insertProcQ(&ReadyQueue, newPcb); /* inserting newPcb onto the Ready Queue */
		stateSYS->s_v0 = SUCCESSCONST; /* placing the value 0 in the caller's v0 because the allocation was completed successfully */
	}
}

/* Function that handles SYS 2. In other words, this internal function terminates the executing process and causes it to cease to 
exist. In addition, all progeny of this process are terminated as well. To accomplish this latter task, we will utilize head
recursion and invoke removeChild(). */
void terminateProcess(){
	/* terminating all progeny of the Current Process by utilizing head recursion */
	while (removeChild(currentProc != NULL)){ /* while there are progeny of the Current Process */
		terminateProcess();
	}

	/* destoying the executing process and causing it to cease to exist */
	freePcb(currentProc); /* returning currentProc onto the pcbFree list */
	procCnt--; /* decrementing the number of started, but not yet terminated, processes */
	switchProcess(); /* calling the Scheduler to begin executing the next process */
	/* ADD BULLET POINTS FROM 3.9 IN GREEN BOOK HERE */
}

/* Function that handles the steps needed for blocking a process. The function copies the saved processor state into the Current
Process' pcb, updates the accumulated CPU time for the Current Process, blocks the Current Process and calls the Scheduler so that the
CPU can begin executing the next process */
void blockCurr(int *sem){
	moveState(&(currentProc->p_s), (state_PTR) BIOSDATAPAGE); /* copying the saved processor state into the Current Process' pcb by calling moveState() */
	currentProc->p_time = 0; /* LEFT OFF WITH THIS LINE IN SYS 2 */
}

/* Function that represents the entry point for this module */
void exceptionHandler(){
	/* initializing global variables */
	stateSYS = currentProc->p_s.s_a1; /* initializing the processor state associated with the syscall to excState */
	currSYS = currentProc->p_s.s_a0; /* initializing the number of the syscall that we are addressing */

	/* ADD 4 TO PC */
	/* enumerating the possible syscall numbers and passing control to the appropriate internal function to handle the syscall */
	switch (currSYS){
		
		case 1: /* if the syscall number is 1 */
			createProcess();

		case 2: /* if the syscall number is 2 */
			terminateProcess();
	}
}
void uTLB_RefillHandler(){
	setENTRYHI(KUSEG);
	setENTRYLO(KSEG0);
	TLBWR();
	LDST((state_PTR) BIOSDATAPAGE);
}

