/**************************************************************************** 
 *
 * This module implements the Scheduler and the deadlock detector. More 
 * specifically, this module ensures that the Nucleus guarantees finite progress
 * by ensuring every ready process will have an opportunity to execute. As a 
 * result, this module includes the implementation of a preemptive round-robin
 * scheduling algorithm with a time slice of five milliseconds. The round-robin
 * algorithm in this module (assuming the Ready Queue is not empty) removes the 
 * pcb at the head of the Ready Queue and stores the pointer to the pcb in the
 * Current Process field. Then, it loads five milliseconds on the processor's
 * Local Timer before performing a LDST on the processor state stored in the pcb
 * of the Current Process. If the Ready Queue is empty, we have a series of 
 * conditionals that need to be examined. If the Process Count is zero, we 
 * invoke the HALT BIOS service/instruction. If the Process Count > 0 and
 * the Soft-block Count > 0, we enter a Wait State. Lastly, if the Process
 * Count > 0 and the Soft-block Count is zero, we invoke the PANIC BIOS
 * service/instruction to handle deadlock.
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 *
 ****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../phase2/initial.c"

/* Declaring global variables */
cpu_t original_tod; /* the time on the Time of Day clock that the most recent process began executing at */

/* Function to move processor state pointed to by source to processor state pointed to by dest. */
void moveState(state_PTR source, state_PTR dest){
	dest->s_entryHI = source->s_entryHI; /* setting dest's entryHI field to that of source */
	dest->s_cause = source->s_cause; /* setting dest's Cause register contents to those associated with source */
	dest->s_status = source->s_status; /* setting dest's Status register contents to those associated with source */
	dest->s_pc = source->s_pc; /* setting the PC associated with dest to the PC associated with source */

	/* setting the contents of all of the general purpose registers associated with dest to the contents of the general purpose registers
	associated with source */ 
	int i;
	for (i=0; i<STATEREGNUM; i++){ 
		dest->s_reg[i] = source->s_reg[i];
	}
}

void switchContext(pcb_PTR curr_proc){
	currentProc = curr_proc; /* DO WE NEED THIS LINE/ */
	LDST(&(currentProc->p_s)); /* loading the processor state for the processor state stored in pcb of the Current Process */
}

/* Function that includes the implementation of the scheduling algorithm that we will use in this operating system. The function
implements a simple preemptive round-robin scheduling algorithm with a time slice of five milliseconds. The function  begins by
removing the pcb at the head of the Ready Queue. If such a pcb exists, the function loads five milliseconds on the PLT and then
performs a Load Processor State (LDST) on the processor state stored in pcb of the Current Process. If the Ready Queue
was empty, then it checks to see if the Process Count is zero. If so, the function invokes the HALT BIOS instruction. 
If the Prcoess Count is greater than zero and Soft-block Count is greater than zero, the function enters a Wait State.
And if the Process Count is greater than zero and the Soft-block Count is zero, the function invokes the PANIC BIOS instruction. */
void switchProcess(){
	currentProc = removeProcQ(&ReadyQueue); /* removing the pcb from the head of the ReadyQueue and storing its pointer in currentProc */
	if (currentProc != NULL){ /* if the Ready Queue is not empty */
		setTIMER(INITIALPLT); /* loading five milliseconds on the processor's Local Timer (PLT) */

		switchContext(currentProc);
	}

	/* We know the ReadyQueue is empty. */
	if (procCnt == 0){ /* if the number of started, but not yet terminated, processes is zero */
		HALT(); /* invoking the HALT() function to halt the system and print a regular shutdown message on terminal 0 */
	}

	if (procCnt > 0 && softBlockCnt > 0){ /* if the number of started, but not yet terminated, processes is greater than zero and there's at least one such process is "blocked" */
		currentProc->p_s.s_status = ((currentProc->p_s.s_status) | IECON) & PLTOFF; /* enabling interrupts and disabling PLT for the Current Process so we can call the WAIT() function */
		WAIT(); /* invoking the WAIT() function to idle the processor, as it needs to wait for a device interrupt to occur */
	}

	/* A deadlock situation is occurring (i.e., procCnt > 0 && softBlockCnt == 0) */
	PANIC(); /* invoking the PANIC() function to stop the system and print a warning message on terminal 0 */
}