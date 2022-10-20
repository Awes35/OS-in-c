#ifndef SCHEDULER
#define SCHEDULER

/**************************************************************************** 
 *
 * The externals declaration file for the Scheduler module
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 * 
 ****************************************************************************/

extern void switchProcess ();
extern void switchContext (pcb_PTR curr_proc);
extern void moveState (state_PTR source, state_PTR dest);

#endif