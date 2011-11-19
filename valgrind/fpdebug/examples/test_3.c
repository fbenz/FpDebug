#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test 3 (float)\n");
	
	/* machine epsilon: 0.00000006f
	   IEEE 745 single-precision, decimal
	*/
	float e = 0.00000006f;
	float e2 = 0.00000005f;

	float pro = e2 * e2;

	printf("%.7e * %.7e = %.7e\n", e2, e2, pro);

	VALGRIND_PRINT_ERROR(&"pro", &pro);

	float diff = (1 + e) - 1;

	printf("%.7e - %.7e = %.7e\n", (1 + e), 1.0f, diff);

	VALGRIND_PRINT_ERROR(&"diff", &diff);

	float div = 1 / e;

	printf("%.7e / %.7e = %.7e\n", 1.0f, e, div);

	VALGRIND_PRINT_ERROR(&"div", &div);

	VALGRIND_DUMP_ERROR_GRAPH(&"test_3_diff.vcg", &diff);
}

