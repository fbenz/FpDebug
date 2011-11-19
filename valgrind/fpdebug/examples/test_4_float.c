#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test 4: float version\n");
	
	float time = 0.0f;
	int i = 0;
	for (; i < 20000; i++) {
		time += 0.1f;
	}

	double relError = ((i * 0.1) - time) / (i * 0.1);

	printf("Real relative error of time: %.7e\n", relError);

	VALGRIND_PRINT_ERROR(&"time", &time);
}

