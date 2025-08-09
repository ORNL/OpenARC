#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "openacc.h"

#define MERGE_TASKS

#ifndef _FLOAT_
#define _FLOAT_ float
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

void matmul( _FLOAT_* restrict Z, _FLOAT_* restrict X, _FLOAT_* restrict Y, unsigned int dimSize, unsigned int chunkSizeX, unsigned int chunkSizeY, int asyncID ) {
	unsigned int i, j, k;
#ifdef MERGE_TASKS
	HI_enter_subregion("matmul", 0);
#endif
	#pragma acc update device(X[0:dimSize*chunkSizeX], Y[0:dimSize*chunkSizeY]) async(asyncID)
	#pragma acc parallel loop independent gang worker collapse(2) present(X[0:dimSize*chunkSizeX], Y[0:dimSize*chunkSizeY]) present(Z[0:chunkSizeX*chunkSizeY]) async(asyncID)
	for (i = 0; i < chunkSizeX; i++) {
		for (j = 0; j < chunkSizeY; j++) {
			_FLOAT_ sum = 0.0F;
			#pragma acc loop seq
			for (k = 0; k<dimSize; k++) {
				sum += X[i*dimSize+k] * Y[k*chunkSizeY+j];
			}
			Z[i*chunkSizeY+j] = sum;
		}
	}
	#pragma acc update host(Z[0:chunkSizeX*chunkSizeY]) async(asyncID)
#ifdef MERGE_TASKS
	HI_exit_subregion("matmul", 0);
#endif
}


int main(int argc, char** argv) {
    unsigned int size = 4096;
    _FLOAT_ *X, *Y, *Z;
    unsigned int i, j, k;
	unsigned int chunkSize = 0;
	unsigned int lastChunkSize = 0;
	unsigned int numTasks = 0;
	unsigned int nDevices = 1;
	int asyncID;
    int error = 0;
	unsigned int numElementsXY;
	unsigned int numElementsZ;
	unsigned int chunkSizeX, chunkSizeY;
	unsigned int taskIDx, taskIDy, taskIDz;
	_FLOAT_ valueX, valueY, valueZ;

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
			} else if(strcmp(argv[i], "-s") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -s");
				}
				ok = StrToInt(argv[i+1], &(size));
				if(!ok) {
					printf("Parse Error on option -s integer value required after argument\n");
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
			} else {
				printf("Invalid commandline option: %s\n", argv[i]);
				exit(1);
			}
		}
	}

	if( chunkSize == 0 ) {
		chunkSize = size/nDevices;
	}
	numTasks = size/chunkSize;
	if( size%chunkSize != 0 ) {
		lastChunkSize = chunkSize + (size - (numTasks)*chunkSize);
	} else {
		lastChunkSize = chunkSize;
	}

    X = (_FLOAT_*) malloc(size * size * sizeof(_FLOAT_));
    Y = (_FLOAT_*) malloc(size * size * sizeof(_FLOAT_));
    Z = (_FLOAT_*) malloc(size * size * sizeof(_FLOAT_));

    for (i = 0; i < size; i++) {
		taskIDx = i/chunkSize;
		valueX = (_FLOAT_)(taskIDx%2 + 1);
    	for (j = 0; j < size; j++) {
			taskIDy = j/chunkSize;
			valueY = (_FLOAT_)(taskIDy%2 + 1);
        	X[i*size+j] = valueX;
        	Y[i*size+j] = valueY;
        	Z[i*size+j] = 0.0F;
		}
    }

	int offset = 0;
    for( i = 0; i < numTasks; i++ ) {
		if( i==numTasks-1 ) {
			numElementsXY = lastChunkSize*size;
		} else {
			numElementsXY = chunkSize*size;
		}
		acc_create(X + i*chunkSize*size, numElementsXY*sizeof(_FLOAT_));
		acc_create(Y + i*chunkSize*size, numElementsXY*sizeof(_FLOAT_));
    	for( j = 0; j < numTasks; j++ ) {
			if( i==numTasks-1 ) {
				if( j==numTasks-1 ) {
					numElementsZ = lastChunkSize*lastChunkSize;
				} else {
					numElementsZ = lastChunkSize*chunkSize;
				}
			} else {
				if( j==numTasks-1 ) {
					numElementsZ = chunkSize*lastChunkSize;
				} else {
					numElementsZ = chunkSize*chunkSize;
				}
			}
			acc_create(Z + offset, numElementsZ*sizeof(_FLOAT_));
			offset += numElementsZ;
		}
	}

	elapsed_time = my_timer();
	offset = 0;
    for( i = 0; i < numTasks; i++ ) {
		if( i == numTasks-1 ) {
			chunkSizeX = lastChunkSize;
		} else {
			chunkSizeX = chunkSize;
		}
    	for( j = 0; j < numTasks; j++ ) {
			if( j == numTasks-1 ) {
				chunkSizeY = lastChunkSize;
			} else {
				chunkSizeY = chunkSize;
			}
			asyncID = (i*numTasks+j) % nDevices;
			matmul( Z + offset, X + i*chunkSize*size, Y + j*chunkSize*size, size, chunkSizeX, chunkSizeY, asyncID);
			offset += chunkSizeX*chunkSizeY;
    	}
	}
	#pragma acc wait
	elapsed_time = my_timer() - elapsed_time;
	printf("Accelerator Elapsed time = %lf sec\n", elapsed_time);
	printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", elapsed_time);

#if VERIFICATION == 1
	elapsed_time = my_timer();
	offset = 0;
    for (i = 0; i < numTasks; i++) {
		if( i == numTasks-1 ) {
			chunkSizeX = lastChunkSize;
		} else {
			chunkSizeX = chunkSize;
		}
    	for (j = 0; j < numTasks; j++) {
			if( j == numTasks-1 ) {
				chunkSizeY = lastChunkSize;
			} else {
				chunkSizeY = chunkSize;
			}
			for(int local_i = 0; local_i <chunkSizeX; local_i++) {
				for(int local_j = 0; local_j <chunkSizeY; local_j++) {
					_FLOAT_ sum = 0.0F;
    				for (k = 0; k < size; k++) {
						sum += X[i*chunkSize*size + local_i*size + k] * Y[j*size*chunkSize + k*chunkSizeY + local_j];
					}
        			if (Z[offset + local_i*chunkSizeY + local_j] != sum) {
						error = 1;
						printf("Error occurred at Z[%d,%d]\n", i*chunkSize + local_i, j*chunkSize + local_j);
						break;
					}
				}
				if( error == 1 ) {
					break;
				}
			}
			if( error == 1 ) {
				break;
			}
			offset += chunkSizeX*chunkSizeY;
		}
		if( error == 1 ) {
			break;
		}
    }
	elapsed_time = my_timer() - elapsed_time;
	if( error == 0 ) {
		printf("CPU Elapsed time = %lf sec\n", elapsed_time);
	}
    printf("size:%u, chunkSize:%u, nDevices:%u error:%d\n", size, chunkSize, nDevices, error);
#else
    printf("size:%u, chunkSize:%u, nDevices:%u \n", size, chunkSize, nDevices);
#endif


	offset = 0;
    for( i = 0; i < numTasks; i++ ) {
		if( i==numTasks-1 ) {
			numElementsXY = lastChunkSize*size;
		} else {
			numElementsXY = chunkSize*size;
		}
		acc_delete(X + i*chunkSize*size, numElementsXY*sizeof(_FLOAT_));
		acc_delete(Y + i*chunkSize*size, numElementsXY*sizeof(_FLOAT_));
    	for( j = 0; j < numTasks; j++ ) {
			if( i==numTasks-1 ) {
				if( j==numTasks-1 ) {
					numElementsZ = lastChunkSize*lastChunkSize;
				} else {
					numElementsZ = lastChunkSize*chunkSize;
				}
			} else {
				if( j==numTasks-1 ) {
					numElementsZ = chunkSize*lastChunkSize;
				} else {
					numElementsZ = chunkSize*chunkSize;
				}
			}
			acc_delete(Z + offset, numElementsZ*sizeof(_FLOAT_));
			offset += numElementsZ;
		}
	}

    return 0;
}

