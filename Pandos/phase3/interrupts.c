/**************************************************************************** 
 *
 * This module serves as the interrupt handler module and contains the implementation
 * of the functions responsible for interrupt exception handling that are called by
 * the Nucleus. More specifically, the module contains the function intTrapH(),
 * which is the entry point to this module. intTrapH(), after initializing the variables
 * that are global to this module, determines what line number the highest-priority
 * pending interrupt is located on, and then it invokes the internal function that is
 * responsible for handling the type of interrupt that occurred, as revealed by the
 * line number of the highest-priority pending interrupt. Note that (if it has not been
 * implied already) if more than one interrupt is pending, we handle each interrupt
 * one-at-a-time, resolving the interrupts in the order of their priority.
 *
 * Note that for the purposes of this phase of development, the time spent
 * handling the interrupt is charged to the process responsible for generating the interrupt.
 * More specifically, if an I/O interrupt was the highest-priority pending interrupt, then 
 * the time spent handling the interrupt is charged to the interrupting process, NOT to the 
 * Current Process which simply happened to be executing when the interrupt was generated. This
 * decision wade made because it seems that charging such CPU time to the process
 * that generated the interrupt is the most logical way to account for the CPU time
 * used in this module, as the Current Process, after all, really had nothing to do
 * with the interrupt being generated. If a PLT interrupt occurred, the time between
 * when the Current Process most recently began executing on the CPU and the time that the
 * interrupt occurred is, naturally, charged to the Current Process, and the time spent 
 * handling the PLT interrupt is also charged to the Current Process, since that process
 * is responsible for the CPU time spent handling the interrupt. Likewise, if a System-
 * wide Interval Timer interrupt occurred, we will charge the Current Process with the
 * time between when the Current Process most recently began executing on the CPU and
 * the time that the interrupt occurred. However, we will refrain from charging any process
 * with the time spent handling the interrupt, because it doesn't make sense to charge the
 * Current Process with this time, as it just so happened to be the process that was
 * executing when the System-wide Interval Timer reached zero. But, in general, the
 * time spent in this module will charged to the responsible process. 
 * 
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 ****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/interrupts.h"
#include "../h/exceptions.h"
#include "../h/initial.h"
#include "/usr/include/umps3/umps/libumps.h"

/* Function declarations */
HIDDEN void pltTimerInt();
HIDDEN void intTimerInt();
HIDDEN void IOInt();
HIDDEN int findDeviceNum(int lineNumber);

/* Declaring variables that are global to this module */
cpu_t interrupt_tod; /* the value on the Time of Day clock when the Interrupt Handler module is first entered */
cpu_t remaining_time; /* the amount of time left on the Current Process' quantum when the interrupt was generated */

/* Internal helper function responsible for determining what device number the highest-priority interrupt occurred on. The function
returns that number to the caller. */
int findDeviceNum(int lineNumber){
	/* declaring local variables */
	devregarea_t *temp; /* device register area that we can use to determine which device number is associated with the highest-priority interrupt */
	unsigned int bitMap; /* the 32-bit contents of the Interrupt Devices Bit Map associated with the line number that was passed in as a parameter */

	/* initializing temp and bitMap in order to determine which device number is associated with the highest-priority interrupt */
	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
	bitMap = temp->interrupt_dev[lineNumber - OFFSET]; /* initialization of bitMap */

	/* determining which device number is associated with the highest-priority interrupt based on the address of bitMap */
	if ((bitMap & DEV0INT) != ALLOFF){ /* if there is a pending interrupt associated with device 0 */
		return DEV0; /* returning 0 to the user, since the highest-priority interrupt is associated with device 0 */
	}
	if ((bitMap & DEV1INT) != ALLOFF){ /* if there is a pending interrupt associated with device 1 */
		return DEV1; /* returning 1 to the user, since the highest-priority interrupt is associated with device 1 */
	}
	if ((bitMap & DEV2INT) != ALLOFF){ /* if there is a pending interrupt associated with device 2 */
		return DEV2; /* returning 2 to the user, since the highest-priority interrupt is associated with device 2 */
	}
	if ((bitMap & DEV3INT) != ALLOFF){ /* if there is a pending interrupt associated with device 3 */
		return DEV3; /* returning 3 to the user, since the highest-priority interrupt is associated with device 3 */
	}
	if ((bitMap & DEV4INT) != ALLOFF){ /* if there is a pending interrupt associated with device 4 */
		return DEV4; /* returning 4 to the user, since the highest-priority interrupt is associated with device 4 */
	}
	if ((bitMap & DEV5INT) != ALLOFF){ /* if there is a pending interrupt associated with device 5 */
		return DEV5; /* returning 5 to the user, since the highest-priority interrupt is associated with device 5 */
	}
	if ((bitMap & DEV6INT) != ALLOFF){ /* if there is a pending interrupt associated with device 6 */
		return DEV6; /* returning 6 to the user, since the highest-priority interrupt is associated with device 6 */
	}
	
	/* otherwise, there is a pending interrupt associated with device 7, since there are only eight devices */
	return DEV7; /* returning 7 to the user, since the highest-priority interrupt is associated with device 7 */
}

/* Internal helper function that handles Processor Local Timer (PLT) interrupts. More specifically, the function copies the processor state
at the time of the exception (which is located at the start of the BIOS Data Page) into the Current Process' pcb, places the Current Process
on the Ready Queue, updates the CPU time of the Current Process so that it includes the time between when it last started executing and 
when the function finished handling the interrupt, because, due to our timing policy described in the module-level documentation, all of
this time will be charged to the Current Process. Once all of this has been accomplished, the function calls the Scheduler so that another process
can begin executing. */
void pltTimerInt(){
	/* delcaring local variables */
	cpu_t curr_tod; /* variable to hold the current TOD clock value */

	if (currentProc != NULL){ /* if there was a running process when the interrupt was generated */
		setTIMER(NEVER); /* loading the PLT with a very large value as we have acknowledged it (don't call PLT again) */
		updateCurrPcb(); /* moving the updated saved exception state from the BIOS Data Page into the Current Process' processor state */
		STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
		currentProc->p_time = currentProc->p_time + (curr_tod - start_tod); /* updating the accumulated processor time used by the Current Process */
		insertProcQ(&ReadyQueue, currentProc); /* placing the Current Process back on the Ready Queue because it has not completed its CPU Burst */
		currentProc = NULL; /* setting Current Process to NULL, since there is no process currently executing */
		switchProcess(); /* calling the Scheduler to begin execution of the next process on the Ready Queue */
	}
	PANIC(); /* otherwise, Current Process is NULL, so the function invokes the PANIC() function to stop the system and print a warning message on terminal 0 */
}

/* Internal helper function that handles interrupts generated by the System-wide Interval Timer. More specifically, the function
acknowledges the interrupt by loading the Interval Timer with a new value (100 milliseconds), unblocks ALL pcbs blocked on the 
Pseudo-clock semaphore, resets the Pseudo-clock semaphore to zero, and returns control to the Current Process so that it can continue
executing (with the same amount of time left on the PLT as there was when the interrupt first occurred). Accordingly, the function
also decrements the Soft-block Count, since a started (but not finished) process has now been unblocked, and it updates the accumulated
CPU time of the Curernt Process so that includes the time between when it last started executing and when the interrupt first occurred.
As stated in the module-level documentation, we will refrain from charging the Current Process (or any process at all) with the time
spent handling this interrupt, since the Current Process is not actually using this CPU time to execute its own process. */
void intTimerInt(){
	/* declaring local variables */
	pcb_PTR temp; /* a pointer to a pcb in the Pseudo-Clock semaphore's process queue that we wish to unblock and insert into the Ready Queue */
	
	LDIT(INITIALINTTIMER); /* placing 100 milliseconds back on the Interval Timer for the next Pseudo-clock tick */
	
	/* unblocking all pcbs blocked on the Pseudo-Clock semaphore */
	while (headBlocked(&deviceSemaphores[PCLOCKIDX]) != NULL){ /* while the Pseudo-Clock semaphore has a blocked pcb */
		temp = removeBlocked(&deviceSemaphores[PCLOCKIDX]); /* unblock the first (i.e., head) pcb from the Pseudo-Clock semaphore's process queue */
		insertProcQ(&ReadyQueue, temp); /* placing the unblocked pcb back on the Ready Queue */
		softBlockCnt--; /* decrementing the number of started, but not yet terminated, processes that are in a "blocked" state */
	}
	deviceSemaphores[PCLOCKIDX] = INITIALPCSEM; /* resetting the Pseudo-clock semaphore to zero */
	if (currentProc != NULL){ /* if there is a Current Process to return control to */
		setTIMER(remaining_time); /* setting the PLT to the remaining time left on the Current Process' quantum when the interrupt handler was first entered*/
		updateCurrPcb(); /* update the Current Process' processor state before resuming process' execution */
		currentProc->p_time = currentProc->p_time + (interrupt_tod - start_tod); /* updating the accumulated processor time used by the Current Process */
		switchContext(currentProc); /* calling the function that returns control to the Current Process */
	}
	switchProcess(); /* if there is no Current Process to return control to, call the Scheduler function to begin executing a new process */
}

/* Internal helper function that handles I/O interrupts on both terminal and non-terminal devices. In other words, this function handles interrupts that
occurred on lines 3-7, as indicated in the Cause register. Note that on terminal devices, I/O interrupts that involve writing take priority over interrupts that
involve reading. From a slightly more narrow level, this function calculates the line number and device number that the highest-priority interrupt occurred on,
saves off the status code from the device's device register, acknowledges the outstanding interrupt by writing the acknowledge command code in the interrupting
device's device register, and peforms a V operation on the Nucleus maintained semaphore associated with this device. Then, it places the stored off status code
in the newly unblocked pcb's v0 register, inserts the newly unblobked pcb on the Ready Queue, and then updates the CPU time of the Current Process so that it
includes the time that it spent executing up until when the interrupt first occurred, and then it charges the time spent handling the interrupt to the 
process responsible for generating the I/O interrupt (if it is not NULL), as described in the module-level documentation for this module. */
void IOInt(){
	/* declaring local variables */
	cpu_t curr_tod; /* variable to hold the current TOD clock value */
	int lineNum; /* the line number that the highest-priority interrupt occurred on */
	int devNum; /* the device number that the highest-priority interrupt occurred on */
	int index; /* the index in devreg of the device associated with the highest-priority interrupt */
	devregarea_t *temp; /* device register area that we can use to determine the status code from the device register associated with the highest-priority interrupt */
	int statusCode; /* the status code from the device register associated with the device that corresponds to the highest-priority interrupt */
	pcb_PTR unblockedPcb; /* the pcb which originally initiated the I/O request */

	/* determining exactly which line number the interrupt occurred on so we can initialize lineNum */
	if (((savedExceptState->s_cause) & LINE3INT) != ALLOFF){ /* if there is an interrupt on line 3 */
		lineNum = LINE3; /* initializing lineNum to 3, since the highest-priority interrupt occurred on line 3 */
	}
	else if (((savedExceptState->s_cause) & LINE4INT) != ALLOFF){ /* if there is an interrupt on line 4 */
		lineNum = LINE4; /* initializing lineNum to 4, since the highest-priority interrupt occurred on line 4 */
	}
	else if (((savedExceptState->s_cause) & LINE5INT) != ALLOFF){ /* if there is an interrupt on line 5 */
		lineNum = LINE5; /* initializing lineNum to 5, since the highest-priority interrupt occurred on line 5 */
	}
	else if (((savedExceptState->s_cause) & LINE6INT) != ALLOFF){ /* if there is an interrupt on line 6 */
		lineNum = LINE6; /* initializing lineNum to 6, since the highest-priority interrupt occurred on line 6 */
	}
	else{ /* otherwise, there is an interrupt on line 7 */
		lineNum = LINE7; /* initializing lineNum to 7, since the highest-priority interrupt occurred on line 7 */
	}

	/* initializing remaining local variables, except for unblockedPcb and curr_tod, which will be initialized later on */
	devNum = findDeviceNum(lineNum); /* initializing devNum by calling the internal function that returns the device number associated with the highest-priority interrupt */	
	index = ((lineNum - OFFSET) * DEVPERINT) + devNum; /* initializing the index in deviceSemaphores of the device associated with the highest-priority interrupt */
	temp = (devregarea_t *) RAMBASEADDR; /* initialization of temp */
	
	if ((lineNum == LINE7) && (((temp->devreg[index].t_transm_status) & STATUSON) != READY)){ /* if the highest-priority interrupt occurred on line 7 and if the device's status code is not 1, meaning the device is not "Ready" */
		/* the interrupt is associated with a terminal device and is a write interrupt */
		statusCode = temp->devreg[index].t_transm_status; /* initializing the status code from the device register associated with the device that corresponds to the highest-priority interrupt */
		temp->devreg[index].t_transm_command = ACK; /* acknowledging the outstanding interrupt by writing the acknowledge command code in the interrupting device's device register */
		unblockedPcb = removeBlocked(&deviceSemaphores[index + DEVPERINT]); /* initializing unblockedPcb by unblocking the semaphore associated with the interrupt and returning the corresponding pcb */
		deviceSemaphores[index + DEVPERINT]++; /* incrementing the value of the semaphore associated with the interrupt as part of the V operation */
	}
	else{ /* otherwise, the highest-priority interrupt either did not occur on a terminal device or it was a read interrupt on a terminal device */
		statusCode = temp->devreg[index].t_recv_status; /* initializing the status code from the device register associated with the device that corresponds to the highest-priority interrupt */
		temp->devreg[index].t_recv_command = ACK; /* acknowledging the outstanding interrupt by writing the acknowledge command code in the interrupting device's device register */
		unblockedPcb = removeBlocked(&deviceSemaphores[index]); /* initializing unblockedPcb by unblocking the semaphore associated with the interrupt and returning the corresponding pcb */
		deviceSemaphores[index]++; /* incrementing the value of the semaphore associated with the interrupt as part of the V operation */
	}
	
	if (unblockedPcb == NULL){ /* if the supposedly unblocked pcb is NULL, we want to return control to the Current Process */
		if (currentProc != NULL){ /* if there is a Current Process to return control to */
			updateCurrPcb(); /* update the Current Process' processor state before resuming process' execution */
			currentProc->p_time = currentProc->p_time + (interrupt_tod - start_tod); /* updating the accumulated processor time used by the Current Process */
			setTIMER(remaining_time); /* setting the PLT to the remaining time left on the Current Process' quantum when the interrupt handler was first entered*/
			switchContext(currentProc); /* calling the function that returns control to the Current Process */
		}
		switchProcess(); /*calling the Scheduler to begin execution of the next process on the Ready Queue (if there is no Current Process to return control to) */
	}
	
	/* unblockedPcb is not NULL */
	unblockedPcb->p_s.s_v0 = statusCode; /* placing the stored off status code in the newly unblocked pcb's v0 register */
	insertProcQ(&ReadyQueue, unblockedPcb); /* inserting the newly unblocked pcb on the Ready Queue to transition it from a "blocked" state to a "ready" state */
	softBlockCnt--; /* decrementing the value of softBlockCnt, since we have unblocked a previosuly-started process that was waiting for I/O */
	if (currentProc != NULL){ /* if there is a Current Process to return control to */
		updateCurrPcb(); /* update the Current Process' processor state before resuming process' execution */
		setTIMER(remaining_time); /* setting the PLT to the remaining time left on the Current Process' quantum when the interrupt handler was first entered*/
		currentProc->p_time = currentProc->p_time + (interrupt_tod - start_tod); /* updating the accumulated processor time used by the Current Process */
		STCK(curr_tod); /* storing the current value on the Time of Day clock into curr_tod */
		unblockedPcb->p_time = unblockedPcb->p_time + (curr_tod - interrupt_tod); /* charging the process associated with the I/O interrupt with the CPU time needed to handle the interrupt */
		switchContext(currentProc); /* calling the function that returns control to the Current Process */
	}
	switchProcess(); /*calling the Scheduler to begin execution of the next process on the Ready Queue (if there is no Current Process to return control to) */
}

/* Function that represents the entry point into this module when handling interrupts. This function's tasks include initializing the
global variables in this module, and identifying the type of interrupt that has the highest priority, so that it can then invoke
the internal function that handles that specific type of interrupt. */
void intTrapH(){
 	/* initializing variables that are global to this module, as well as savedExceptState */
	STCK(interrupt_tod); /* storing the value on the Time of Day clock when the Interrupt Handler module is first entered into interrupt_tod */
 	remaining_time = getTIMER(); /* storing the remaining time left on the Current Process' quantum into remaining_time */
	savedExceptState = (state_PTR) BIOSDATAPAGE; /* initializing the saved exception state to the state stored at the start of the BIOS Data Page */

 	/* calling the appropriate interrupt handler function based off the type of the interrupt that has the highest priority */
 	if (((savedExceptState->s_cause) & LINE1INT) != ALLOFF){ /* if there is a PLT interrupt (i.e., an interrupt occurred on line 1) */
 		pltTimerInt(); /* calling the internal function that handles PLT interrupts */
 	}
 	if (((savedExceptState->s_cause) & LINE2INT) != ALLOFF){ /* if there is a System-Wide Interval Timer/Pseudo-clock interrupt (i.e., an interrupt occurred on line 2) */
 		intTimerInt(); /* calling the internal function that handles System-Wide Interval Timer/Pseudo-clock interrupts */
 	}
 	IOInt(); /* otherwise, an I/O interrupt occurred (i.e., an interrupt occurred on lines 3-7) */
 }
