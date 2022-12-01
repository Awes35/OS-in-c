/**************************************************************************** 
 *
 * Program that implements Ackermann's function and tests our operating system by,
 * in part, running Ackermann's function on a sample input (2,3)
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 ****************************************************************************/

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

/* Function that implements Ackermann's function, as defined as:
		A(0,n) = n+1
		A(m+1,0) = A(m,1)
		A(m+1, n+1) = A(m,A(m+1,n)) */
int ack(int m, int n){
	int k; /* local variable representing A(m,n) */

	if (m == 0){ /* if m is 0, then we just need to return n+1 */
		return n++; /* returning n + 1 */
	}

	if (n == 0){ /* if n is 0, then we need to call ack(m-1,1) */
		m--; /* decrementing the value of m by 1 */
		return ack(m,1); /* making the recursive call to calculate ack(m-1,1) */
	}

	/* m and n are both not equal to 0 */
	return ack(m-1, ack(m,n-1)); /* making the recursive call to calculate ack(m-1, ack(m,n-1)) */
}

/* Function that prints messages to the terminal as decided by what value is returned by Ackermann's function when the values 2 and 3,
respectively, are passed in as parameters. The function also terminates the U-proc by issuing a SYS 9 once the test has concluded. */
void main(){
	int i; /* local variable representing the value that Ackermann's function will return later on */

	print(WRITETERMINAL, "Recursive Ackermann (2,3) Test starts\n"); /* printing a message to the terminal saying the test has been started */
	i = ack(2,3); /* setting i equal to the value returned by Ackermann's function when 2 and 3 (in that order) are passed in as parameters */
	print(WRITETERMINAL, "Recursion concluded\n"); /* printing a message to the terminal saying the recursion from Ackermann's function has concluded */

	if (i == 9){ /* if Ackermann's function returned the correct value (namely, 9)*/
		print(WRITETERMINAL, "Recursion concluded successfully\n"); /* printing a message to the terminal saying the recursion from Ackermann's function concluded successfully */
	}

	else{ /* Ackermann's function did not return the correct value (9) */
		print(WRITETERMINAL, "ERROR: Recursion problems\n"); /* printing an error message to the terminal saying there are recursion problems */
	}

	SYSCALL(TERMINATE, 0, 0, 0); /* terminating the U-proc by issuing a SYS 9 */
}