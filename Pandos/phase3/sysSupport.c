/**************************************************************************** 
 *
 * This module implements phase 3's general exception handler, the SYSCALL
 * exception handler, and the Program Trap exception handler. More specifically,
 * this module first determines the cause of the exception that was raised. If
 * the exception code is 8, then control is passed to the sysTrapHandler() function;
 * otherwise, control is passed to the phase 3 Program Trap handler. The Program
 * Trap handler simply executes a SYS9 and termiantes the running U-proc. On the
 * other hand, the phase 3 System Trap Handler function handles SYSCALLS 9, 10
 * and 12 when the running process is in kernel-mode. The function determines which
 * SYSCALL number was requested and then passes control to the appropriate internal
 * helper function. Once the SYSCALL is handled, control is passed back to
 * the requesting U-proc.
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
HIDDEN void writeToTerminal(char *virtAddr, int strLength, int procASID, state_PTR savedState);
HIDDEN void sysTrapHandler(state_PTR savedState, support_t *curProcSupportStruct);

/* Function that handles all passed up non-TLB exceptions (i.e., all SYSCALL exceptions numbered 9 and above, as well as all Program
Trap exceptions). Upon reaching this function, the processor state within the U-proc's support structure is in kernel-mode, as this
phase will execute requests in kernel mode that a user normally could not. This function first determines the cause of the exception
that was raised. If the exception code is 8, then control is passed to the sysTrapHandler() function; otherwise, control is passed to
the phase 3 Program Trap handler. */
void vmGeneralExceptionHandler(){
	/* declaring local variables */
	state_PTR savedState; /* a pointer to the saved processor's state at time of the exception */
	support_t *curProcSupportStruct; /* a pointer to the Current Process' Support Structure */
	int exceptionCode; /* the exception code */

	/* initializing local variables */
	curProcSupportStruct = (support_t *) SYSCALL(SYS8NUM, 0, 0, 0); /* obtaining a pointer to the Current Process' Support Structure */
	savedState = &(curProcSupportStruct->sup_exceptState[GENERALEXCEPT]); /* initializing savedState to the state found in the Current Process' Support Structure for General (non-TLB) exceptions */
	exceptionCode = ((savedState->s_cause) & GETEXCEPCODE) >> CAUSESHIFT; /* initializing the exception code so that it matches the exception code stored in the .ExcCode field in the Cause register */

	/* calling the appropriate internal function based off the exception code */
	if (exceptionCode == SYSCONST){ /* if the exception code is 8 */
		sysTrapHandler(savedState, curProcSupportStruct); /* calling the phase 3 SYSCALL exception handler */
	} 
    
    programTrapHandler(); /* calling the phase 3 Program Trap handler, as the exception code indicated a non-SYSCALL and non-TLB exception */
}

/* Internal function that handles SYS9 requests. This function kills the executing User Process by calling the Nucleus' SYS2 
function while in kernel-mode. Before issuing the SYS2, it also performs a V operation on masterSemaphore in order to ensure that
test() comes to a more gracious conclusion. */
void terminateUProc(){
    /* We are in kernel-mode already */

    SYSCALL(SYS4NUM, (unsigned int) &masterSemaphore, 0, 0); /* performing a V operation on masterSemaphore, to come to a more graceful conclusion */
    SYSCALL(SYS2NUM, 0, 0, 0); /* issuing a SYS2 to terminate the U-proc */
}

/* Internal function that handles SYS10 requests. This function places the current system time (since the last reboot) into 
the calling U-proc's v0 register and returns control back to the running U-proc. */
void getTOD(state_PTR savedState){
	/* declaring local variables */
    cpu_t currTOD; /* local variable to store the current value on the time of day clock, upon a sys10 */

    STCK(currTOD); /* storing the current value on the Time of Day clock into currTOD */
    savedState->s_v0 = currTOD; /* placing the current system time (since last reboot) in v0 */
    switchUContext(savedState); /* returning control to the Current Process by loading its (updated) processor state */
}	

/* Internal function that handles SYS12 requests. This function causes the requesting user-process to be suspended until
a complete string of characters (of length strLength, with first character at address virtAddr) has been transmitted to the
terminal device associated with the user-process. Once the user-process resumes, this function returns in the process' v0 register either:
- the number of characters actually transmitted, if the write was successful or
- the negative of the terminal device's status value (if the operation ends with a status other than "Character Transmitted"). */
void writeToTerminal(char *virtAddr, int strLength, int procASID, state_PTR savedState){
	/* declaring local variables */
    devregarea_t *temp; /* pointer to device register area that we can use to read and write the process procASID's terminal device */
    int index; /* the index in devreg (and in devSemaphores) of the terminal device associated with process procASID */
	unsigned int status; /* the contents of the terminal device's status register after a SYS5 call is issued */
	int statusCode; /* the status code returned by the terminal device after a SYS5 call is issued */

    /* pre-checks: (each lead to SYS9)
        error if addr outside of u-proc's logical address space (KUSEG)
        error if strLength < 0
        error if strLength > 128 */
    if (((unsigned int) virtAddr < KUSEG) || (strLength < 0) || (strLength > MAXSTRLEN)){
        terminateUProc(); /* calling the internal function that handles SYS9 requests to terminate the U-proc, as the request info is an error */
    }

	/* initializing local variables, except for status and statusCode, which will be initialized later */
	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
    index = ((TERMINT - OFFSET) * DEVPERINT) + (procASID - 1); /* index of terminal device associated with the U-proc; note that terminal device semaphores for writing come after those for reading */
    
    mutex(TRUE, (int *) &(devSemaphores[index + DEVPERINT])); /* calling the function that gains mutual exclusion over the appropriate terminal device's device register */

	/* transmitting each character to the terminal */
    int i;
    for (i = 0; i < strLength; i++){
		setInterrupts(FALSE); /* calling the function that disables interrupts in order to write the COMMAND field and issue the SYS 5 atomically */
    	temp->devreg[index].t_transm_command = (*(virtAddr + i)  << TERMSHIFT) | TRANSMITCHAR; /* placing the command code for printing the character into the terminal's command field (and the character to be printed) */
    	status = SYSCALL(SYS5NUM, LINE7, (procASID - 1), WRITE); /* issuing the SYS 5 call to block the I/O requesting process until the operation completes */
		setInterrupts(TRUE); /* calling the function that enables interrupts for the Status register, since the atomically-executed steps have now been completed */
		statusCode = status & TERMSTATUSON; /* setting the status code returned by the terminal device after the SYS5 call */
	    
		if (statusCode != CHARTRANSM){ /* if the write operation led to an error status */
			savedState->s_v0 = statusCode * (-1); /* returning the negative of the status code */
			mutex(FALSE, (int *) &(devSemaphores[index + DEVPERINT])); /* calling the function that releases mutual exclusion over the appropriate terminal device's device register */
   			switchUContext(savedState); /* return control back to the Current Process */
		}
    }
	
	/* the write operation was successful */
	savedState->s_v0 = strLength; /* return length of string transmitted */
	mutex(FALSE, (int *) &(devSemaphores[index + DEVPERINT])); /* calling the function that releases mutual exclusion over the appropriate terminal device's device register */
   	switchUContext(savedState); /* return control back to the Current Process */
}

/* Internal function that handles SYSCALL events when the running process is executing in kernel-mode. This function's tasks include, but are not
limited to, incrementing the value of the PC in the stored exception state (to avoid an infinite loop of SYSCALLs) and checking to see what SYSCALL
number was requested so it can invoke an internal helper function to handle that specific SYSCALL. If an invalid SYSCALL number was provided
(i.e., the SYSCALL number requested was not 9, 10 or 12), we invoke the internal function that handles phase 3 Program Traps. */ 
void sysTrapHandler(state_PTR savedState, support_t *curProcSupportStruct){
    /* declaring local variables */ 
    int sysNum; /* the number of the SYSCALL that we are addressing */
    int procASID; /* the ASID or Process ID for the Current Process */
	
	/* initializing local variables */
    sysNum = savedState->s_a0; /* initializing the SYSCALL number variable to the correct number for the exception */
    procASID = curProcSupportStruct->sup_asid; /* get the process ID from the support structure */

	savedState->s_pc = savedState->s_pc + WORDLEN; /* increment the proc's PC to continue with the next instruction when it resumes running */

	/* enumerating the sysNum values (9, 10 and 12) and passing control to the respective function to handle the SYSCALL request */
	switch (sysNum){ 
        case SYS9NUM: /* if the sysNum indicates a SYS9 event */
            terminateUProc(); /* invoking the internal function that handles SYS9 events */
        
        case SYS10NUM: /* if the sysNum indicates a SYS10 event */
            getTOD(savedState); /* invoking the internal function that handles SYS10 events */
			
		case SYS12NUM: /* if the sysNum indicates a SYS12 event */
			/* a1 should contain the virtual address of the first character of the string to be transmitted */
			/* a2 should contain the length of this string */
			writeToTerminal((char *) (savedState->s_a1), (int) (savedState->s_a2), procASID, savedState); /* invoking the internal function that handles SYS12 events */	
			
		default: /* the sysNum indicates a SYSCALL event whose number is not between 9 and 12 */
			programTrapHandler(); /* calling the phase 3 function that handles Program Traps */		
    }
}

/* Function that handles Program Traps in phase 3. The function performs the same operations as a SYS9 by calling the internal function that
handles SYS9 requests. In other words, the function terminates the running U-proc in an orderly fashion. */
void programTrapHandler(){
    terminateUProc(); /* calling the internal function that handles SYS9 requests */
}
