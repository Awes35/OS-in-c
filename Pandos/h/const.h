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
#define TODLOADDR		0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024
#define	INTDEVICEADDR	0x10000040
#define	DEVADDRBASE		0x10000054

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
#define CHARTRANSM    5

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1
#define	READBLK				2
#define	WRITEBLK			3

/* printer and terminal device unique COMMAND codes */
#define PRINTCHR            2
#define TRANSMITCHAR        2

/* Memory related constants */
#define KSEG0           0x00000000 /* the installed EPROM - BIOS memory region */
#define KSEG1           0x20000000 /* the start of kernel n OS */
#define KSEG2           0x40000000 
#define KUSEG           0x80000000
#define VPNSTART        0x80000    /* constant for KUSEG so that no bits get lost when writing an EntryHI field */
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

/* Maximum string length for transmitting to device */
#define MAXSTRLEN    128

/* Maximum number of external (sub)devices in UMPS3, plus one additional semaphore to support
the Pseudo-clock */
#define MAXDEVICECNT	49

/* Address for initializing Process 0's Pass Up Vector's fields for the address of handling general exceptions and TLB-Refill events */
#define PROC0STACKPTR	0x20001000

/* Value that the system-wide Interval Timer is initialized to 100ms (100,000 microseconds) */
#define INITIALINTTIMER	100000

/* Constants representing the initial values of softBlockCount, Process Count (i.e., procCnt), the semaphores in the deviceSemaphores array and
the accumulated CPU time field for a process that is instantiated */
#define	INITIALPROCCNT		0			/* the initial value of procCnt */
#define	INITIALSFTBLKCNT	0			/* the initial value of softBlockCnt */
#define	INITIALDEVSEMA4		0			/* the initial value of the device semaphores */
#define INITIALACCTIME	  0     /* Value to set the accumulated time field for a process that is instantiated */

/* Value to */
#define PCLOCKIDX       (MAXDEVICECNT - 1)

/* Processor State--Status register constants */
#define ALLOFF			0x0     	/* every bit in the Status register is set to 0; this will prove helpful for bitwise-OR operations */
#define USERPON			0x00000008	/* constant for setting the user-mode on after LDST (i.e., KUp (bit 3) = 1) */
#define IEPON			0x00000004	/* constant for enabling interrupts after LDST (i.e., IEp (bit 2) = 1) */
#define IECON			0x00000001	/* constant for enabling the global interrupt bit (i.e., IEc (bit 0) = 1) */
#define PLTON			0x08000000	/* constant for enabling PLT (i.e., TE (bit 27) = 1) */
#define IMON			0x0000FF00	/* constant for setting the Interrupt Mask bits to on so interrupts are fully enabled */
#define	IECOFF			0xFFFFFFFE	/* constant for disabling the global interrupt bit (i.e., IEc (bit 0) = 0) */

/* Value that the processor's Local Timer (PLT) is intialized to 5 milliseconds (5,000 microseconds) */
#define INITIALPLT		5000

/* Cause register constants for generalExceptionHandler */
#define GETEXCEPCODE	0x0000007C	/* constant for setting all bits to 0 in the Cause register except for the ExcCode field */
#define CAUSESHIFT		2			/* number of bits needed to shift the ExcCode field over to the right so that we can read the ExcCode directly */
#define INTCONST		0			/* exception code signaling an interrupt occurred */
#define TLBCONST		3			/* upper bound on the exception codes that signal a TLB exception occurred */
#define SYSCONST		8			/* exception code signaling a SYSCALL occurred */

/* Constants for returning values in v0 to the caller */
#define ERRORCONST		-1			/* constant denoting an error occurred in the caller's request */
#define SUCCESSCONST	0			/* constant denoting that the caller's request completed successfully */

/* Constant to help determine the index in deviceSemaphores/devSemaphores and in the Interrupt Devices Bitmap that a particular device is located at. 
This constant is subtracted from the line number (or 4, in the case of backing store management), since interrupt lines 3-7 are used for peripheral devices  */
#define	OFFSET			3	

/* Cause register constant for setting all bits to 1 in the Cause register except for the ExcCode field, which is set to 10 for the RI code */
#define	RESINSTRCODE	0xFFFFFF28

/* Cause register constants to help determine which line the highest-priority interrupt is located at */
#define	LINE1INT		0x00000200		/* constant for setting all bits to 0 in the Cause register except for bit 9, which is tied to line 1 interrupts */
#define	LINE2INT		0x00000400		/* constant for setting all bits to 0 in the Cause register except for bit 10, which is tied to line 2 interrupts */
#define	LINE3INT		0x00000800		/* constant for setting all bits to 0 in the Cause register except for bit 11, which is tied to line 3 interrupts */
#define	LINE4INT		0x00001000		/* constant for setting all bits to 0 in the Cause register except for bit 12, which is tied to line 4 interrupts */
#define	LINE5INT		0x00002000		/* constant for setting all bits to 0 in the Cause register except for bit 13, which is tied to line 5 interrupts */
#define	LINE6INT		0x00004000		/* constant for setting all bits to 0 in the Cause register except for bit 14, which is tied to line 6 interrupts */
#define	LINE7INT		0x00008000		/* constant for setting all bits to 0 in the Cause register except for bit 15, which is tied to line 7 interrupts */

/* Constants for the different line numbers an interrupt may occur on */
#define	LINE1			1				/* constant representing line 1 */
#define	LINE2			2				/* constant representing line 2 */
#define	LINE3			3				/* constant representing line 3 */
#define	LINE4			4				/* constant representing line 4 */
#define	LINE5			5				/* constant representing line 5 */
#define	LINE6			6				/* constant representing line 6 */
#define	LINE7			7				/* constant representing line 7 */

/* Constants to help determine which device the highest-priority interrupt occurred on */
#define	DEV0INT			0x00000001		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 0, which is tied to device 0 interrupts */
#define	DEV1INT			0x00000002		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 1, which is tied to device 1 interrupts */
#define	DEV2INT			0x00000004		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 2, which is tied to device 2 interrupts */
#define	DEV3INT			0x00000008		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 3, which is tied to device 3 interrupts */
#define	DEV4INT			0x00000010		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 4, which is tied to device 4 interrupts */
#define	DEV5INT			0x00000020		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 5, which is tied to device 5 interrupts */
#define	DEV6INT			0x00000040		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 6, which is tied to device 6 interrupts */
#define	DEV7INT			0x00000080		/* constant for setting all bits in the Interrupting Devices Bit Map to 0, except for bit 7, which is tied to device 7 interrupts */

/* Constants for the different device numbers an interrupt may occur on */
#define	DEV0			0				/* constant representing device 0 */
#define	DEV1			1				/* constant representing device 1 */
#define	DEV2			2				/* constant representing device 2 */
#define	DEV3			3				/* constant representing device 3 */
#define	DEV4			4				/* constant representing device 4 */
#define	DEV5			5				/* constant representing device 5 */
#define	DEV6			6				/* constant representing device 6 */
#define	DEV7			7				/* constant representing device 7 */

/* Constants signifying the first index of the deviceSemaphores array (We can get the last index of the array by saying MAXDEVICECNT - 1.) */
#define	FIRSTDEVINDEX	0			

/* Constants representing the Syscall Numbers in Pandos */
#define	SYS1NUM			1		
#define	SYS2NUM			2
#define	SYS3NUM			3
#define	SYS4NUM			4
#define	SYS5NUM			5
#define	SYS6NUM			6
#define	SYS7NUM			7
#define	SYS8NUM			8
#define SYS9NUM         9
#define SYS10NUM        10
#define SYS11NUM        11
#define SYS12NUM        12
#define SYS13NUM        13

/* Constant representing the lower bound on which we unblock semaphores and remove them from the ASL */
#define	SEMA4THRESH		0

/* Constant representing the initial value of the Pseudo-clock semaphore */
#define	INITIALPCSEM	0

/* Constant defining a large value to load the PLT with in switchProcess() when the Process Count and Soft Block Count are both greater than zero */
#define NEVER			0xFFFFFFFF

/* Constant that represents when the first four bits in a terminal device's device register's status field are turned on */
#define	STATUSON		0x0F

/* Phase 3 constant that defines how many user processes can be running at once */
#define UPROCMAX		1

/* Constant representing how many page table entries there are per page table */
#define	ENTRIESPERPG	32

/* TLB constants to help retrieve the virtual page number (VPN) of a TLB entry */
#define GETVPN			0xFFFFF000		/* Constant for setting all of the non-VPN bits in a TLB entry to 0 */
#define	VPNSHIFT		12				/* Number of bits needed to shift the VPN field of EntryLo over to the right so that we can read the VPN directly */

/* Constant that represents the number of sharable peripheral I/O devices */
#define	MAXIODEVICES	48		


/* Constant that represents the ASID of a frame is unoccupied */
#define	EMPTYFRAME		-1

/* Constant that represents the top of the stack for handling general exceptions and TLB exceptions, with each of these handlers having their own stack area */
#define	TOPOFSTACK		499

/* Constants that denote important addresses when initializing a U-proc's initial processor state */
#define	UPROCPC			0x800000B0		/* the address of a U-proc's PC; this address is the address of the start of the .text section */
#define	UPROCSP			0xC0000000		/* the address of a U-proc's stack pointer */

/* Constant that represents the number of bits needd to shift the ASID field of EntryHi over to the left when initializing that field */
#define	ASIDSHIFT		6

/* Constant that represents the initial contents of the VPN field for the stack page in a U-proc's Page Table */
#define	STACKPGVPN		0xBFFFF		

/* Constants that help initialize the EntryLo fields in a U-proc's page table */
#define	DBITON			0x00000400		/* Constant for setting all of the bits to 0 in the EntryLo portion of a TLB entry except for the D bit (i.e., D (bit 10) = 1) */

/* Constant that represents the maximum number of frames in phase 3 */
#define	MAXFRAMECNT		2 * UPROCMAX

/* Constant representing the exception code that signfifies that a TLB-Modification Exception occurred */
#define	TLBMODEXCCODE	1

/* Constant for setting all of the bits to 1 in the EntryLo portion of a TLB entry except for the V bit */
#define	VBITOFF			0xFFFFFD

/* Constants to signify whether one wishes to read or write to a flash device; these constants serve as potential parameters to the flashOperation() function in vmSupport.c */
#define	WRITE			0				/* Constant that represents the parameter to flashOperation() for writing a flash device */
#define	READ			1 				/* Constant that represents the parameter to flashOperation() for reading a flash device */

/* Constant that represents the Swap Pool's starting address */
#define	SWAPPOOLADDR	0x20020000

/* Constant that represents the number of bits needed to shift a flash device's block number over to the left */
#define	BLKNUMSHIFT		8				

/* Constant for setting all of the bits to 0 in the EntryLo portion of a TLB entry except for the V bit */
#define	VBITON			0x00000200		

/* Constant that represents the number of bits needed to shift a terminal device's TRANSM_COMMAND.TRANSM-CHAR field over to the left */
#define TERMSHIFT   8

#endif
