
/* IEEE 745 single precision (float) machine epsilon: 0.00000006f
   that is the smallest number n such that
   1 + n != 1 */

#include <stdio.h>

int main( int argc, const char* argv[] ) {
	printf("Machin epsilon\n");
	
	float a = 1.0f;
	float e = 0.00000005f;
	float sum = a + e;

	printf("Sum: %e + %e = %.7e\n", a, e, sum);

	if (sum == 1.0f) {
		printf("Sum == 1.0f\n");
	} else {
		printf("Sum != 1.0f\n");
	}
}

