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
#include "../phase2/initial.c"

/* Function to move processor state pointed to by source to processor state pointed to by dest. */
void moveState(state_PTR source, state_PTR dest){

	dest->s_entryHI = source->s_entryHI;
	dest->s_cause = source->s_cause;
	dest->s_status = source->s_status;
	dest->s_pc = source->s_pc;

	for (int i=0; i<STATEREGNUM; i++){
		dest->s_reg[i] = source->s_reg[i];
	}
	
}


/* Function that includes the implementation of the scheduling algorithm that we will use in this operating system. The function
implements a simple preemptive round-robin scheduling algorithm with a time slice of five milliseconds. The function  begins by
removing the pcb at the head of the Ready Queue. If such a pcb exists, the function loads five milliseconds on the PLT and then
performs a Load Processor State (LDST) on the processor state stored in pcb of the Current Process. If the Ready Queue
was empty, then it checks to see if the Process Count is zero. If so, the function invokes the HALT BIOS instruction. 
If the Prcoess Count is greater than zero and Soft-block Count is greater than zero, the function enters a Wait State.
And if the Process Count is greater than zero and the Soft-block Count is zero, the function invokes the PANIC BIOS instruction. */
void scheduler(){
	currentProc = removeProcQ(&ReadyQueue); /* removing the pcb from the head of the ReadyQueue and storing its pointer in currentProc */
	if (currentProc != NULL){ /* if the Ready Queue is not empty */
		setTIMER(INITIALPLT); /* loading five milliseconds on the processor's Local Timer (PLT) */
		LDST(&(currentProc->p_s)); /* loading the processor state for the processor state stored in pcb of the Current Process */
		return;
	}

	/* We know the ReadyQueue is empty. */
	if (procCnt == 0){ /* if the number of started, but not yet terminated, processes is zero */
		HALT(); /* invoking the HALT() function to halt the system and print a regular shutdown message on terminal 0 */
		return;
	}

	if (procCnt > 0 && softBlockCnt > 0){ /* if the number of started, but not yet terminated, processes is greater than zero and there's at least one such process is "blocked" */
		/* SET THE STATUS REGISTER TO ENABLE INTERRUPTS AND DISABLE THE PLT OR LOAD IT WITH A LARGE VALUE */
		WAIT(); /* invoking the WAIT() function to idle the processor, as it needs to wait for a device interrupt to occur */
		return;
	}

	/* A deadlock situation is occurring (i.e., procCnt > 0 && softBlockCnt == 0) */
	PANIC(); /* invoking the PANIC() function to stop the system and print a warning message on terminal 0 */
}