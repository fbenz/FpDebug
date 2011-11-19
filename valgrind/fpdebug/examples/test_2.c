#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test 2: chatastropic cancelation (float)\n");
	
	/* IEEE 745 single precision machine epsilon: 0.00000006f */
	float e = 0.00000006f;

	float x = 0.5f;

	float one = 1.0f + x;

	float more = one + e;

	VALGRIND_PRINT_ERROR(&"more", &more);

	float diff = more - one;

	float diff2 = diff - e;

	printf("%.7e - %.7e = %.7e\n", more, one, diff);

	printf("expected: %.7e, but error of: %.7e\n", e, diff2);


	VALGRIND_PRINT_ERROR(&"diff", &diff);

	VALGRIND_PRINT_ERROR(&"diff2", &diff2);

	VALGRIND_DUMP_ERROR_GRAPH(&"test_2_diff2.vcg", &diff2);
}

