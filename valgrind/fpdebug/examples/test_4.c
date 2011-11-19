#include <stdio.h>
#include "../fpdebug.h"

int main( int argc, const char* argv[] )
{
	printf("Test 4: double version\n");
	
	double time = 0.0;
	int ct = 0;
	while (ct++ < 20000) {
		time += 0.1; /* 0.1f increases the error */
	}

	double relError = (2000.0 - time) / 2000.0;

	printf("Real relative error of time: %.7e\n", relError);

	VALGRIND_PRINT_ERROR(&"time", &time);

	VALGRIND_DUMP_ERROR_GRAPH(&"test_6_time.vcg", &time);
}

