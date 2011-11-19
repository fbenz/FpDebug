#include <cmath>
#include <iostream>
#include <cstdlib>

#include "../fpdebug.h"

int main(int argc, char** argv)
{
	double g = 9.81;
	double mass = 1.;
	double length = 2.;

	double alpha_0 = -0.2;
	double omega_0 = 0.;

	double alpha_last = alpha_0;
	double omega_last = omega_0;

	double alpha_next, omega_next, energy;

	double dt = 0.01;

	int num_steps = atoi(argv[1]);

	for (int i=0; i<num_steps; ++i)
	{
		alpha_next = alpha_last + omega_last * dt;
		omega_next = omega_last - (g/length) * alpha_last * dt;

		energy = 0.5 * pow(length, 2) * pow(omega_next, 2) + 
		         0.5 * mass * g * length * pow(alpha_next, 2);

		alpha_last = alpha_next;
		omega_last = omega_next;

		std::cout << i << " " << alpha_next << " " << omega_next << " " << energy << std::endl;
	}

	VALGRIND_PRINT_ERROR_DOUBLE(&"energy", &energy);

	return 0;
}

