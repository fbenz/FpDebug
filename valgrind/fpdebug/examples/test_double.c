#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test: double\n");
	
	double a = 2.6;
	double b = 3.7;
	double c = a + b;

	printf("%.15e + %.15e = %.15e\n", a, b, c);

	VALGRIND_PRINT_ERROR_DOUBLE(&"c", &c);
}

