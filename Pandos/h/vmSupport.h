#ifndef VMSUPPORT
#define VMSUPPORT

/**************************************************************************** 
 *
 * The externals declaration file for the module containing the TLB exception
 * handler, the functions for reading and writing flash devices, and the function
 * (initSwapStructs) which initializes both the Swap Pool table and the accompanying
 * semaphore
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 * 
 ****************************************************************************/

#include "../h/types.h"

extern void initSwapStructs();
extern void vmTlbHandler();
extern void setInterrupts(int onOrOff);
extern void switchUContext(state_PTR returnState);
extern void mutex(int opCode, int *semaphore); 

#endif
