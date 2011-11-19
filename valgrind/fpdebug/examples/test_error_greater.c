#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test program: machine epsilon, client request\n");
	
	/* machine epsilon: 0.00000006f
	   IEEE 745 single precision, decimal
	*/
	float e = 0.00000005f;

	float sum = 1.0f;
	int i = 0;
	for (; i < 5; i++) {
		sum += e;

		double errorBound = 1.5e-7;
		if (VALGRIND_ERROR_GREATER(&sum, &errorBound)) {
			printf("%d: error greater as %.7e\n", i, errorBound);
		}
	}

	/* expected value for sum: 1.0 (exactly)
	   expected absolute error: 5 * e
	   works with both SSE and x87 FPU
	*/
	printf("Sum: %.7e\n", sum);

	VALGRIND_PRINT_ERROR(&"sum", &sum);
}

