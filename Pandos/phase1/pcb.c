#include <stdio.h>
#include "pcb.h"

/* Insert the element pointed to by p onto the pcbFree list. In other words,
this method returns a pcb which is no longer in use to the pcbFree list. */
void freePcb(pcb_t *p)
{}

/* This method allocates pcbs. Return NULL if the pcbFree list is empty.
Otherwise, remove an element from the pcbFree list, provide initial values
for ALL of the pcbs fields (i.e., NULL and/or 0) and then return a pointer
to the removed element. pcbs get reused, so it is important that no previous
value persists in a pcb when it gets reallocated. */
pcb_t *allocPcb(pcb_t *p)
{}

/* This method initializes the pcbFree list to contain all the elements of the
static array of MAXPROC pcbs. This method will be called only once during
data structure initializaion. */
initPcbs()
{}


/*WE NEED TO DEFINE pcb_t AND UPDATE THE SYNTAX OF THE NEXT AND PREVIOUS FIELDS. 
EVERYTHING ELSE SHOULD WORK FINE IN THIS SECTION. */

/* This method is used to initialize a variable to be tail pointer to a 
process queue. Return a pointer to the tail of an empty process queue; 
i.e. NULL. */
pcb_t *mkEmptyProcQ()
{
    return NULL;
}

/* Return TRUE if the queue whose tail is pointed to by tp is empty.
Return FALSE otherwise. */
int emptyProcQ(pcb_t *tp)
{
    return (tp == NULL);
}

/* Insert the pcb pointed to by p into the process queue whose 
tail-pointer is pointed to by tp. */
void insertProcQ(pcb_t **tp, pcb_t *p)
{
    if (emptyQ(*tp))
    {
        /* make p the one and only node in this queue */
		p->p_next = p;
		p->p_prev = p;
    	*tp = p;
    }
    else
    {
        /*
            insert p at the tail, and it becomes the new tail
            This is a circular queue, so the tail must point to head, i.e.
            (*tp)->next must always be the front of the queue 
        */
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
pcb_t *removeProcQ(pcb_t **tp)
{
    if (emptyProcQ(*tp))
    {
        return NULL;
    }
    else
    {
        /* disconnect the head node from the queue, update the tail,
        return the head */
		if ((*tp)->p_next == *tp){ //then there's only one element in the queue
			pcb_t *temp1 = (*tp)->p_next; //the node we want to remove 
			temp1->p_next = NULL;
			temp1->p_prev = NULL;
			*tp = NULL;
			return temp1;
		}
		else{ //then there's more than one element in the queue
			pcb_t *temp2 = (*tp)->p_next; //the node we want to remove
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
pcb_t *headProcQ(pcb_t *tp)
{
    if (emptyProcQ(tp)) return NULL;
    else return tp->p_next;
}
/* Remove the pcb pointed to by p from the process queue whose 
tail-pointer is pointed to by tp. Update the process queue's tail pointer if 
necessary. If the desired entry is not in the indicated queue return NULL; 
otherwise, return p. Note that p can point to any element of the process queue. */
pcb_t *outProcQ(pcb_t **tp, pcb_t *p)
{
    pcb_t *previous, *current;
    if (emptyProcQ(*tp)) return NULL;
    previous = *tp;
    current = (*tp)->p_next; /* head of queue */
    /* loop through the queue checking current against p.
    As you advance through the loop, update previous and current.
    If you find p, you have to disconnect it from the queue by causing the node
    referred to by previous to connect to p->p_next 
    */
   if (current == previous){ //then there's only one element in the queue
		if (current == p){ //if the node that p points to is the one node in the queue
			pcb_t *temp1 = current; //the node we want to remove 
			temp1->p_next = NULL;
			temp2->p_prev = NULL;
			*tp = NULL;
			return temp1;
		}
		else{ //the node p points to is not in the queue
			return NULL;
		}
   }
   while (current != *tp){ //looping through the queue
		if (current == p){ //if the node p points to is the current node in the queue
			previous->p_next = current->p_next; //removing current
			(current->p_next)->p_prev = previous;
			current->p_next = NULL;
			current->p_prev = NULL;
			return p;
		}
		current = current->p_next; //incrementing the current node
		previous = previous->p_next; //incrementing the previous node
   }
   if (current == p){ //if p points to the tail of the queue
		previous->p_next = current->p_next; //removing the tail and updating the tail pointer
		(current->p_next)->p_prev = previous;
		current->p_next = NULL;
		current->p_prev = NULL;
		*tp = previous;
		return p;
   }
   return NULL; //the node p points to is not in the queue
}

/* This method returns TRUE if the pcb pointed to by p has no children. 
Return FALSE otherwise. */
int emptyChild(pcb_t *p)
{}

/* This method makes the pcb pointed to by p a child of the pcb pointed to
by prnt. */
insertChild(pcb_t *prnt, pcb_t *p)
{}

/* This method makes the first child of the pcb pointed to by p no longer a 
child of p. Return NULL if initially there were no children of p. Otherwise,
return a pointer to this removed first child pcb. */
pcb_t *removeChild(pcb_t *p)
{}

/* This method makes the pcb pointed to by p no longer the child of its 
parent. If the pcb pointed to by p has no parent, return NULL; otherwise,
return p. Note that the element pointed to by p need not be the first child
of its parent. */
pcb_t *outChild(pcb_t *p)
{}