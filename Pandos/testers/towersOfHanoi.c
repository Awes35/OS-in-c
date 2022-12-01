/**************************************************************************** 
 *
 * Program that includes a helper function that, given a value of n, returns
 * the number of moves necessary to complete a Towers of Hanoi game with n
 * discs. The program also tests our operating system by running this
 * helper function on a sample input (5).
 * 
 * Written by: Kollen Gruizenga and Jake Heyser
 ****************************************************************************/

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

/* Function that calculates the number of moves necessary to complete a Towers of Hanoi game with n discs, as defined by:
		hanoi(1) = 1
		hanoi(n) = 2(hanoi(n-1)) + 1 */
int hanoi(int n){
	if (n == 1){ /* if n is 1, then we just need to return 1 */
		return 1; /* returning 1 */
	}

	/* n is not 1, meaning the base case has not yet been reached */
	return 2(hanoi(n-1)) + 1; /* making the recursive call to calculate 2(hanoi(n-1)) + 1 and returning that value to the caller  */
}

/* Function that prints messages to the terminal as decided by what value is returned by the helper function that calculates
Hanoi(n) when the value 5 is passed in as a parameter. The function also terminates the U-proc by issuing a SYS 9 once the
test has concluded. */
void main(){
	int i; /* local variable representing the value that our hanoi() function will return later on */

	print(WRITETERMINAL, "Recursive Hanoi (5) Test starts\n"); /* printing a message to the terminal saying the test has been started */
	i = hanoi(5); /* setting i equal to the value returned by hanoi() when 5 is passed in as a parameter */
	print(WRITETERMINAL, "Recursion concluded\n"); /* printing a message to the terminal saying the recursion from hanoi() has concluded */

	if (i == 31){ /* if hanoi() returned the correct value (namely, 31)*/
		print(WRITETERMINAL, "Recursion concluded successfully\n"); /* printing a message to the terminal saying the recursion from hanoi() concluded successfully */
	}

	else{ /* hanoi() did not return the correct value (31) */
		print(WRITETERMINAL, "ERROR: Recursion problems\n"); /* printing an error message to the terminal saying there are recursion problems */
	}

	SYSCALL(TERMINATE, 0, 0, 0); /* terminating the U-proc by issuing a SYS 9 */
}