/**************************************************************************** 
 *
 * This module serves as the exception handler module for TLB-Refill events
 * and contains the implementation of the functions that were called in intiial.c's
 * generalExceptionHandler() function. (i.e., This module ctonains the functions
 * responsible for SYSCALL exception handling.)
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

/* Function that consitutes Process 0's Pass Up Vector's TLB-Refill event handler function */
void uTLB_RefillHandler(){
	setENTRYHI(KUSEG);
	setENTRYLO(KSEG0);
	TLBWR();
	LDST((state_PTR) BIOSDATAPAGE);
}