#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE		  4096			/* page size in bytes	*/
#define WORDLEN			  4				  /* word size in bytes	*/


/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		0x10000000
#define RAMBASESIZE		0x10000004
#define TODLOADDR		  0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024


/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			  static
#define EOS				    '\0'

#define NULL 			    ((void *)0xFFFFFFFF)

/* device interrupts */
#define DISKINT			  3
#define FLASHINT 		  4
#define NETWINT 		  5
#define PRNTINT 		  6
#define TERMINT			  7

#define DEVINTNUM		  5		  /* interrupt lines used by devices */
#define DEVPERINT		  8		  /* devices per interrupt line */
#define DEVREGLEN		  4		  /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	  16 		/* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS			  0
#define COMMAND			  1
#define DATA0			    2
#define DATA1			    3

/* device register field number for terminal devices */
#define RECVSTATUS  	0
#define RECVCOMMAND 	1
#define TRANSTATUS  	2
#define TRANCOMMAND 	3

/* device common STATUS codes */
#define UNINSTALLED		0
#define READY			    1
#define BUSY			    3

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1

/* Memory related constants */
#define KSEG0           0x00000000
#define KSEG1           0x20000000
#define KSEG2           0x40000000
#define KUSEG           0x80000000
#define RAMSTART        0x20000000
#define BIOSDATAPAGE    0x0FFFF000
#define	PASSUPVECTOR	  0x0FFFF900

/* Exceptions related constants */
#define	PGFAULTEXCEPT	  0
#define GENERALEXCEPT	  1


/* operations */
#define	MIN(A,B)		((A) < (B) ? A : B)
#define MAX(A,B)		((A) < (B) ? B : A)
#define	ALIGNED(A)		(((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))


/* Maximum number of concurrent processes */
#define MAXPROC     20

/* Maximum int (fixed value for 'inf') */
#define MAXINT		0x0FFFFFFF

/* Minimum int field for the value of a semaphore address in the ASL */
#define LEASTINT	0x00000000

/* Maximum number of external (sub)devices in UMPS3, plus one additional semaphore to support
the Pseudo-clock */
#define MAXDEVICECNT	49

/* Address for initializing Process 0's Pass Up Vector's fields for the address of handling general exceptions and TLB-Refill events */
#define PROC0STACKPTR	0x20001000

/* Value that the system-wide Interval Timer is initialized to (in milliseconds) */
#define INITIALINTTIMER	100

/* Value to set the accumulated time field for the first process that is instantiated */
#define INITIALACCTIME	0

/* Processor State--Status register constants */
#define ALLOFF			0x00000000	/* every bit in the Status register is set to 0; this will prove helpful for bitwise-OR operations */
#define USERPON			0x00000008	/* constant for setting the user-mode on after LDST (i.e., KUp (bit 3) = 1) */
#define IEPON			0x00000004	/* constant for enabling interrupts after LDST (i.e., IEp (bit 2) = 1) */
#define IECON			0x00000001	/* constant for enabling interrupts (i.e., IEc (bit 0) = 1) */
#define PLTON			0x08000000	/* constant for enabling PLT (i.e., TE (bit 27) = 1) */
#define PLTOFF			0xF7FFFFFF 	/* constant for disabling the PLT (i.e., TE (bit 27) = 0) */
#define KERNON			0x00000000	/* constant for setting kernel-mode on after LDST (i.e., KUp (bit 3) = 0) */

/* Value that the processor's Local Timer (PLT) is intialized to (in milliseconds) */
#define INITIALPLT		5

/* Cause register constants for generalExceptionHandler */
#define GETEXCEPCODE	0x0000007C	/* constant for setting all bits to 0 in the Cause register except for the ExcCode field */
#define CAUSESHIFT		2			/* number of bits needed to shift the ExcCode field over to the right so that we can read the ExcCode directly */
#define INTCONST		0			/* exception code signaling an interrupt occurred */
#define TLBCONST		3			/* upper bound on the exception codes that signal a TLB exception occurred */
#define SYSCONST		8			/* exception code signaling a SYSCALL occurred */

#endif