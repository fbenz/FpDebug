#include <stdio.h>
#include "../fpdebug.h"

void testFunc(void)
{
	float e = 0.00000005f;

	float sum = 1.0f;
	int i = 0;
	for (; i < 5; i++) {
		sum += e;
	}

	/* expected value for sum: 1.0 (exactly)
	   expected absolute error: 5 * e
	   works with both SSE and x87 FPU
	*/
	printf("Sum: %.7e\n", sum);

	if (RUNNING_ON_VALGRIND) {
		printf("Running on valgrind\n");
	} else {
		printf("Not running on valgrind\n");
	}

	VALGRIND_PRINT_ERROR(&"sum", &sum);

	VALGRIND_DUMP_ERROR_GRAPH(&"test_1_sum.vcg", &sum);
}

int main( int argc, const char* argv[] )
{
	printf("Test program with function: machine epsilon, client request\n");
	
	testFunc();
}

