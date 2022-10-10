/**************************************************************************** 
 *
 * This module handles the allocation and deallocation of pcbs, 
 *	process queue maintenance, and process tree maintenance
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 *
 ****************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"

/* Declaring global variables */
HIDDEN pcb_PTR pcbFree_h; /* ptr to head of the free list */

/*****Functions that support the allocation and deallocation of pcbs*********
 * 
 * Since Pandos supports no more than MAXPROC concurrent processes, we need a 
 * "pool" of MAXPROC pcbs to allocate from and deallocate to. The free (or 
 * ununsed) pcbs can be kept on a NULL-terminated doubly, linearly 
 * linked list (using the p_next and p_prev fields), called the pcbFreeList, 
 * which we will treat as a stack. The head for pcbFreeList is pointed to by the 
 * variable pcbFree_h. */


/* Insert the element pointed to by p onto the pcbFree list. In other words,
this method returns a pcb (which is no longer in use) to the pcbFree list. */
void freePcb(pcb_PTR p){
	if (pcbFree_h == NULL){ 
		/* the free list is currently empty */
		p->p_next = NULL;
		p->p_prev = NULL;
		pcbFree_h = p;
	}
	else{ 
		/* there's at least one pcb on the free list already */
		p->p_next = pcbFree_h;
		p->p_prev = NULL;
		pcbFree_h->p_prev = p;
		pcbFree_h = p;
	}
}

/* This function allocates pcbs. Return NULL if the pcbFree list is empty.
Otherwise, remove an element from the pcbFree list, provide initial values
for ALL of the pcb's fields (i.e., NULL and/or 0) and then return a pointer
to the removed element. pcbs get reused, so it is important that no previous
value persists in a pcb when it gets reallocated. */
pcb_PTR allocPcb(){
	if (pcbFree_h == NULL){ 
		/* if the free list is empty */
		return NULL;
	}
	pcb_PTR temp1 = pcbFree_h; /* the element that we will remove from pcbFreeList */
	if (temp1->p_next == NULL){ 
		/* if there's only one element in pcbFreeList */
		pcbFree_h = NULL;
	}
	else{ 
		/* else there's more than one element in pcbFreeList */
		(temp1->p_next)->p_prev = NULL;
		pcbFree_h = temp1->p_next;
	}
	/* provide initial values for ALL of the pcb's fields */
	temp1->p_next = NULL;
	temp1->p_prev = NULL;
	temp1->p_prnt = NULL; /* setting the pointer to temp1's parent to NULL */
	temp1->p_child = NULL; /* setting the pointer to temp1's child to NULL */
	temp1->p_next_sib = NULL; /* setting the pointer to temp1's next sibling to NULL */
	temp1->p_prev_sib = NULL; /* setting the pointer to temp1's previous sibling to NULL */
	
	temp1->p_semAdd = NULL; /* setting temp1's blocking sempahore address to NULL */
	temp1->p_time = 0; /* setting temp1's accumulated time field to zero */
	/* initializing processor state fields */
	temp1->p_s.s_entryHI=NULL;
	temp1->p_s.s_cause=NULL;
	temp1->p_s.s_status=NULL;
	temp1->p_s.s_pc=NULL;
	for (int i=0; i<STATEREGNUM; i++){
		temp1->p_s.s_reg[i]=0;
	}

	temp1->p_supportStruct = NULL; /* setting temp1's Support Structure pointer to NULL */

	return temp1;
}

/* This function initializes the pcbFree list to contain all the elements of the
static array of MAXPROC pcbs. This method will be called only once during
data structure initializaion. */
void initPcbs(){
	int i;
	static pcb_t procTable[MAXPROC]; /* array of pbcs */
	pcbFree_h = NULL;
	for(i = 0; i < MAXPROC; i++){
		/* add each pcb to pcbFree list */
		freePcb(&(procTable[i]));
	}
}

/************Functions that support process queue maintenance***************
 * 
 * Note that all process queues are double, circularly linked lists that have
 * the p_next and p_prev pointer fields. Each queue will be pointed at by a tail
 * pointer. */


/* This function is used to initialize a variable to be tail pointer to a 
process queue. Return a pointer to the tail of an empty process queue; 
i.e. NULL. */
pcb_PTR mkEmptyProcQ(){
    return NULL;
}

/* Return TRUE if the queue whose tail is pointed to by tp is empty.
Return FALSE otherwise. */
int emptyProcQ(pcb_PTR tp){
    return (tp == NULL);
}

/* Insert the pcb pointed to by p into the process queue whose 
tail-pointer is pointed to by tp. */
void insertProcQ(pcb_PTR *tp, pcb_PTR p){
    if (emptyProcQ(*tp)){ 
		/* if the process queue is empty */
		/* We want to make p the one and only pcb in this queue. */
		p->p_next = p;
		p->p_prev = p;
    	*tp = p;
    }
    else{
		/* else process queue is not empty  */
        /* inserting p at the tail, and it becomes the new tail. This is a circular queue, 
		so the tail must point to head, i.e. (*tp)->next must always be the front of the queue. */
	   p->p_next = (*tp)->p_next;
	   p->p_prev = *tp;
	   ((*tp)->p_next)->p_prev = p;
	   (*tp)->p_next = p;
	   *tp = p;
    }
}

/* Remove the first (i.e. head) element from the queue whose 
tail-pointer is pointed to by tp. Return NULL if the process queue was initially empty; 
otherwise return the pointer to the removed element. Update the 
queue’s tail pointer if necessary. (Note: since *tp is a pointer to the tail,
(*tp)->p_next is the head, if it exists)*/
pcb_PTR removeProcQ(pcb_PTR *tp){
    if (emptyProcQ(*tp)){
		/* if the process queue is empty */
        return NULL;
    }
    else
    {
		/* else process queue is not empty */
        /* We need to disconnect the head pcb from the queue, update the tail, and
        return the head. */
		if ((*tp)->p_next == *tp){ 
			/* there's only one element in the queue */
			pcb_t *temp1 = (*tp)->p_next; /* the pcb we want to remove */
			temp1->p_next = NULL;
			temp1->p_prev = NULL;
			*tp = NULL;
			return temp1;
		}
		else{ 
			/* there's more than one element in the queue */
			pcb_t *temp2 = (*tp)->p_next; /* the pcb we want to remove */
			(*tp)->p_next = ((*tp)->p_next)->p_next;
			((*tp)->p_next)->p_prev = *tp;
			temp2->p_next = NULL;
			temp2->p_prev = NULL;
			return temp2;
		}	
	}
}

/* Return a pointer to the first pcb from the process queue whose tail is 
pointed to by tp. Do not remove this pcb from the process queue. Return NULL 
if the process queue is empty. */
pcb_PTR headProcQ(pcb_PTR tp){
    if (emptyProcQ(tp)){
		/* if the process queue is empty */
		return NULL;
	} 
    else{
		/* else process queue is not empty */
		return tp->p_next;
	} 
}

/* Remove the pcb pointed to by p from the process queue whose 
tail-pointer is pointed to by tp. Update the process queue's tail pointer if 
necessary. If the desired entry is not in the indicated queue return NULL; 
otherwise, return p. Note that p can point to any element of the process queue. */
pcb_PTR outProcQ(pcb_PTR *tp, pcb_PTR p){
    pcb_t *previous, *current;
    if (emptyProcQ(*tp)){
		/* if the process queue is empty */
		return NULL;
	} 
    previous = *tp;
    current = (*tp)->p_next; /* head of queue */

    /* We will loop through the queue checking current against p.
    As we advance through the loop, we will update previous and current.
    If we find p, we will disconnect it from the queue by causing the pcb
    referred to by previous to connect to p->p_next. */
    if (current == previous){
		/* if there's only one element in the queue */
		if (current == p){ 
			/* if the pcb that p points to is the one pcb in the queue */
			pcb_t *temp1 = current; /* the pcb we want to remove */
			temp1->p_next = NULL;
			temp1->p_prev = NULL;
			*tp = NULL;
			return temp1;
		}
		else{ 
			/* else the pcb p points to is not in the queue */
			return NULL;
		}
    }
    while (current != *tp){ 
		/*looping through the process queue */
		if (current == p){ 
			/* if the pcb p points to is the current pcb in the queue */
			previous->p_next = current->p_next; /* removing current */
			(current->p_next)->p_prev = previous;
			current->p_next = NULL;
			current->p_prev = NULL;
			return p;
		}
		current = current->p_next; /* incrementing the current pcb */
		previous = previous->p_next; /* incrementing the previous pcb */
   }
   if (current == p){ 
		/* if p points to the tail of the queue */
		previous->p_next = current->p_next; /* removing the tail and updating the tail pointer */
		(current->p_next)->p_prev = previous;
		current->p_next = NULL;
		current->p_prev = NULL;
		*tp = previous;
		return p;
   }
   return NULL; /* the pcb p points to is not in the queue */
}

/***********Functions that support process tree maintenance************************
 * 
 * pcbs are also organized into trees of pcbs, called process trees. Such trees are 
 * implemented so that a parent pcb contains a pointer (p_child) to a NULL-terminated
 * double, linearly linked list (which we will treat as a stack) of its child pcbs. 
 * Each child process has a pointer to its parent pcb (p_prnt) and possibly the next child 
 * pcb of its parent (p_next_sib) and/or the previous child pcb in the list (p_prev_sib). */


/* This function returns TRUE if the pcb pointed to by p has no children. 
Return FALSE otherwise. */
int emptyChild(pcb_PTR p){
	return (p->p_child == NULL);
}

/* This function makes the pcb pointed to by p a child of the pcb pointed to
by prnt. */
void insertChild(pcb_PTR prnt, pcb_PTR p){
	if (prnt->p_child == NULL){ 
		/* if the parent has no children */
		prnt->p_child = p;
		p->p_prnt = prnt;
		p->p_next_sib = NULL;
		p->p_prev_sib = NULL;
	}
	else{ 
		/* else the parent has at least one child already */
		p->p_prnt = prnt;
		p->p_next_sib = prnt->p_child;
		p->p_prev_sib = NULL;
		(prnt->p_child)->p_prev_sib = p;
		prnt->p_child = p;
	}
}

/* This function makes the first child of the pcb pointed to by p no longer a 
child of p. Return NULL if initially there were no children of p. Otherwise,
return a pointer to this removed first child pcb. */
pcb_PTR removeChild(pcb_PTR p){
	if (p->p_child == NULL){ 
		/* if p has no children */
		return NULL;
	}
	if ((p->p_child)->p_next_sib == NULL){ 
		/* p has exactly one child */
		pcb_t *temp1 = p->p_child;
		temp1->p_prnt = NULL;
		p->p_child = NULL;
		return temp1;
	}
	/* p has more than one child */
	pcb_t *temp2 = p->p_child;
	temp2->p_prnt = NULL;
	(temp2->p_next_sib)->p_prev_sib = NULL;
	p->p_child = temp2->p_next_sib;
	temp2->p_next_sib = NULL;
	return temp2;
}

/* This function makes the pcb pointed to by p no longer the child of its 
parent. If the pcb pointed to by p has no parent, return NULL; otherwise,
return p. Note that the element pointed to by p need not be the first child
of its parent. */
pcb_PTR outChild(pcb_PTR p){
	pcb_t *current;
	if (p->p_prnt == NULL){ 
		/* the pcb pointed to by p has no parent */
		return NULL;
	}
	current = (p->p_prnt)->p_child; /* the pcb at the top of the stack */
	if (current->p_next_sib == NULL){ 
		/* there's only one pcb in the stack */
		if (current == p){ 
			/* if the pcb p points to is the only pcb in the stack, and we want to remove it */
			/* removing p */
			pcb_t *temp1 = current;
			temp1->p_prnt = NULL;
			p->p_child = NULL;
			return temp1;
		}
	}
	/* The stack has at least two pcbs in it. */
	if (p->p_next_sib == NULL){ 
		/* if the pcb p points to is the last pcb in the stack */
		/* removing p */
		(p->p_prev_sib)->p_next_sib = NULL; 
		p->p_prev_sib = NULL;
		p->p_prnt = NULL;
		return p;
	}
	/* The pcb p points to is not the last pcb in the stack. */
	if (p->p_prev_sib == NULL){ 
		/* if the pcb p points to is the first pcb in the stack */
		/* removing p */
		(p->p_next_sib)->p_prev_sib = NULL; 
		(p->p_prnt)->p_child = p->p_next_sib;
		p->p_prnt = NULL;
		p->p_next_sib = NULL;
		return p;
	}
	/* The pcb p points to is in the middle of the stack. */
	/* removing p */
	(p->p_prev_sib)->p_next_sib = p->p_next_sib; 
	(p->p_next_sib)->p_prev_sib = p->p_prev_sib;
	p->p_prev_sib = NULL;
	p->p_next_sib = NULL;
	p->p_prnt = NULL;
	return p;
}
