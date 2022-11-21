/**************************************************************************** 
 *
 * This module implements the TLB exception handler (i.e., The Pager) and contains
 * the functions for reading and writing flash devices. Furthermore, the module
 * also initializes both the Swap Pool table and the accompanying semaphore.
 *  
 * Written by: Kollen Gruizenga and Jake Heyser
 ****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initial.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "/usr/include/umps3/umps/libumps.h"

/* function declarations */

/* declaring variables that are global to this module */
int swapSem; /* mutual exclusion semaphore that controls access to the Swap Pool data structure */
swap_t swapPoolTbl[2 * UPROCMAX]; /* the Swap Pool data structure/table */

/* Function that initializes the Swap Pool table and the Swap Pool semaphore (i.e., the variables that are global
to this module) */
void initSwapStructs(){
	swapSem = 1; /* initializing the Swap Pool semaphore to 1, since it will be used for the purpose of mutual exclusion */

	/* initializing the Swap Pool table */
	int i;
	for (i = 0; j < (2 * UPROCMAX); i++){
		swapPoolTbl[i].asid = EMPTYFRAME; /* initializing the ASID to -1, since all frames are currently unoccupied */
	}
}

/* Function that handles page faults that are passed up by the Nucleus */
void vmTlbHandler(){
}



