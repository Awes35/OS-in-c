#ifndef SYSSUPPORT
#define SYSSUPPORT

/**************************************************************************** 
 *
 * The externals declaration file for the module responsible for implementing
 * phase 3's general exception handler, SYSCALL exception handler, and Program
 * Trap exception handler
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 * 
 ****************************************************************************/

#include "../h/types.h"

extern void vmGeneralExceptionHandler();
extern void programTrapHandler();

#endif