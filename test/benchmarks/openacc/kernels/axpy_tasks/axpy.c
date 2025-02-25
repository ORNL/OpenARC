#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "openacc.h"

//#define MERGE_TASKS
//#define GRAPH

#ifndef _FLOAT_
#define _FLOAT_ double
#endif

#ifndef VERIFICATION
#define VERIFICATION 0
#endif


/* Helper function for converting strings to ints, with error checking */
int StrToInt(const char *token, unsigned int *retVal)
{
  const char *c ;
  char *endptr ;
  const int decimal_base = 10 ;

  if (token == NULL)
    return 0 ; 

  c = token ;
  *retVal = (int)strtol(c, &endptr, decimal_base) ;
  if((endptr != c) && ((*endptr == ' ') || (*endptr == '\0')))
    return 1 ; 
  else
    return 0 ; 
}

double my_timer ()
{
    struct timeval time;

    gettimeofday (&time, 0);

    return time.tv_sec + time.tv_usec / 1000000.0;
}

void axpy(_FLOAT_ alpha, _FLOAT_* restrict Y, _FLOAT_* restrict X, unsigned int numElements, int asyncID ) {
	unsigned int i;
#ifdef MERGE_TASKS
	HI_enter_subregion("axpy", 0);
#endif
	#pragma acc update device(X[0:numElements], Y[0:numElements]) async(asyncID)
	#pragma acc parallel loop independent gang worker present(X[0:numElements]) present(Y[0:numElements]) async(asyncID)
	for (i = 0; i < numElements; i++) {
		Y[i] = alpha * X[i] + Y[i];
	}
	#pragma acc update host(Y[0:numElements]) async(asyncID)
#ifdef MERGE_TASKS
	HI_exit_subregion("axpy", 0);
#endif
}


int main(int argc, char** argv) {
	unsigned int _M_ = 1024;
	_FLOAT_ alpha = 2.0F;
    _FLOAT_ *X, *Y, *Yin;
    unsigned int i, j, k, m;
	unsigned int numItr = 1;
	unsigned int chunkSize = 0;
	unsigned int lastChunkSize = 0;
	unsigned int numTasks = 0;
	unsigned int nDevices = 1;
	unsigned int asyncMode = 2;
	int asyncID;
    int error = 0;
	unsigned int numElements;
	unsigned int taskIDi;
	_FLOAT_ valueX;

	double elapsed_time;

	if( argc > 1 ) {
		i = 1;
		while( i<argc ) {
			int ok;
			if(strcmp(argv[i], "-d") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -d");
				}
				ok = StrToInt(argv[i+1], &(nDevices));
				if(!ok) {
					printf("Parse Error on option -d integer value required after argument\n");
				}
				i+=2;
			} else if(strcmp(argv[i], "-m") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -m");
				}
				ok = StrToInt(argv[i+1], &(_M_));
				if(!ok) {
					printf("Parse Error on option -m integer value required after argument\n");
				}
				i+=2;
			} else if(strcmp(argv[i], "-c") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -c");
				}
				ok = StrToInt(argv[i+1], &(chunkSize));
				if(!ok) {
					printf("Parse Error on option -c integer value required after argument\n");
				}
				i+=2;
			} else if(strcmp(argv[i], "-i") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -i");
				}
				ok = StrToInt(argv[i+1], &(numItr));
				if(!ok) {
					printf("Parse Error on option -i integer value required after argument\n");
				}
				i+=2;
            } else if(strcmp(argv[i], "-am") == 0) {
                if (i+1 >= argc) {
                    printf("Missing integer argument to -am");
                }
                ok = StrToInt(argv[i+1], &(asyncMode));
                if(!ok) {
                    printf("Parse Error on option -i integer value required after argument\n");
                }
                i+=2;
			} else if(strcmp(argv[i], "-h") == 0) {
				printf("==> Available commandline options:\n");
				printf("    -h    #print help message\n");
				printf("    -k K  #set the value of _K_ with K\n");
				printf("    -d D  #set the value of nDevices with D\n");
				printf("    -c C  #set the value of chunkSize with C\n");
				printf("    -i I  #set the value of numItr with I\n");
				exit(0);
			} else {
				printf("Invalid commandline option: %s\n", argv[i]);
				exit(1);
			}
		}
	}

	if( chunkSize == 0 ) {
		chunkSize = _M_/nDevices;
	}
	numTasks = _M_/chunkSize;
	if( _M_%chunkSize != 0 ) {
		lastChunkSize = chunkSize + (_M_ - (numTasks)*chunkSize);
	} else {
		lastChunkSize = chunkSize;
	}

    X = (_FLOAT_*) malloc(_M_ * sizeof(_FLOAT_));
    Y = (_FLOAT_*) malloc(_M_ * sizeof(_FLOAT_));
    Yin = (_FLOAT_*) malloc(_M_ * sizeof(_FLOAT_));

    for (i = 0; i < _M_; i++) {
		taskIDi = i/chunkSize;
		valueX = (_FLOAT_)(taskIDi%2 + 1);
        X[i] = valueX;
        Y[i] = 1.0F;
        Yin[i] = 1.0F;
    }

	elapsed_time = my_timer();
    for( i = 0; i < numTasks; i++ ) {
		if( i==numTasks-1 ) {
			numElements = lastChunkSize;
		} else {
			numElements = chunkSize;
		}
		acc_create(X + i*chunkSize, numElements*sizeof(_FLOAT_));
		acc_create(Y + i*chunkSize, numElements*sizeof(_FLOAT_));
	}

#if VERIFICATION == 1
	numItr = 1;
#endif
	for( m = 0; m < numItr; m++ ) {
#ifdef GRAPH
		HI_enter_subregion("axpy-graph", 1);
#endif
    	for( i = 0; i < numTasks; i++ ) {
			if( i == numTasks-1 ) {
				numElements = lastChunkSize;
			} else {
				numElements = chunkSize;
			}
			if( asyncMode == 1 ) {
				asyncID = 1;
			} else if( asyncMode == 2 ) {
				asyncID = i % nDevices;
			} else {
				asyncID = i;
			}
			axpy(alpha, Y + i*chunkSize, X + i*chunkSize, numElements, asyncID);
		}
#ifdef GRAPH
		HI_exit_subregion("axpy-graph", 1);
#endif
		#pragma acc wait
	}
	elapsed_time = my_timer() - elapsed_time;
	printf("Accelerator Elapsed time = %lf sec\n", elapsed_time);
	printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", elapsed_time);

#if VERIFICATION == 1
	elapsed_time = my_timer();
    for (i = 0; i < numTasks; i++) {
		if( i == numTasks-1 ) {
			numElements = lastChunkSize;
		} else {
			numElements = chunkSize;
		}
		for(int local_i = 0; local_i <numElements; local_i++) {
			_FLOAT_ sum = alpha * X[i*chunkSize + local_i] + Yin[i*chunkSize + local_i];
        	if (Y[i*chunkSize + local_i] != sum) {
				error = 1;
				printf("Error occurred at Y[%d]\n", i*chunkSize + local_i);
				break;
			}
		}
		if( error == 1 ) {
			break;
		}
    }
	elapsed_time = my_timer() - elapsed_time;
	if( error == 0 ) {
		printf("CPU Elapsed time = %lf sec\n", elapsed_time);
	}
    printf("_M_:%u, chunkSize:%u, nDevices:%u error:%d\n", _M_, chunkSize, nDevices, error);
#else
    printf("_M_:%u, chunkSize:%u, nDevices:%u \n", _M_, chunkSize, nDevices);
#endif

    for( i = 0; i < numTasks; i++ ) {
		if( i==numTasks-1 ) {
			numElements = lastChunkSize;
		} else {
			numElements = chunkSize;
		}
		acc_delete(X + i*chunkSize, numElements*sizeof(_FLOAT_));
		acc_delete(Y + i*chunkSize, numElements*sizeof(_FLOAT_));
	}

    return 0;
}

