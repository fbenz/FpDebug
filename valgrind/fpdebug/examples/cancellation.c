#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test program: Cancellation\n");
	
	float max = 3.4028235e38;
	float zero = max - max;
	
	printf("Max: %.7e\n", max);
	printf("Zero: %.7e\n", zero);

	float more = 1.00000006f;
	float one = 1.0f;

	float diff = more - one;

	printf("Diff: %.7e\n", diff);

	VALGRIND_PRINT_ERROR(&"diff", &diff);
}

