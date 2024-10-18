#include <stdio.h>
#include <stdlib.h>

#ifndef _ITER_
#define _ITER_ 100
#endif
#ifndef _SIZE_
#define _SIZE_ 1000000
#endif

#ifndef ASYNC_MODE
#define ASYNC_MODE 0
#endif
//ASYNC_MODE == 2 is knowen to fail.

int main(int argc, char** argv) {
    long size;
    long *A, *B;
	long sum = 0;
    long i, m;
	long one = 1;
    int error = 0;

	printf("==> ASYNC_MODE = %d\n", ASYNC_MODE);
	printf("==> ITER = %d\n", _ITER_);
	printf("==> SIZE = %d\n", _SIZE_);

    size = _SIZE_;

    A = (long*) malloc(size * sizeof(long));
    B = (long*) malloc(size * sizeof(long));

    for (i = 0; i < size; i++) {
        A[i] = i;
        B[i] = i;
    }

	for (m = 0; m < _ITER_; m++) {
		sum = 0;
#if ASYNC_MODE == 0
#pragma acc parallel loop gang worker copyin(A[0:size], B[0:size]) reduction(+:sum)
#else
#pragma acc parallel loop gang worker copyin(A[0:size], B[0:size], one) reduction(+:sum) async(1)
#endif
    	for (i = 0; i < size; i++) {
    		sum += A[i] + B[i] + one;
    	}
#if ASYNC_MODE == 1
		#pragma acc wait(1)
#endif
#if ASYNC_MODE <= 1
		if (sum != size * size) {error++;}
#endif
	}
#if ASYNC_MODE == 2
	#pragma acc wait(1)
	if (sum != size * size) {error++;}
#endif
	if (error == 0) {printf("==> Test Passed!\n"); }
	else {printf("==> Test Failed (error = %d)\n", error);}

    return 0;
}

