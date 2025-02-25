#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "openacc.h"

//#define MERGE_TASKS

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

void gemm(_FLOAT_ alpha, _FLOAT_ beta, _FLOAT_* restrict C, _FLOAT_* restrict A, _FLOAT_* restrict B, unsigned int dimSize, unsigned int chunkSizeM, unsigned int chunkSizeN, int asyncID ) {
	unsigned int i, j, k;
#ifdef MERGE_TASKS
	HI_enter_subregion("gemm", 0);
#endif
	#pragma acc update device(A[0:dimSize*chunkSizeM], B[0:dimSize*chunkSizeN], C[0:chunkSizeM*chunkSizeN]) async(asyncID)
	#pragma acc parallel loop independent gang worker collapse(2) present(A[0:dimSize*chunkSizeM], B[0:dimSize*chunkSizeN]) present(C[0:chunkSizeM*chunkSizeN]) async(asyncID)
	for (i = 0; i < chunkSizeM; i++) {
		for (j = 0; j < chunkSizeN; j++) {
			_FLOAT_ sum = 0.0F;
			#pragma acc loop seq
			for (k = 0; k<dimSize; k++) {
				sum += A[i*dimSize+k] * B[k*chunkSizeN+j];
			}
			C[i*chunkSizeN+j] = alpha * sum + beta * C[i*chunkSizeN+j];
		}
	}
	#pragma acc update host(C[0:chunkSizeM*chunkSizeN]) async(asyncID)
#ifdef MERGE_TASKS
	HI_exit_subregion("gemm", 0);
#endif
}


int main(int argc, char** argv) {
	unsigned int _M_ = 1024;
	unsigned int _N_ = 1024;
	unsigned int _K_ = 1024;
	_FLOAT_ alpha = 2.0F;
	_FLOAT_ beta = 2.0F;
    _FLOAT_ *A, *B, *C, *Cin;
    unsigned int i, j, k, m;
	unsigned int numItr = 1;
	unsigned int chunkSize = 0;
	unsigned int lastChunkSizeM = 0;
	unsigned int lastChunkSizeN = 0;
	unsigned int numTasksM = 0;
	unsigned int numTasksN = 0;
	unsigned int nDevices = 1;
	unsigned int asyncMode = 2;
	int asyncID;
    int error = 0;
	unsigned int numElementsAB;
	unsigned int numElementsC;
	unsigned int chunkSizeM, chunkSizeN;
	unsigned int taskIDi, taskIDj;
	_FLOAT_ valueA, valueB, valueC;

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
			} else if(strcmp(argv[i], "-n") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -n");
				}
				ok = StrToInt(argv[i+1], &(_N_));
				if(!ok) {
					printf("Parse Error on option -n integer value required after argument\n");
				}
				i+=2;
			} else if(strcmp(argv[i], "-k") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -k");
				}
				ok = StrToInt(argv[i+1], &(_K_));
				if(!ok) {
					printf("Parse Error on option -k integer value required after argument\n");
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
				printf("    -m M  #set the value of _M_ with M\n");
				printf("    -n N  #set the value of _N_ with N\n");
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
	numTasksM = _M_/chunkSize;
	if( _M_%chunkSize != 0 ) {
		lastChunkSizeM = chunkSize + (_M_ - (numTasksM)*chunkSize);
	} else {
		lastChunkSizeM = chunkSize;
	}
	numTasksN = _N_/chunkSize;
	if( _N_%chunkSize != 0 ) {
		lastChunkSizeN = chunkSize + (_N_ - (numTasksN)*chunkSize);
	} else {
		lastChunkSizeN = chunkSize;
	}

    A = (_FLOAT_*) malloc(_M_ * _K_ * sizeof(_FLOAT_));
    B = (_FLOAT_*) malloc(_K_ * _N_ * sizeof(_FLOAT_));
    C = (_FLOAT_*) malloc(_M_ * _N_ * sizeof(_FLOAT_));
    Cin = (_FLOAT_*) malloc(_M_ * _N_ * sizeof(_FLOAT_));

    for (i = 0; i < _M_; i++) {
		taskIDi = i/chunkSize;
		valueA = (_FLOAT_)(taskIDi%2 + 1);
    	for (k = 0; k < _K_; k++) {
        	A[i*_K_+k] = valueA;
		}
    }

    for (j = 0; j < _N_; j++) {
		taskIDj = j/chunkSize;
		valueB = (_FLOAT_)(taskIDi%2 + 1);
    	for (k = 0; k < _K_; k++) {
        	B[j*_K_+k] = valueB;
		}
    }

    for (i = 0; i < _M_; i++) {
    	for (j = 0; j < _N_; j++) {
        	C[i*_K_+j] = 1.0F;
        	Cin[i*_K_+j] = 1.0F;
		}
    }

	elapsed_time = my_timer();
    for( i = 0; i < numTasksM; i++ ) {
		if( i==numTasksM-1 ) {
			numElementsAB = lastChunkSizeM*_K_;
		} else {
			numElementsAB = chunkSize*_K_;
		}
		acc_create(A + i*chunkSize*_K_, numElementsAB*sizeof(_FLOAT_));
	}
    for( j = 0; j < numTasksN; j++ ) {
		if( j==numTasksN-1 ) {
			numElementsAB = lastChunkSizeN*_K_;
		} else {
			numElementsAB = chunkSize*_K_;
		}
		acc_create(B + j*chunkSize*_K_, numElementsAB*sizeof(_FLOAT_));
	}
	int offset = 0;
    for( i = 0; i < numTasksM; i++ ) {
    	for( j = 0; j < numTasksN; j++ ) {
			if( i==numTasksM-1 ) {
				if( j==numTasksN-1 ) {
					numElementsC = lastChunkSizeM*lastChunkSizeN;
				} else {
					numElementsC = lastChunkSizeM*chunkSize;
				}
			} else {
				if( j==numTasksN-1 ) {
					numElementsC = chunkSize*lastChunkSizeN;
				} else {
					numElementsC = chunkSize*chunkSize;
				}
			}
			acc_create(C + offset, numElementsC*sizeof(_FLOAT_));
			offset += numElementsC;
		}
	}

#if VERIFICATION == 1
	numItr = 1;
#endif
	for( m = 0; m < numItr; m++ ) {
#ifdef GRAPH
		HI_enter_subregion("gemm-graph", 1);
#endif
		offset = 0;
    	for( i = 0; i < numTasksM; i++ ) {
			if( i == numTasksM-1 ) {
				chunkSizeM = lastChunkSizeM;
			} else {
				chunkSizeM = chunkSize;
			}
    		for( j = 0; j < numTasksN; j++ ) {
				if( j == numTasksN-1 ) {
					chunkSizeN = lastChunkSizeN;
				} else {
					chunkSizeN = chunkSize;
				}
				if( asyncMode == 1 ) {
					asyncID = 1;
				} else if( asyncMode == 2 ) {
					asyncID = (i*numTasksN+j) % nDevices;
				} else {
					asyncID = (i*numTasksN+j);
				}
				gemm(alpha, beta, C + offset, A + i*chunkSize*_K_, B + j*chunkSize*_K_, _K_, chunkSizeM, chunkSizeN, asyncID);
				offset += chunkSizeM*chunkSizeN;
    		}
		}
#ifdef GRAPH
		HI_exit_subregion("gemm-graph", 1);
#endif
		#pragma acc wait
	}
	elapsed_time = my_timer() - elapsed_time;
	printf("Accelerator Elapsed time = %lf sec\n", elapsed_time);
	printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", elapsed_time);

#if VERIFICATION == 1
	elapsed_time = my_timer();
	offset = 0;
    for (i = 0; i < numTasksM; i++) {
		if( i == numTasksM-1 ) {
			chunkSizeM = lastChunkSizeM;
		} else {
			chunkSizeM = chunkSize;
		}
    	for (j = 0; j < numTasksN; j++) {
			if( j == numTasksN-1 ) {
				chunkSizeN = lastChunkSizeN;
			} else {
				chunkSizeN = chunkSize;
			}
			for(int local_i = 0; local_i <chunkSizeM; local_i++) {
				for(int local_j = 0; local_j <chunkSizeN; local_j++) {
					_FLOAT_ sum = 0.0F;
    				for (k = 0; k < _K_; k++) {
						sum += A[i*chunkSize*_K_ + local_i*_K_ + k] * B[j*_K_*chunkSize + k*chunkSizeN + local_j];
					}
					sum = alpha * sum + beta * Cin[offset + local_i*chunkSizeN + local_j];
        			if (C[offset + local_i*chunkSizeN + local_j] != sum) {
						error = 1;
						printf("Error occurred at C[%d,%d]\n", i*chunkSize + local_i, j*chunkSize + local_j);
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
			offset += chunkSizeM*chunkSizeN;
		}
		if( error == 1 ) {
			break;
		}
    }
	elapsed_time = my_timer() - elapsed_time;
	if( error == 0 ) {
		printf("CPU Elapsed time = %lf sec\n", elapsed_time);
	}
    printf("_M_:%u, _N_:%u, _K_:%u, chunkSize:%u, nDevices:%u error:%d\n", _M_, _N_, _K_, chunkSize, nDevices, error);
#else
    printf("_M_:%u, _N_:%u, _K_:%u, chunkSize:%u, nDevices:%u\n", _M_, _N_, _K_, chunkSize, nDevices);
#endif

    for( i = 0; i < numTasksM; i++ ) {
		if( i==numTasksM-1 ) {
			numElementsAB = lastChunkSizeM*_K_;
		} else {
			numElementsAB = chunkSize*_K_;
		}
		acc_delete(A + i*chunkSize*_K_, numElementsAB*sizeof(_FLOAT_));
	}
    for( j = 0; j < numTasksN; j++ ) {
		if( j==numTasksN-1 ) {
			numElementsAB = lastChunkSizeN*_K_;
		} else {
			numElementsAB = chunkSize*_K_;
		}
		acc_delete(B + j*chunkSize*_K_, numElementsAB*sizeof(_FLOAT_));
	}
	offset = 0;
    for( i = 0; i < numTasksM; i++ ) {
    	for( j = 0; j < numTasksN; j++ ) {
			if( i==numTasksM-1 ) {
				if( j==numTasksN-1 ) {
					numElementsC = lastChunkSizeM*lastChunkSizeN;
				} else {
					numElementsC = lastChunkSizeM*chunkSize;
				}
			} else {
				if( j==numTasksN-1 ) {
					numElementsC = chunkSize*lastChunkSizeN;
				} else {
					numElementsC = chunkSize*chunkSize;
				}
			}
			acc_delete(C + offset, numElementsC*sizeof(_FLOAT_));
			offset += numElementsC;
		}
	}

    return 0;
}

