/*
	Malcolm’s algorithm
	Returns the radix β of the ﬂoating-point system	
	It works if the active rounding mode is one of the four rounding modes of IEEE 754-1895
*/

#include <stdio.h>
#include <math.h>

#pragma STDC FP_CONTRACT OFF

int main (void)
{
	double A, B;
	A = 1.0;
	while ((A + 1.0) - A == 1.0)
		A *= 2.0;

	B = 1.0;
	while ((A + B) - A != B)
		B += 1.0;

	printf ("Radix B = %g\n", B);
	return 0;
}

