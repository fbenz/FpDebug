#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test 2: chatastropic cancelation (double)\n");
	
	/* IEEE 745 single precision machine epsilon: 0.00000006f */
	double e = 0.00000006;

	double one = 1.0f;

	double more = one + e;

	double diff = more - one;

	double diff2 = diff - e;

	printf("%.16e - %.16e = %.16e\n", more, one, diff);

	printf("expected: %.16e, but error of: %.16e\n", e, diff2);


	VALGRIND_PRINT_ERROR(&"diff", &diff);
}

