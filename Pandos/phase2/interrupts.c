/**************************************************************************** 
  *
  * This module contains the definition for all interrupt-handling functions.
  * Such functions are called by the Nucleus in initial.c.
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

 /* Function declarations */
 HIDDEN void pltTimerInt(pcb_PTR proc);
 HIDDEN void intTimerInt(pcb_PTR proc);
 HIDDEN void terminalInt(pcb_PTR proc);
 HIDDEN void IOInt(pcb_PTR proc);
 HIDDEN int findDeviceNum(int lineNumber);
 HIDDEN pcb_PTR unblockSem(int *sem, pcb_PTR proc);

 /* Declaring global variables */
 cpu_t curr_tod; /* the current value on the Time of Day clock */
 cpu_t remaining_time; /* the amount of time left on the Current Process' quantum */
 state_PTR savedExceptState; /* a pointer to the saved exception state */

 /* Internal helper function responsible for determining what device number the highest-priority interrupt occurred on. The function
 returns that number to the caller. */
 int findDeviceNum(int lineNumber){
	/* declaring local variables */
	devregarea_t *temp; /* device register area that we can use to determine which device number is associated with the highest-priority interrupt */
	unsigned int bitMap; /* the 32-bit contents of the Interrupt Devices Bit Map associated with the line number that was passed in as a parameter */

	/* initializing temp and bitMap in order to determine which device number is associated with the highest-priority interrupt */
	temp = (devregarea_t *) INTDEVICEADDR; /* initialization of temp */
	bitMap = temp->interrupt_dev[lineNumber - OFFSET]; /* initialization of bitMap */

	/* determining which device number is associated with the highest-priority interrupt based on the address of bitMap */
	if (bitMap & DEV0INT != ALLOFF){ /* if there is a pending interrupt associated with device 0 */
		return DEV0; /* returning 0 to the user, since the highest-priority interrupt is associated with device 0 */
	}
	if (bitMap & DEV1INT != ALLOFF){ /* if there is a pending interrupt associated with device 1 */
		return DEV1; /* returning 1 to the user, since the highest-priority interrupt is associated with device 1 */
	}
	if (bitMap & DEV2INT != ALLOFF){ /* if there is a pending interrupt associated with device 2 */
		return DEV2; /* returning 2 to the user, since the highest-priority interrupt is associated with device 2 */
	}
	if (bitMap & DEV3INT != ALLOFF){ /* if there is a pending interrupt associated with device 3 */
		return DEV3; /* returning 3 to the user, since the highest-priority interrupt is associated with device 3 */
	}
	if (bitMap & DEV4INT != ALLOFF){ /* if there is a pending interrupt associated with device 4 */
		return DEV4; /* returning 4 to the user, since the highest-priority interrupt is associated with device 4 */
	}
	if (bitMap & DEV5INT != ALLOFF){ /* if there is a pending interrupt associated with device 5 */
		return DEV5; /* returning 5 to the user, since the highest-priority interrupt is associated with device 5 */
	}
	if (bitMap & DEV6INT != ALLOFF){ /* if there is a pending interrupt associated with device 6 */
		return DEV6; /* returning 6 to the user, since the highest-priority interrupt is associated with device 6 */
	}
	/* otherwise, there is a pending interrupt associated with device 7, since there are only eight devices */
	return DEV7; /* returning 7 to the user, since the highest-priority interrupt is associated with device 7 */
 }

 /* Internal helper function that handles Processor Local Timer (PLT) interrupts */
 void pltTimerInt(pcb_PTR proc){
 	if (proc != NULL){
 		moveState(savedExceptState, &(proc->p_s)); /* moving the updated saved exception state from the BIOS Data Page into the Current Process' processor state */
 		proc->p_time = proc->p_time + (curr_tod - start_tod); /* updating the accumulated processor time used by the Current Process */
 		insertProcQ(&ReadyQueue, proc); /* placing the Current Process back on the Ready Queue because it has not completed its CPU Burst */
 		switchProcess(); /* calling the Scheduler to begin execution of the next process on the Ready Queue */
 	}
 	PANIC(); /* otherwise, Current Process is NULL, so the function invokes the PANIC() function to stop the system and print a warning message on terminal 0 */
 }

 /* Internal helper function that handles interrupts generated by the System-wide Interval Timer. Note that for the purposes of this
 function, we will NOT charge any process with the accumulated CPU time needed to handle this interrupt, primarily because we should not
 charge the Current Process with such time, because it is possible for the an Interval Timer interrupt to occur even when there is no
 Current Process. It is also illogical to charge the Current Process with the CPU time needed to handle this type of interrupt because
 the Current Process is not actually using this CPU time to execute its own process; the time is being spent handling a system-wide 
 interrupt. So, we will refrain from charging any process with the time needed to handle this type of interrupt. */
 void intTimerInt(pcb_PTR proc){
 	LDIT(INITIALINTTIMER); /* placing 100 milliseconds back on the Interval Timer for the next Pseudo-clock tick */
 	/* unblocking all pcbs blocked on the Pseudo-Clock semaphore */
	while (headBlocked(&deviceSemaphores[PCLOCKIDX]) != NULL){ /* while the Pseudo-Clock semaphore has a blocked pcb */
		removeBlocked(&deviceSemaphores[PCLOCKIDX]); /* unblock the first (i.e., head) pcb from the Pseudo-Clock semaphore's process queue */
	}
	deviceSemaphores[PCLOCKIDX] = INITIALPCSEM; /* resetting the Pseudo-clock semaphore to zero */
	moveState(savedExceptState, &(proc->p_s)); /* moving the updated saved exception state from the BIOS Data Page into the Current Process' processor state */
	if (currentProc != NULL){ /* if there is a Current Process to return control to */
		switchContext(proc); /* calling the function that returns control to the Current Process */
 	}
	switchProcess(); /* if there is no Current Process to return control to, call the Scheduler function to begin executing a new process */
}

/* Internal helper function that handles I/O interrupts on non-terminal devices. In other words, this function handles interrupts that
occurred on lines 3-6, as indicated in the Cause register. Note that for the purposes of this function, we will NOT charge any process
with the accumulated CPU time needed to handle this interrupt, primarily because the Current Process is not actually using this CPU
time to execute its own process; the time is being spent handling a interrupt generated by another device. We will also not charge the 
interrupting device (and its associated process) with the time spent handling the I/O interrupt. So, we will refrain from charging any
process with the time needed to handle this type of interrupt. */
void IOInt(pcb_PTR proc){
	/* delcaring local variables */
	int lineNum; /* the line number that the highest-priority interrupt occurred on */
	int devNum; /* the device number that the highest-priority interrupt occurred on */
	int index; /* the index in deviceSemaphores and in devreg of the device associated with the highest-priority interrupt */
	/* memaddr devRegAddr; /* the starting address of devNum's device register */
	devregarea_t *temp; /* device register area that we can use to determine the status code from the device register associated with the highest-priority interrupt */
	unsigned int statusCode; /* the status code from the device register associated with the device that corresponds to the highest-priority interrupt */
	pcb_PTR unblockedPcb; /* the pcb which originally initiated the I/O request */

	/* determining exactly which line number the interrupt occurred on so we can initialize lineNum */
	if (savedExceptState->s_cause & LINE3INT != ALLOFF){ /* if there is an interrupt on line 3 */
		lineNum = LINE3; /* initializing lineNum to 3, since the highest-priority interrupt occurred on line 3 */
	}
	else if (savedExceptState->s_cause & LINE4INT != ALLOFF){ /* if there is an interrupt on line 4 */
		lineNum = LINE4; /* initializing lineNum to 4, since the highest-priority interrupt occurred on line 4 */
	}
	else if (savedExceptState->s_cause & LINE5INT != ALLOFF){ /* if there is an interrupt on line 5 */
		lineNum = LINE5; /* initializing lineNum to 5, since the highest-priority interrupt occurred on line 5 */
	}
	else if (savedExceptState->s_cause & LINE6INT != ALLOFF){ /* if there is an interrupt on line 6*/
		lineNum = LINE6; /* initializing lineNum to 6, since the highest-priority interrupt occurred on line 6 */
	}

	/* initializing remaining local variables, except for unblockedPcb, which will be initialized later on */
	devNum = findDeviceNum(lineNum); /* initializing devNum by calling the internal function that returns the device number associated with the highest-priority interrupt */	
	index = ((lineNum - OFFSET) * DEVPERINT) + devNum; /* initializing the index in deviceSemaphores of the device associated with the highest-priority interrupt */
	/* devRegAddr = ((memaddr) DEVADDRBASE) + ((lineNum - OFFSET) * ((memaddr) DEVADDROFFSET)) + (devNum * ((memaddr) DEVADDROFFSET2)); /* initializing the starting address of devNum's device register */
	temp = (devregarea_t *) DEVADDRBASE; /* initialization of temp */
	statusCode = temp->devreg[index].d_status; /* initializing the status code from the device register associated with the device that corresponds to the highest-priority interrupt */
	
	temp->devreg[index].d_command = ACK; /* acknowledging the outstanding interrupt by writing the acknowledge command code in the interrupting device's device register */
	unblockedPcb = removeBlocked(&deviceSemaphores[index]); /* initializing unblockedPcb by unblocking the semaphore associated with the interrupt and returning the corresponding pcb */

	if (unblockedPcb == NULL){ /* if the supposedly unblocked pcb is NULL, we want to return control to the Current Process */
		moveState(savedExceptState, &(proc->p_s)); /* update the Current Process' processor state before resuming process' execution */
		if (currentProc != NULL){ /* if there is a Current Process to return control to */
			switchContext(proc); /* calling the function that returns control to the Current Process */
		}
		switchProcess(); /*calling the Scheduler to begin execution of the next process on the Ready Queue (if there is no Current Process to return control to) */
	}
	
	/* unblockedPcb is not NULL */
	*(unblockedPcb->p_semAdd)++; /* incrementing the value of the semaphore associated with the interrupt as part of the V operation */
	unblockedPcb->p_s.s_v0 = statusCode; /* placing the stored off status code in the newly unblocked pcb's v0 register */
	insertProcQ(&ReadyQueue, unblockedPcb); /* inserting the newly unblocked pcb on the Ready Queue to transition it from a "blocked" state to a "ready" state */
	softBlockCnt--; /* decrementing the value of softBlockCnt, since we have unblocked a previosuly-started process that was waiting for I/O */
	moveState(savedExceptState, &(proc->p_s)); /* update the Current Process' processor state before resuming process' execution */
	if (currentProc != NULL){ /* if there is a Current Process to return control to */
		switchContext(proc); /* calling the function that returns control to the Current Process */
	}
	switchProcess(); /*calling the Scheduler to begin execution of the next process on the Ready Queue (if there is no Current Process to return control to) */
}

/* Internal helper function that handles I/O interrupts on terminal devices. In other words, this function handles interrupts that
occurred on line 7, as indicated in the Cause register. Note that interrupts that involve writing take priority over interrupts that
involve reading. Additionally, for the purposes of this function, we will NOT charge any process with the accumulated CPU time needed
to handle this interrupt, primarily because the Current Process is not actually using this CPU time to execute its own process;
the time is being spent handling a interrupt generated by another device. We will also not charge the interrupting device (and its
associated process) with the time spent handling the I/O interrupt. So, we will refrain from charging any process with the time needed
to handle this type of interrupt.*/
void terminalInt(pcb_PTR proc){
	/* delcaring local variables */
	int devNum; /* the device number that the highest-priority interrupt occurred on */
	int index; /* the index in devreg of the device associated with the highest-priority interrupt */
	devregarea_t *temp; /* device register area that we can use to determine the status code from the device register associated with the highest-priority interrupt */
	unsigned int statusCode; /* the status code from the device register associated with the device that corresponds to the highest-priority interrupt */
	pcb_PTR unblockedPcb; /* the pcb which originally initiated the I/O request */

	/* initializing remaining local variables, except for unblockedPcb, which will be initialized later on */
	devNum = findDeviceNum(LINE7); /* initializing devNum by calling the internal function that returns the device number associated with the highest-priority interrupt */	
	index = ((LINE7 - OFFSET) * DEVPERINT) + devNum; /* initializing the index of the device register of the device associated with the highest-priority interrupt */
	temp = (devregarea_t *) DEVADDRBASE; /* initialization of temp */
	if (temp->devreg[index].d_data0 & STATUSON != READY){ /* if the device's status code is not 1, meaning the device is not "Ready" */
		/* the interrupt associated with the terminal device is a write interrupt */
		statusCode = temp->devreg[index].d_data0; /* initializing the status code from the device register associated with the device that corresponds to the highest-priority interrupt */
		temp->devreg[index].d_data0 = ACK; /* acknowledging the outstanding interrupt by writing the acknowledge command code in the interrupting device's device register */
		unblockedPcb = removeBlocked(&deviceSemaphores[index + DEVPERINT]); /* initializing unblockedPcb by unblocking the semaphore associated with the interrupt and returning the corresponding pcb */
	}
	else{ /* otherwise, the interrupt associated with the terminal device is a read interrupt */
		statusCode = temp->devreg[index].d_status; /* initializing the status code from the device register associated with the device that corresponds to the highest-priority interrupt */
		temp->devreg[index].d_command = ACK; /* acknowledging the outstanding interrupt by writing the acknowledge command code in the interrupting device's device register */
		unblockedPcb = removeBlocked(&deviceSemaphores[index]); /* initializing unblockedPcb by unblocking the semaphore associated with the interrupt and returning the corresponding pcb */
	}
	
	if (unblockedPcb == NULL){ /* if the supposedly unblocked pcb is NULL, we want to return control to the Current Process */
		moveState(savedExceptState, &(proc->p_s)); /* update the Current Process' processor state before resuming process' execution */
		if (currentProc != NULL){ /* if there is a Current Process to return control to */
			switchContext(proc); /* calling the function that returns control to the Current Process */
		}
		switchProcess(); /*calling the Scheduler to begin execution of the next process on the Ready Queue (if there is no Current Process to return control to) */
	}

	/* unblockedPcb is not NULL */
	*(unblockedPcb->p_semAdd)++; /* incrementing the value of the semaphore associated with the interrupt as part of the V operation */
	unblockedPcb->p_s.s_v0 = statusCode; /* placing the stored off status code in the newly unblocked pcb's v0 register */
	insertProcQ(&ReadyQueue, unblockedPcb); /* inserting the newly unblocked pcb on the Ready Queue to transition it from a "blocked" state to a "ready" state */
	softBlockCnt--; /* decrementing the value of softBlockCnt, since we have unblocked a previosuly-started process that was waiting for I/O */
	moveState(savedExceptState, &(proc->p_s)); /* update the Current Process' processor state before resuming process' execution */
	if (currentProc != NULL){ /* if there is a Current Process to return control to */
		switchContext(proc); /* calling the function that returns control to the Current Process */
	}
	switchProcess(); /*calling the Scheduler to begin execution of the next process on the Ready Queue (if there is no Current Process to return control to) */
}

 /* Nucleus' device interrupt handler function (exception code 0) */
 void intTrapH(){
 	/* initializing global variables */
 	STCK(curr_tod); /* initializing the current value on the Time of Day clock */
 	remaining_time = getTIMER(); /* initializing the remaining time left on the Current Process' quantum */
	savedExceptState = (state_PTR) BIOSDATAPAGE; /* initializing the saved exception state to the state stored at the start of the BIOS Data Page */

 	/* calling the appropriate interrupt handler function based off the type of the highest-priority interrupt that occurred */
 	if (savedExceptState->s_cause & LINE1INT != ALLOFF){ /* if there is a PLT interrupt (i.e., an interrupt occurred on line 1) */
 		pltTimerInt(currentProc); /* calling the internal function that handles PLT interrupts */
 	}
 	if (savedExceptState->s_cause & LINE2INT != ALLOFF){ /* if there is a System-Wide Interval Timer/Pseudo-clock interrupt (i.e., an interrupt occurred on line 2) */
 		intTimerInt(currentProc); /* calling the internal function that handles System-Wide Interval Timer/Pseudo-clock interrupts */
 	}
 	if (savedExceptState->s_cause & LINE7INT != ALLOFF){ /* if there is an I/O interrupt on a terminal device (i.e., an interrupt occurred on line 7) */
		terminalInt(currentProc); /* calling the internal function that handles I/O interrupts on terminal devices */
 	}
 	IOInt(currentProc); /* otherwise, an I/O interrupt occurred on a non-terminal device (i.e., an interrupt occurred on lines 3-6) */
 }