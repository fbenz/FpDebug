#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test with special numbers (NaN, +Inf, and -Inf)\n");

	float nan = 0.0f / 0.0f;

	float posInf = 1.0f / 0.0f;
	
	float negInf = -1.0f / 0.0f;

	printf("NaN: %.7e\n", nan);
	printf("+Inf: %.7e\n", posInf);
	printf("-Inf: %.7e\n", negInf);
}

