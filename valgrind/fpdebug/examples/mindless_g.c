#include <stdio.h>
#include <math.h>
#include "../fpdebug.h"

float T(float z)
{
	if (z == 0.0f) {
		return 1.0f;
	} else {
		return (expf(z) - 1.0f) / z;
	}
}

float Q(float y)
{
	return abs(y - sqrtf(y*y + 1.0f)) - 1.0f / (y + sqrtf(y*y + 1.0f));
}

float G(float x)
{
	return T( Q(x)*Q(x) );
}

int main( int argc, const char* argv[] )
{
	printf("Kahan - Mindless §6\n");
	
	int i = 1;
	for (; i < 9999; i++) {
		printf("G(%d) = %.7e\n", i, G(i));
	}

	printf("If computed with reals: G(n) = 1 for all n\n");

	float sample = G(9999);
	VALGRIND_PRINT_ERROR_FLOAT(&"G(9999)", &sample);
}

