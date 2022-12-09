#ifndef EXCEPTIONS
#define EXCEPTIONS

/**************************************************************************** 
 *
 * The externals declaration file for the TLB-Refill exception handler module
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 * 
 ****************************************************************************/

#include "../h/types.h"

extern void sysTrapH ();
extern void tlbTrapH ();
extern void pgmTrapH ();
extern void updateCurrPcb();
extern void uTLB_RefillHandler();

#endif
