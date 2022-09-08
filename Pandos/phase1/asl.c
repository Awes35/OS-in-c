/* Maintain the free list as a stack */

#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"

/* Declaring global variables */
HIDDEN semd_PTR semd_h; /* ptr to the head of the ASL (semd ACTIVE list) */
HIDDEN semd_PTR semdFree_h; /* ptr to the head of the semaphore descriptors (semd) free list */

/* This helper function searches through the ASL and determines the position where
a node with the given address pointer should belong in the ASL. It returns a pointer
to the semaphore descriptor whose address directly precedes where a semaphore descriptor with the
given address pointer (semAdd) would belong in the ASL. */ 
semd_PTR findSemaphore(int *semAdd){
	semd_t *current, *previous;
	current = semd_h; /* the head of the ASL */
	previous = NULL; /* the ASL is not a circular list, so there is no semd prior to the head of the ASL */
	/* looping through the ASL to determine the location where a semd with the given address pointer would belong */
	while (current != NULL){  /* traversing through the ASL */
		if (current->s_semAdd >= semAdd){ /* IS THIS LINE RIGHT??? */
			/* the address associated with current >= semAdd, so a semd with the address pointer of semAdd would belong in the position of previous */
			break;
		}
		/* if the address associated with current < semAdd, we need to keep moving through the list */
		previous = current; /* incrementing the previous pointer to the next semd in the ASL */
		current = current->s_next; /* incrementing the current pointer to the next semd in the ASL */
	}
	return previous; /* return a pointer to the semd that precedes current */
}

/* This helper function handles adding a new semaphore descriptor to the free list. It will prove
useful in initASL(), removeBlocked() and outBlocked(), where we need to handle cases that involve
removing a semd from the ASL and adding it onto the free list. */
void deallocateSemaphore(semd_PTR sem){
	if (semdFree_h == NULL){ /* the semdFree list is currently empty */
			sem->s_next = NULL;
			semdFree_h = sem;
	}
	else{ /* there's at least one semaphore descriptor on the ASL already */
		sem->s_next = semdFree_h;
		semdFree_h = sem;
	}
}

/* This function inserts the pcb pointed to by p at the tail of the
process queue associated with the semaphore whose physical address is
semAdd and set the semaphore address of p to semAdd. If the semaphore
is currently not active (i.e., there is no descriptor for it in the
ASL), allocate a new descriptor from the semdFree list, insert it
in the ASL (at the appropriate position), initialize all of the fields
(i.e., set s_semAdd to semAdd, and s_procq to mkEmptyProcQ()), and
proceed as above. If a new semaphore descriptor needs to be allocated
and the semdFree list is empty, return TRUE. In all other cases, return
FALSE. */
int insertBlocked(int *semAdd, pcb_PTR p){
	if (((findSemaphore(semAdd))->s_next)->s_semAdd != semAdd){ /* the semaphore is not currently active */
		/* need to remove a semaphore from the free list */
		semd_PTR temp = semdFree_h;
		if (temp == NULL){ /* the semdFree list is empty */
			return TRUE;
		}
		/* At this point, we know the semdFree list is not empty */
		semdFree_h = temp->s_next; /* adjusting the head pointer of the free list */

		/*inserting the new semd into the ASL */
		semd_PTR previous = findSemaphore(semAdd); /* the semd that will directly precede temp in the ASL */
		temp->s_next = previous->s_next;
		previous->s_next = temp;

		temp->s_semAdd = semAdd; /* initializing the remaining fields of the semd that we added to the ASL */
		temp->s_procQ = mkEmptyProcQ();
		insertProcQ(&(temp->s_procQ), p); /* inserting the pcb pointed to by p at the tail of the proc queue associated with temp */
		p->p_semAdd = semAdd; /* setting the semaphore address of p to semAdd */
		return FALSE;
	}
	else{ /* the semaphore is currently in the ASL already */
		semd_PTR prev = findSemaphore(semAdd); /* the semd that directly precedes the semd with the given address in the ASL */
		semd_PTR current = prev->s_next; /* the semd whose physical address is semAdd */
		insertProcQ(&(current->s_procQ), p); /* inserting the pcb pointed to by p at the tail of the proc queue associated with temp */
		p->p_semAdd = semAdd; /* setting the semaphore address of p to semAdd */
		return FALSE;
	}
}

/* This function searches the ASL for a descriptor of this semaphore. If
none is found, return NULL; otherwise, remove the first (i.e., head) pcb
from the process queue of the found semaphore descriptor and return a 
pointer to it. If the process queue for this semaphore becomes empty
(emptyProcQ(s_procq) is TRUE), remove the semaphore descriptor from
the ASL and return it to the semdFree list. */
pcb_PTR removeBlocked(int *semAdd){
	if (((findSemaphore(semAdd))->s_next)->s_semAdd != semAdd){ /* the semaphore is not currently active */
		return NULL;
	}
	semd_PTR prev = (findSemaphore(semAdd)); /* the descriptor that precedes that which has the given address in the ASL */
	semd_PTR s = prev->s_next; /* the descriptor with the given address in the ASL */
	pcb_PTR result = removeProcQ(&(s->s_procQ)); /* the first pcb from the process queue of the found semaphore descriptor */
	if (emptyProcQ(s->s_procQ)){ /* the process queue for the semaphore became empty */
		/* We want to remove s from the ASL and return it to the semdFree list, since its process queue is now empty */
		prev->s_next = s->s_next; /* removing s from the ASL */
		/* adding s to the semdFree list */
		deallocateSemaphore(s);
	}
	return result; /* returning the head pcb from the process queue of the found semaphore descriptor */
}

/* This function removes the pcb pointed to by p from the process queue
associated with p's semaphore (p->p_semAdd) on the ASL. If pcb poined to 
by p does not appear in the process queue associated with p's semaphore,
which is an error condition, return NULL; otherwise, return p. */
pcb_PTR outBlocked(pcb_PTR p){
	if (((findSemaphore(p->p_semAdd))->s_next)->s_semAdd != p->p_semAdd ){ /* p's semaphore is not on the ASL */
		return NULL;
	}
	/* At this point, we know p's semaphore is on the ASL */
	semd_PTR prev = (findSemaphore(p->p_semAdd)); /* the descriptor that precedes that which may or may not contain p */
	semd_PTR s = prev->s_next; /* p's semaphore in the ASL */
	pcb_PTR result = outProcQ(&(s->s_procQ), p); /* the pcb pointed to by p from the process queue of the found semaphore descriptor */
	if (emptyProcQ(s->s_procQ)){ /* the process queue for p's semaphore became empty */
		/* We want to remove s from the ASL and return it to the semdFree list, since its process queue is now empty */
		prev->s_next = s->s_next; /* removing s from the ASL */
		/* adding s to the semdFree list */
		deallocateSemaphore(s);
	}
	return result; /* returning the pcb pointed to by p from the process queue of the found semaphore descriptor */
}

/* This function returns a pointer to the pcb that is at the head of the
process queue associated with the semaphore semAdd. Return NULL if
semAdd is not found on the ASL or if the process queue associated with
semAdd is empty. */
pcb_PTR headBlocked(int *semAdd){
	if (((findSemaphore(semAdd))->s_next)->s_semAdd != semAdd){ /* the descriptor was not found in the ASL */
		return NULL;
	}
	semd_PTR s = (findSemaphore(semAdd))->s_next; /* the descriptor with the given address */
	return headProcQ(s->s_procQ); /* returning a pointer to the head of the process queue associated with semAdd */
}

/* This function initializes the semdFree list to contain all the elements
of the array static semd_t semdTable[MAXPROC]. This method will be only
called once during data structure initialization. It also initializes dummy
nodes at both the head and tail of the ASL to make checking boundary cases
more efficient; namely, we no longer need to check whether the ASL will ever
be empty, because the two dummy nodes will always be in the ASL. Also, the
dummy nodes will prove useful for inserting and deleting as they pertain to
the ASL, since we will never insert before the first dummy node whose address
is LEASTINT, and we will never remove after the last dummy node whose address is MAXINT */
void initASL(){
	static semd_t semdTable[MAXPROC + 2];
	semdFree_h = NULL;

	int i = 0;
	while (i < MAXPROC){
		semd_PTR s = &(semdTable[i]);
		deallocateSemaphore(s);
		i++;
	}

	/* initializing dummy nodes for the ASL (head:0, tail:inf) */
	semd_PTR temp = semdFree_h;
	semdFree_h = temp->s_next; /* removing temp from the free list */
	temp->s_next = NULL; 
	semd_h = temp; /* adding temp to the head of the ASL */

	semd_PTR temp2 = semdFree_h;
	semdFree_h = temp2->s_next; /* removing temp2 from the free list */
	temp2->s_next = NULL;
	temp->s_next = temp2; /* adding temp2 to the tail of the ASL */
	temp2->s_semAdd = (int*) MAXINT; /* setting the address of the tail of the ASL to MAXINT */
	temp->s_semAdd = (int *) LEASTINT; /* setting the address of the head of the ASL to 0 */
}
