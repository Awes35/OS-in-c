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
#include "initial.c"
#include "/mnt/c/Users/JakeH/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Ubuntu/libumps.h"

void scheduler(){
	currentProc = removeProcQ(&ReadyQueue); /* removing the pcb from the head of the ReadyQueue and storing its pointer in currentProc */
	if (currentProc != NULL){ /* if the Ready Queue is not empty */
		/* LOAD TIMER AND GENERATE THE INTERRUPT */
		LDST(&(currentProc->p_s)); /* performing a LDST on the processor state stored in pcb of the Current Process */
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
}