/**************************************************************************** 
 *
 * This module implements phase 3's general exception handler, the SYSCALL
 * exception handler, and the Program Trap exception handler.
 *  
 * Written by: Kollen Gruizenga and Jake Heyser
 ****************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"

#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"

#include "../h/types.h"
#include "../h/const.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* function declarations */


/* declaring variables that are global to this module */
state_t savedExceptState; /* a pointer to the saved exception state */


/* Function that handles all passed up non-TLB exceptions (i.e., all SYSCALL exceptions numbered 9 and above, as well as all Program
Trap exceptions). Upon reaching this function, the processor state within the u-proc's support structure is in kernel-mode, as this
phase will execute things in kernel mode that a user normally could not. */
void vmGeneralExceptionHandler(){
	state_t oldState; /* a pointer to the saved processor's state at time of the exception */
	support_t *curProcSupportStruct; /* a pointer to the Current Process' Support Structure */
	int exceptionCode; /* the exception code */

	curProcSupportStruct = SYSCALL(SYS8NUM, 0, 0, 0); /* obtaining a pointer to the Current Process' Support Structure */
	oldState = curProcSupportStruct->sup_exceptState[GENERALEXCEPT]; /* initializing oldState to the state found in the Current Process' Support Structure for General (non-TLB) exceptions */
	exceptionCode = ((oldState.s_cause) & GETEXCEPCODE) >> CAUSESHIFT; /* initializing the exception code so that it matches the exception code stored in the .ExcCode field in the Cause register */

	if (exceptionCode == SYSCONST){ /* if the exception code is 8 */
		sysTrapHandler(); /* calling the Nucleus' SYSCALL exception handler function */
	}
    else if(exceptionCode > TLBCONST || exceptionCode == INTCONST){ /* avoid exception codes 1,2,3 pertaining to TLB exceptions */ 
        programTrapHandler();
    }

}


/* Function for SYS 9. This function kills the executing User Process by calling the Nucleus' SYS2 
function while in Kernel-mode. */
void terminateUProc(){
    /* We are in kernel-mode already */

    SYSCALL(SYS4NUM, &masterSemaphore, 0, 0); /* performing a V operation on masterSemaphore, to come to a more graceful conclusion */
    SYSCALL(SYS2NUM, 0, 0, 0); /* issuing a SYS2 to terminate the u-proc */
}


/* Function for SYS 10. This function places the current system time (since last reboot) into 
the calling User Process' v0 register */
void getTOD(){
    cpu_t currTOD; /* variable to store the current value on the time of day clock, upon a sys10 */

    STCK(currTOD); /* storing the current value on the Time of Day clock into currTOD */
    savedExceptState.s_v0 = currTOD; /* placing the current system time (since last reboot) in v0 */
    switchContext(savedExceptState); /* returning control to the Current Process by loading its (updated) processor state */
}


/* Function for SYS 11. This function causes the requesting user-process to be suspended until
a complete string of characters (of length strLength, with first character at address virtAddr) 
has been transmitted to the printer device associated with the user-process. 

Once the user-process resumes, this function returns in the process' v0 register either:
- the number of characters actually transmitted, if the write was successful or
- the negative of the printer device's status value (if the operation ends with a status other than "Character Transmitted"). */
void writeToPrinter(char *virtAddr, int strLength, int procASID){
    /* printers: interrupt line 6 */
    devregarea_t *temp; /* device register area that we can use to read and write the process procASID's printer device */
    int index; /* the index in devreg (and in devSemaphores) of the printer device associated with process procASID */
	int statusCode; /* the printer device's status value, located within the device's devreg */
    int proc_logicalAddr; /* the user-process' logical address space (starting value) */

    /* pre-checks: (each lead to SYS9)
        error if addr outside of u-proc's logical address space
        error if strLength < 0
        error if strLength > 128
    */
    proc_logicalAddr = KUSEG + procASID;
    if ((virtAddr < KUSEG) || (strLength < 0) || (strLength > MAXSTRLEN)){
        SYSCALL(SYS9NUM, 0, 0, 0); /* issue a SYS9 call to terminate the u-proc, as the request info is an error */
    }

	/* initializing local variables, except for statusCode, which will be initialized later */
	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
    index = ((PRNTINT - OFFSET) * DEVPERINT) + (procASID - 1); /* index of printer device associated with the u-proc */

    mutex(TRUE, &devSemaphores[index]); /* calling the function that gains mutual exclusion over process's printer device's device register */
	
    int i;
    for (i = 0; i < strLength; i++){
        /* pass info to printer device */
        temp->devreg[index].d_data0 = *(virtAddr+(i*WORDLEN)); /* pass character to print */

        /* prep to write to the printer device */
        setInterrupts(FALSE); /* calling the function that disables interrupts in order to write the COMMAND field and issue the SYS 5 atomically */
        temp->devreg[index].d_command = PRINTCHR;
        
        SYSCALL(SYS5NUM, LINE6, (procASID - 1), WRITE); /* issuing the SYS 5 call to block the I/O requesting process until the operation completes */
        setInterrupts(TRUE); /* calling the function that enables interrupts for the Status register, since the atomically-executed steps have now been completed */

    }

	/* read the status returned by printer device */
    statusCode = temp->devreg[index].d_status; /* setting the status code from the device register of process's associated printer device */

	if (statusCode != READY){ /* if the write operation led to an error status */
        savedExceptState.s_v0 = statusCode * (-1);
	}
    else{
        savedExceptState.s_v0 = strLength;
    }

	mutex(FALSE, &devSemaphores[index]); /* calling the function that releases mutual exclusion over process's printer device's device register */
}


/* Function for SYS 12 */
/* causes the requesting u-proc to be suspended until a line of output has been transmitted to the 
terminal device associated with the u-proc */
/* Once process resumes, return the number of characters actually transmitted in u-proc's v0 if write successful */
/* if the operation ends with a status other than "Character Transmitted" (5), then return negative of device's status value */
void writeToTerminal(char *virtAddr, int strLength, int procASID){
    /* terminal: interrupt line number 7 */
    devregarea_t *temp; /* device register area that we can use to read and write the process procASID's terminal device */
    int index; /* the index in devreg (and in devSemaphores) of the terminal device associated with process procASID */
	int statusCode; /* the status code from the device register associated with the device that corresponds to the highest-priority interrupt */

    /* pre-checks:
        error if addr outside of u-proc's logical address space
        error if strLength < 0
        error if strLength > 128
    */

	/* initializing local variables, except for statusCode, which will be initialized later */
	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
    
    index = ((TERMINT - OFFSET) * DEVPERINT) + (procASID - 1); /* determine terminal device associated with the u-proc */


    /* load appropriate values into the TRANSM_COMMAND field -- device register field 3 */
    /* Upon completion of the operation, an interrupt is raised, status code set in TRANSM_STATUS -- device reg field 0 */
    
    /* TRANSM_COMMAND has 2 parts: 
        command itself: TRANSM_COMMAND.TRANSM-CMD
        char to be transmitted: TRANSM_COMMAND.TRANSM-CHAR
    */

}


/* Function for SYS 13 */
void readFromTerminal(char *virtAddr){
    /* pass */
}


void sysTrapHandler(){
    /* initializing variables that are global to this module, as well as savedExceptState */ 
    support_t *curProcSupportStruct; /* a pointer to the Current Process' Support Structure */
    int sysNum; /* the number of the SYSCALL that we are addressing */
    int procASID; /* the ASID or Process ID for the Current Process */

    curProcSupportStruct = SYSCALL(SYS8NUM, 0, 0, 0); /* obtaining a pointer to the Current Process' Support Structure */
	savedExceptState = curProcSupportStruct->sup_exceptState[GENERALEXCEPT]; /* initializing savedExceptState to the state found in the Current Process' Support Structure for General (non-TLB) exceptions */
	sysNum = savedExceptState.s_a0; /* initializing the SYSCALL number variable to the correct number for the exception */
    procASID = curProcSupportStruct->sup_asid; /* get the process ID from the support structure */

	savedExceptState.s_pc = savedExceptState.s_pc + WORDLEN;

	/* enumerating the sysNum values (9-13) and passing control to the respective function to handle it */
	switch (sysNum){ 
        case SYS9NUM: /* if the sysNum indicates a SYS9 event */
            terminateUProc(); /* invoking the internal function that handles SYS9 events */
        
        case SYS10NUM: /* if the sysNum indicates a SYS10 event */
            getTOD(); /* invoking the internal function that handles SYS10 events */
        
        case SYS11NUM: /* if the sysNum indicates a SYS11 event */
            /* a1 should contain the virtual address of the first character of the string to be transmitted */
			/* a2 should contain the length of this string */
            writeToPrinter((char *) (savedExceptState.s_a1), (savedExceptState.s_a2)); /* invoking the internal function that handles SYS11 events */
        
        case SYS12NUM: /* if the sysNum indicates a SYS12 event */
            /* a1 should contain the virtual address of the first character of the string to be transmitted */
			/* a2 should contain the length of this string */
            writeToTerminal((char *) (savedExceptState.s_a1), (savedExceptState.s_a2)); /* invoking the internal function that handles SYS12 events */
        
        case SYS13NUM: /* if the sysNum indicates a SYS13 event */
            /* a1 should contain the virtual address of a string buffer where the data read should be placed */
            readFromTerminal((char *) (savedExceptState.s_a1)); /* invoking the internal function that handles SYS13 events */
        
    }
}

/* Function that handles Program Traps */
void programTrapHandler(){
    /* if the process to be terminated is currently holding mutual exclusion on a Support Level semaphore (Swap Pool semaphore),
        mutual exclusion must first be released (sys4) before invoking the Nucleus terminate command (sys2). */

    /* CHECK HOLDING MUTUAL EXCLUSION OVER POOLSEMAPHORE, DEVICESEMAPHORE? */

    /* terminate process in an orderly fashion -- perform same operations as a sys9 */
    SYSCALL(SYS4NUM, &masterSemaphore, 0, 0); /* performing a V operation on masterSemaphore, to come to a more graceful conclusion */
    SYSCALL(SYS2NUM, 0, 0, 0); /* issuing a SYS2 to terminate the u-proc */

}
