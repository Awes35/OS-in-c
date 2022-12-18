#ifndef INITPROC
#define INITPROC

/**************************************************************************** 
 *
 * The externals declaration file for the module that implements the test()
 * function and declares and initializes the phase 3 global variables, which
 * include an array of device semaphores and the masterSemaphore responsible
 * for ensuring test() comes to a more graceful conclusion by calling HALT()
 * instead of PANIC()
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 * 
 ****************************************************************************/

#include "../h/const.h"
#include "../h/types.h"

extern int masterSemaphore; /* semaphore to be V'd and P'd by test as a means to ensure test terminates in a way so that the PANIC()
							function is not called */
extern int devSemaphores[MAXIODEVICES]; /* array of mutual exclusion semaphores; each potentially sharable peripheral I/O device has one
									semaphore defined for it. Note that this array will be implemented so that terminal device
									semaphores are last and terminal device semaphores associated with a read operation in the array
									come before those associated with a write operation. */
extern void test(); /* function that represents the instantiator process */

#endif
