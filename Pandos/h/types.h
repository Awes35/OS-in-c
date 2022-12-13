#ifndef TYPES
#define TYPES

/**************************************************************************** 
 *
 * This header file contains utility types definitions.
 * 
 ****************************************************************************/

#include "../h/const.h"

typedef signed int cpu_t;
typedef unsigned int memaddr;


/* Device Register */
typedef struct {
	unsigned int d_status;
	unsigned int d_command;
	unsigned int d_data0;
	unsigned int d_data1;
} device_t;

#define t_recv_status		d_status
#define t_recv_command		d_command
#define t_transm_status		d_data0
#define t_transm_command	d_data1


/* Bus Register Area */
typedef struct {
	unsigned int rambase;
	unsigned int ramsize;
	unsigned int execbase;
	unsigned int execsize;
	unsigned int bootbase;
	unsigned int bootsize;
	unsigned int todhi;
	unsigned int todlo;
	unsigned int intervaltimer;
	unsigned int timescale;
	unsigned int TLB_Floor_Addr;
	unsigned int inst_dev[DEVINTNUM];
	unsigned int interrupt_dev[DEVINTNUM];
	device_t	devreg[DEVINTNUM * DEVPERINT];
} devregarea_t;

/* strucure that represents a single page table entry */
typedef struct pte_entry_t {
	unsigned int entryHI;
	unsigned int entryLO;
} pte_entry_t;

#define STATEREGNUM	31
typedef struct state_t { /* TEMP-- POO pg8 */
	unsigned int	s_entryHI; /* the entryHI register, containing the current ASID --aka processID */
	unsigned int	s_cause; /* Cause CP0 register */
	unsigned int	s_status; /* Status CP0 register */ /* TEMP-- POO pg9 */
	unsigned int 	s_pc; /* PC register */
	int	 			s_reg[STATEREGNUM]; /* 29 words for the GPR registers + HI, LOW registers? */
} state_t, *state_PTR;

/* process context type */
typedef struct context_t {
	/* process context fields */
	unsigned int 	c_stackPtr,	/* stack pointer value */
					c_status,	/* status reg value */
					c_pc;		/* PC address */
} context_t;

/* type representing the table of entries whose frames are in the Swap Pool (i.e., the Swap Pool table)*/
typedef struct swap_t {
	int				asid; 		/* the ASID of the U-Proc whose page is occupying the frame */
	int				pgNo; 		/* the logical page number of the occupying page */
	pte_entry_t		*ownerProc;	/* a pointer to the matching Page Table entry in the Page Table belonging directly to the owner process */
} swap_t;

/* Support structure type */
typedef struct support_t {
	int				sup_asid;				/* process Id (asid) */
	state_t			sup_exceptState[2];		/* stored except states */
	context_t		sup_exceptContext[2];	/* pass up contexts */
	pte_entry_t		sup_privatePgTbl[32];	/* the user process's page table */
	int				sup_stackTLB[500];		/* the stack area for the process' TLB exception handler */
	int				sup_stackGen[500];		/* the stack area for the process' general exception handler */
} support_t;

/* Process control block type */
typedef struct pcb_t {
	/* process queue fields */
	struct pcb_t	*p_next,	/* pointer to next entry */
					*p_prev,	/* pointer to prev entry */
	
	/* process tree fields */
					*p_prnt,	/* pointer to parent */
					*p_child,	/* pointer to first child */
					*p_next_sib,	/* pointer to next sibling */
					*p_prev_sib;	/* pointer to prev sibling */

	/* process status information */
	state_t			p_s;		/* processor state (a 35 word block) */
	cpu_t			p_time;		/* cpu time used by proc */
	int				*p_semAdd;	/* pointer to sema4 on which process blocked */

	/* support layer information */
	support_t	*p_supportStruct; /* ptr to support struct */
} pcb_t, *pcb_PTR;				

/* Semaphore descriptor type */
typedef struct semd_t {
	struct semd_t	*s_next;	/* next element on the ASL */
	int 			*s_semAdd;	/* pointer to the semaphore */
	pcb_t			*s_procQ;	/* tail pointer to a process queue */
} semd_t, *semd_PTR;

/* Pass Up Vector */
typedef struct passupvector_t {
    unsigned int tlb_refll_handler;
    unsigned int tlb_refll_stackPtr;
    unsigned int exception_handler;
    unsigned int exception_stackPtr;
} passupvector_t;

#define	s_at	s_reg[0]
#define	s_v0	s_reg[1]
#define s_v1	s_reg[2]
#define s_a0	s_reg[3]
#define s_a1	s_reg[4]
#define s_a2	s_reg[5]
#define s_a3	s_reg[6]
#define s_t0	s_reg[7]
#define s_t1	s_reg[8]
#define s_t2	s_reg[9]
#define s_t3	s_reg[10]
#define s_t4	s_reg[11]
#define s_t5	s_reg[12]
#define s_t6	s_reg[13]
#define s_t7	s_reg[14]
#define s_s0	s_reg[15]
#define s_s1	s_reg[16]
#define s_s2	s_reg[17]
#define s_s3	s_reg[18]
#define s_s4	s_reg[19]
#define s_s5	s_reg[20]
#define s_s6	s_reg[21]
#define s_s7	s_reg[22]
#define s_t8	s_reg[23]
#define s_t9	s_reg[24]
#define s_gp	s_reg[25]
#define s_sp	s_reg[26]
#define s_fp	s_reg[27]
#define s_ra	s_reg[28]
#define s_HI	s_reg[29]
#define s_LO	s_reg[30]


#endif
