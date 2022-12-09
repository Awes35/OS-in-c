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
HIDDEN void terminateUProc();
HIDDEN void getTOD(state_PTR savedState);
HIDDEN void writeToPrinter(char *virtAddr, int strLength, int procASID, state_PTR savedState);
HIDDEN void writeToTerminal(char *virtAddr, int strLength, int procASID, state_PTR savedState);
HIDDEN void sysTrapHandler(state_PTR savedState, support_t *curProcSupportStruct);


/* Function that handles all passed up non-TLB exceptions (i.e., all SYSCALL exceptions numbered 9 and above, as well as all Program
Trap exceptions). Upon reaching this function, the processor state within the u-proc's support structure is in kernel-mode, as this
phase will execute things in kernel mode that a user normally could not. */
void vmGeneralExceptionHandler(){
	state_PTR savedState; /* a pointer to the saved processor's state at time of the exception */
	support_t *curProcSupportStruct; /* a pointer to the Current Process' Support Structure */
	int exceptionCode; /* the exception code */

	curProcSupportStruct = (support_t *) SYSCALL(SYS8NUM, 0, 0, 0); /* obtaining a pointer to the Current Process' Support Structure */
	savedState = &(curProcSupportStruct->sup_exceptState[GENERALEXCEPT]); /* initializing savedState to the state found in the Current Process' Support Structure for General (non-TLB) exceptions */
	exceptionCode = ((savedState->s_cause) & GETEXCEPCODE) >> CAUSESHIFT; /* initializing the exception code so that it matches the exception code stored in the .ExcCode field in the Cause register */

	if (exceptionCode == SYSCONST){ /* if the exception code is 8 */
		sysTrapHandler(savedState, curProcSupportStruct); /* calling the Nucleus' SYSCALL exception handler function */
	}
    if (exceptionCode > TLBCONST || exceptionCode == INTCONST){ /* avoid exception codes 1,2,3 pertaining to TLB exceptions */ 
        programTrapHandler();
    }

}


/* Function for SYS 9. This function kills the executing User Process by calling the Nucleus' SYS2 
function while in Kernel-mode. */
void terminateUProc(){
    /* We are in kernel-mode already */

    SYSCALL(SYS4NUM, (unsigned int) &masterSemaphore, 0, 0); /* performing a V operation on masterSemaphore, to come to a more graceful conclusion */
    SYSCALL(SYS2NUM, 0, 0, 0); /* issuing a SYS2 to terminate the u-proc */
}


/* Function for SYS 10. This function places the current system time (since last reboot) into 
the calling User Process' v0 register */
void getTOD(state_PTR savedState){
    cpu_t currTOD; /* variable to store the current value on the time of day clock, upon a sys10 */

    STCK(currTOD); /* storing the current value on the Time of Day clock into currTOD */
    savedState->s_v0 = currTOD; /* placing the current system time (since last reboot) in v0 */
    switchUContext(savedState); /* returning control to the Current Process by loading its (updated) processor state */
}


/* Function for SYS 11. This function causes the requesting user-process to be suspended until
a complete string of characters (of length strLength, with first character at address virtAddr) 
has been transmitted to the printer device associated with the user-process. 

Once the user-process resumes, this function returns in the process' v0 register either:
- the number of characters actually transmitted, if the write was successful or
- the negative of the printer device's status value (if the operation ends with a status other than "Device Ready"). */
void writeToPrinter(char *virtAddr, int strLength, int procASID, state_PTR savedState){
    /* printers: interrupt line 6 */
    devregarea_t *temp; /* pointer to device register area that we can use to read and write the process procASID's printer device */
    int index; /* the index in devreg (and in devSemaphores) of the printer device associated with process procASID */
	int statusCode; /* the printer device's status value, located within the device's devreg */

    /* pre-checks: (each lead to SYS9)
        error if addr outside of u-proc's logical address space (KUSEG)
        error if strLength < 0
        error if strLength > 128 */
    if (((unsigned int) virtAddr < KUSEG) || (strLength < 0) || (strLength > MAXSTRLEN)){
        SYSCALL(SYS9NUM, 0, 0, 0); /* issue a SYS9 call to terminate the u-proc, as the request info is an error */
    }

	/* initializing local variables, except for statusCode, which will be initialized later */
	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
    index = ((PRNTINT - OFFSET) * DEVPERINT) + (procASID - 1); /* index of printer device associated with the u-proc */
    
    mutex(TRUE, (int *) (&devSemaphores[index])); /* calling the function that gains mutual exclusion over process's printer device's device register */
	
    int i;
    for (i = 0; i < strLength; i++){
        /* pass info to printer device */
        temp->devreg[index].d_data0 = *(virtAddr + (i * WORDLEN)); /* pass character to print */

        /* prep to write to the printer device */
        setInterrupts(FALSE); /* calling the function that disables interrupts in order to write the COMMAND field and issue the SYS 5 atomically */
        temp->devreg[index].d_command = PRINTCHR; /* placing the command code for printing the character into the printer's command field */
        
        SYSCALL(SYS5NUM, LINE6, (procASID - 1), WRITE); /* issuing the SYS 5 call to block the I/O requesting process until the operation completes */
        setInterrupts(TRUE); /* calling the function that enables interrupts for the Status register, since the atomically-executed steps have now been completed */

    }

	/* read the status returned by printer device */
    statusCode = temp->devreg[index].d_status; /* setting the status code from the device register of process's associated printer device */

	if (statusCode != READY){ /* if the write operation led to an error status */
        savedState->s_v0 = statusCode * (-1); /* return negative of the status code */
	}
    else{ /* else, successful operation */
        savedState->s_v0 = strLength; /* return length of string transmitted */
    }

	mutex(FALSE, (int *) (&devSemaphores[index])); /* calling the function that releases mutual exclusion over process's printer device's device register */
    switchUContext(savedState); /* return control back to the Current Process */
}

/* Function for SYS 12. This function causes the requesting user-process to be suspended until
a complete string of characters (of length strLength, with first character at address virtAddr) 
has been transmitted to the terminal device associated with the user-process. 

Once the user-process resumes, this function returns in the process' v0 register either:
- the number of characters actually transmitted, if the write was successful or
- the negative of the terminal device's status value (if the operation ends with a status other than "Character Transmitted"). */
void writeToTerminal(char *virtAddr, int strLength, int procASID, state_PTR savedState){
    /* terminals: interrupt line 7 */
    devregarea_t *temp; /* pointer to device register area that we can use to read and write the process procASID's terminal device */
    int index; /* the index in devreg (and in devSemaphores) of the terminal device associated with process procASID */
	int statusCode; /* the terminal device's status value, located within the device's devreg */

    /* pre-checks: (each lead to SYS9)
        error if addr outside of u-proc's logical address space (KUSEG)
        error if strLength < 0
        error if strLength > 128 */
    if (((unsigned int) virtAddr < KUSEG) || (strLength < 0) || (strLength > MAXSTRLEN)){
        SYSCALL(SYS9NUM, 0, 0, 0); /* issue a SYS9 call to terminate the u-proc, as the request info is an error */
    }

	/* initializing local variables, except for statusCode, which will be initialized later */
	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
    index = ((TERMINT - OFFSET) * DEVPERINT) + (procASID - 1) + DEVPERINT; /* index of terminal device associated with the u-proc; note that terminal device semaphores for writing come after those for reading */
    
    mutex(TRUE, (int *) (&devSemaphores[index])); /* calling the function that gains mutual exclusion over the appropriate terminal device's device register */
	
    int i;
    for (i = 0; i < strLength; i++){
        /* prep to write to the terminal device */
        setInterrupts(FALSE); /* calling the function that disables interrupts in order to write the COMMAND field and issue the SYS 5 atomically */
        temp->devreg[index].t_transm_command = (*(virtAddr + (i * WORDLEN))  << TERMSHIFT) | TRANSMITCHAR; /* placing the command code for printing the character into the terminal's command field (and the character to be printed) */
        
        SYSCALL(SYS5NUM, LINE7, (procASID - 1), WRITE); /* issuing the SYS 5 call to block the I/O requesting process until the operation completes */
        setInterrupts(TRUE); /* calling the function that enables interrupts for the Status register, since the atomically-executed steps have now been completed */

    }

	/* read the status returned by terminal device */
    statusCode = temp->devreg[index].t_transm_status; /* setting the status code from the device register of process's associated terminal device */

	if (statusCode != CHARTRANSM){ /* if the write operation led to an error status */
        savedState->s_v0 = statusCode * (-1); /* return negative of the status code */
	}
    else{ /* else, successful operation */
        savedState->s_v0 = strLength; /* return length of string transmitted */
    }

	mutex(FALSE, (int *) (&devSemaphores[index])); /* calling the function that releases mutual exclusion over the appropriate terminal device's device register */
    switchUContext(savedState); /* return control back to the Current Process */
}

void sysTrapHandler(state_PTR savedState, support_t *curProcSupportStruct){
    /* initializing variables that are global to this module, as well as savedState */ 
    int sysNum; /* the number of the SYSCALL that we are addressing */
    int procASID; /* the ASID or Process ID for the Current Process */

    sysNum = savedState->s_a0; /* initializing the SYSCALL number variable to the correct number for the exception */
    procASID = curProcSupportStruct->sup_asid; /* get the process ID from the support structure */

	savedState->s_pc = savedState->s_pc + WORDLEN; /* increment the proc's PC to continue with the next instruction when it resumes running */

	/* enumerating the sysNum values (9-11) and passing control to the respective function to handle it */
	switch (sysNum){ 
        case SYS9NUM: /* if the sysNum indicates a SYS9 event */
            terminateUProc(); /* invoking the internal function that handles SYS9 events */
        
        case SYS10NUM: /* if the sysNum indicates a SYS10 event */
            getTOD(savedState); /* invoking the internal function that handles SYS10 events */
        
        case SYS11NUM: /* if the sysNum indicates a SYS11 event */
           	/* a1 should contain the virtual address of the first character of the string to be transmitted */
			/* a2 should contain the length of this string */
            writeToPrinter((char *) (savedState->s_a1), (int) (savedState->s_a2), procASID, savedState); /* invoking the internal function that handles SYS11 events */
			
	case SYS12NUM: /* if the sysNum indicates a SYS12 event */
		/* a1 should contain the virtual address of the first character of the string to be transmitted */
			/* a2 should contain the length of this string */
		writeToTerminal((char *) (savedState->s_a1), (int) (savedState->s_a2), procASID, savedState); /* invoking the internal function that handles SYS12 events */
		
    }
}

/* Function that handles Program Traps */
void programTrapHandler(){
    /* terminate process in an orderly fashion */
    SYSCALL(SYS9NUM, 0, 0, 0); /* issue a SYS9 call to terminate the u-proc (gracefully) */
}
