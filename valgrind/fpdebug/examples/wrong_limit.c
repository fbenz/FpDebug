/*
	Wrong Limit of Convergent Sequence

	This example is from the Handbbok of Floating-Point Arithmetic.
	This example has also been analyzed by Kahan before.

	Limit with real numbers is 6. But the limit with arbitrary
	but fixed precision is always 100.
*/

#include <stdio.h>
#include <math.h>
#include "../fpdebug.h"

int main(void)
{
	double u, v, w;
	int i, max;

	max = 100;
	
	u = 2;
	v = -4;

	printf("Computation from 3 to n:\n");
	for (i = 3; i <= max; i++)
	{
		VALGRIND_BEGIN_STAGE(0);

		w = 111. - 1130./v + 3000./(v*u);
		u = v;
		v = w;
		printf("u%d = %1.17g\n", i, v);

		VALGRIND_END_STAGE(0);
	}

	VALGRIND_PRINT_ERROR(&"u(max)", &v);

	return 0;
}

