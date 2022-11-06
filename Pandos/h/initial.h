#ifndef INITIAL
#define INITIAL

/**************************************************************************** 
 *
 * The externals declaration file for the Initial.c Global Variables
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 * 
 ****************************************************************************/
#include "../h/const.h"
#include "../h/types.h"


extern int procCnt; /* integer indicating the number of started, but not yet terminated, processes */
extern int softBlockCnt; /* integer indicating the number of started, but not yet terminated, processes that're in the "blocked" state" */
extern pcb_PTR currentProc; /* pointer to the pcb that is in the "running" state */
extern pcb_PTR ReadyQueue; /* pointer to the tail of a queue of pcbs that are in the "ready" state */
extern cpu_t start_tod; /* the value on the time of day clock that the Current Process begins executing at */
extern int deviceSemaphores[MAXDEVICECNT]; /* array of integer semaphores that correspond to each external (sub) device, plus one semd for the Pseudo-clock, located 
									at the last index of the array (PCLOCKIDX). Note that this array will be implemented so that terminal device semaphores are last and terminal device semaphores
									associated with a read operation in the array come before those associated with a write operation. */
extern state_PTR savedExceptState; /* a pointer to the saved exception state */

#endif
