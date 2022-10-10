/**************************************************************************** 
 *
 * This is where TLB-Refill exception functions are located, which will be called by 
 * the Nucleus in initial.c (in the generalExceptionHandler)
 * (i.e., This module ctonains the functions responsible for SYSCALL exception handling.)
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 * 
 ****************************************************************************/

#include "../h/types.h"
#include "../h/const.h"
#include "../h/asl.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../phase2/initial.c"

/* This function is the event handler for TBL-Refill events
(Process 0's Pass Up Vector's TLB-Refill event handler function)
---- do we need this? defined in p2test.c
 */
void uTLB_RefillHandler(){
    setENTRYHI(KUSEG); 
    setENTRYLO(KSEG0);
    TLBWR();
    LDST((state_PTR) BIOSDATAPAGE);
}

/* Nucleus' TLB exception handler (exception codes 1,2,3) */


/* Nucleus' Program Trap exception handler (exception codes 4-7, 9-12) */


/* Nucleus' general SYSCALL exception handler (exception code 8). 
This function will call a specific SYS function depending on value in a0 by
the process executing the SYSCALL instruction.

A SYSCALL exception occurs when the SYSCALL assembly instruction is executed. */



/************Functions for specific system call codes ***************
 * The code values are specified in registers such as a0 by 
 * the executing process.
*/
waitOp(int *sema4, pcb_PTR p){
    sema4--;
    if(sema4 < 0){
        insertBlocked(&sema4, p);
        scheduler();
    }
    /* return to current proc */
}

signalOp(int *sema4){
    sema4++;
    if(sema4 <= 0){
        pcb_PTR temp = removeBlocked(&sema4);
        insertProcQ(temp, &ReadyQueue);
    }
    /* return to current proc */
}