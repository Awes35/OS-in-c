/**************************************************************************** 
 *
 * This module adds several important components to the development of our
 * Pandos operating system. First off, this module contains the entry point
 * for our program (i.e., main()). main() will, among other tasks, initialize
 * the Phase 1 data structures (i.e., the ASL, free list of semaphore descriptors,
 * and the process queue that we will use to hold processes that are 
 * ready to be executed), initialize four words in the BIOS data page (i.e., one
 * word each for the general exception handler, a corresponding stack pointer, the 
 * TLB-Refill handler, and a corresponding stack pointer), and create one
 * process and call the scheduler on it. We also define the global variables
 * (and initialize them in main()) needed in Phase 2 of development, and we 
 * implement the general exception handler in this module.
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 *
 ****************************************************************************/

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/initial.h"

/* declaring global variables */
pcb_PTR ReadyQueue; /* pointer to the tail of a queue of pcbs that are in the "ready" state */
pcb_PTR currentProc; /* pointer to the pcb that is in the "running" state */
int procCnt; /* integer indicating the number of started, but not yet terminated, processes */
int softBlockCnt; /* integer indicating the number of started, but not yet terminated, processes that're in the "blocked" state" */
int deviceSemaphores[MAXDEVICECNT]; /* array of integer semaphores that correspond to each external (sub) device, plus one semd for the Pseudo-clock */

/* Function that represents the entry point of our program. It initializes the phase 1 data
 * structures (i.e., the ASL, free list of semaphore descriptors, and the process queue that we 
 * we will use to hold processes that are ready to be executed), initializes four words in the 
 * BIOS data page (i.e., one word each for the general exception handler, a corresponding stack 
 * pointer, the TLB-Refill handler, and a corresponding stack pointer), and creates one process
 * and calls the scheduler on it. The function also initializes the global variables for this
 * module. */
int main(){
	
	/* initializing global variables */
	ReadyQueue = mkEmptyProcQ(); /* initializng the ReadyQueue's tail pointer to be NULL */
	currentProc = NULL; /* setting the pointer to the pcb that is in the "running" state to NULL */
	procCnt = 0; /* setting the number of started, but not yet terminated, processes to 0 */
	softBlockCnt = 0; /* setting the number of started, but not yet terminated, processes that're in the "blocked" state to 0 */

	int i;
	for (i = 0; i < 49; i++){
		/* initializing the device semaphores */
		deviceSemaphores[i] = 0;
	}

	/* initializing the free list of semaphore descriptors (along with the dummy nodes for the ASL) 
	and the pcbFree list */
	initPcbs(); /* initializing the pcbFree list */
	initASL(); /* initializing the semdFree list and the ASL's dummy nodes */

	/* instantiating a single process so we can call the scheduler on it */
	pcb_PTR p = allocPcb(); /* instantiating the process */
	/* FINISH INSTANTIATION OF THE PROCESS */
	p->p_s.s_pc = (memaddr) test; /* assigning the PC to the address of test */
	p->p_s.s_t9 = (memaddr) test; /* assigning the address of test to register t9 */

	/* initializng the all of the process tree fields to NULL */
	p->p_prnt = NULL; /* setting the pointer to p's parent to NULL */
	p->p_child = NULL; /* setting the pointer to p's child to NULL */
	p->p_next_sib = NULL; /* setting the pointer to p's next sibling to NULL */
	p->p_prev_sib = NULL; /* setting the pointer to p's previous sibling to NULL */

	/* initializing the remaining pcb fields */
	p->p_time = 0; /* setting p's accumulated time field to zero */
	p->p_semAdd = NULL; /* setting p's blocking address to NULL */
	p->p_supportStruct = NULL; /* setting p's Support Structure pointer to NULL */

	/* placing p into the Ready Queue and incrementing the Process Count */
	insertProcQ(&ReadyQueue, p); /* inserting p into the Ready Queue */
	procCnt++; /* incrementing the Process Count */

	/* calling the scheduler */
	scheduler();
	return SUCCESS;
}
