/**************************************************************************** 
 *
 * This module implements phase 3's general exception handler, the SYSCALL
 * exception handler, and the Program Trap exception handler.
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
#include "/usr/include/umps3/umps/libumps.h"

/* function declarations */

/* declaring variables that are global to this module */