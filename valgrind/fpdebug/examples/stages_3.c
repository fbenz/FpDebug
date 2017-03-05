#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Stages 3\n");
	
	/* IEEE 745 single precision machine epsilon: 0.00000006f */
	float e = 0.00000005f;

	float sum = 1.0f;
	int i = 0;
	for (; i < 5; i++) {
		VALGRIND_BEGIN_STAGE(0);
		sum += e;
		VALGRIND_END_STAGE(0);
	}
	VALGRIND_CLEAR_STAGE(0);

	float x = 2.0f;
	for (i = 0; i < 5; i++) {
		VALGRIND_BEGIN_STAGE(0);
		x *= x;
		VALGRIND_CLEAR_STAGE(0);
		VALGRIND_BEGIN_STAGE(0);
		x *= x;
		VALGRIND_CLEAR_STAGE(0);
		VALGRIND_BEGIN_STAGE(0);
		x *= x;
		VALGRIND_END_STAGE(0);
	}

	printf("Sum: %.7e\n", sum);

	VALGRIND_PRINT_ERROR(&"sum", &sum);
}

