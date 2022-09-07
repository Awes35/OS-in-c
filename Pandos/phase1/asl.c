/* Maintain the free list as a stack */

#include <stdio.h>
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../phase1/pcb.c"

/* Declaring global variables */
HIDDEN semd_PTR semd_h; // ptr to the head of the ASL (semd ACTIVE list)
HIDDEN semd_PTR semdFree_h; // ptr to the head of the semaphore descriptors (semd) free list

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
int insertBlocked(int *semAdd, pcb_PTR p)
{
	if (findSemaphore(semAdd) == NULL){ /*the semaphore is not currently active*/
		/* need to remove a semaphore from the free list */
		semd_PTR temp = semdFree_h;
		semdFree_h = temp->s_next; // adjusting the head pointer of the free list
		insertSemaphore(temp, semAdd); // inserting the semaphore into the appropriate position in the ASL
		temp->s_semAdd = &semAdd;
		temp->s_procQ = mkEmptyProcQ();
		

	}
	else{ /*the semaphore is currently in the list*/
		semd_PTR s = findSemaphore(semAdd);
		insertProcQ(s->s_procQ, p); // inserting the pcb pointed to by p into the process queue
		return FALSE;
	}
}

/* This function searches the ASL for a descriptor of this semaphore. If
none is found, return NULL; otherwise, remove the first (i.e., head) pcb
from the process queue of the found semaphore descriptor and return a 
pointer to it. If the process queue for this semaphore becomes empty
(emptyProcQ(s_procq) is TRUE), remove the semaphore descriptor from
the ASL and return it to the semdFree list. */
pcb_PTR removeBlocked(int *semAdd)
{
	if (findSemaphore(semAdd) == NULL){ // the descriptor was not found in the ASL
		return NULL;
	}
	semd_PTR s = findSemaphore(semAdd); // the descriptor with the given address
	pcb_PTR result = removeProcQ(s->s_procQ); // the first pcb from the process queue of the found semaphore descriptor IS THIS CALL RIGHT????
	if (emptyProcQ(s->s_procQ)){ // the process queue for the semaphore became empty
		//NOT DONE

	}
}




/* This function removes the pcb pointed to by p from the process queue
associated with p's semaphore (p->p_semAdd) on the ASL. If pcb poined to 
by p does not appear in the process queue associated with p's semaphore,
which is an error condition, return NULL; otherwise, return p. */
pcb_PTR outBlocked(pcb_PTR p)
{
	//NOT DONE
	//if(findSemaphore(semAdd)==NULL){

	//}
}

/* This function returns a pointer to the pcb that is at the head of the
process queue associated with the semaphore semAdd. Return NULL if
semAdd is not found on the ASL or if the process queue associated with
semAdd is empty. */
pcb_PTR headBlocked(int *semAdd)
{
	if (findSemaphore(semAdd) == NULL){ // the descriptor was not found in the ASL
		return NULL;
	}
	semd_PTR s = findSemaphore(semAdd); // the descriptor with the given address
	return headProcQ(s->s_procQ); // returning a pointer to the head of the process queue associated with semAdd
}

/* This function initializes the semdFree list to contain all the elements
of the array static semd_t semdTable[MAXPROC]. This method will be only
called once during data structure initialization. It also initializes dummy
nodes at both the head and tail of the ASL to make checking boundary cases
more efficient. */
void initASL()
{
	static semd_t semdTable[MAXPROC + 2];
	semdFree_h = NULL;

	for (int i=0; i<MAXPROC; i++){
		semd_PTR s = &(semdTable[i]);
		if (semdFree_h == NULL){ // the semdFree list is currently empty
			s->s_next = NULL;
		}
		else{ // there's at least one semaphore descriptor on the ASL already
			s->s_next = semdFree_h;
			semdFree_h = s;
		}
	}

	// initializing dummy nodes for the ASL (head:0, tail:inf)
	semd_PTR temp = semdFree_h;
	semdFree_h = temp->s_next; // removing temp from the free list
	temp->s_next = NULL; 
	semd_h = temp; // adding temp to the head of the ASL

	semd_PTR temp2 = semdFree_h;
	semdFree_h = temp2->s_next; // removing temp2 from the free list
	temp2->s_next = NULL;
	temp->s_next = temp2; // adding temp2 to the tail of the ASL
	temp2->s_semAdd = MAXINT; // setting the address of the tail of the ASL to MAXINT
	temp->s_semAdd = 0; // setting the address of the head of the ASL to 0
}

/* This function searches through the ASL and checks to see whether the semaphore
pointed to by semAdd is in the ASL. If so, it returns the semaphore with the address
pointed to by semAdd; if not, it returns NULL. */
semd_PTR findSemaphore(int *semAdd){
	semd_t *current;
	if (semd_h == NULL){ // then the ASL is empty
		return NULL;
	}
	// the ASL is not empty
	current = semd_h; // the head of the ASL
	while (current != NULL){
		if (current->s_semAdd == &semAdd){ // if we've arrived at the semaphore with the given address
			return current;
		}
		current = current->s_next;
	}
	return NULL; // the desired semaphore is not in the ASL
}

void insertSemaphore(semd_PTR s, int *semAdd){
	semd_t *current;
	current = semd_h; // the head of the ASL
	//if(){

	//}
}