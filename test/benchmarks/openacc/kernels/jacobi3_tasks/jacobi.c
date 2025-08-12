#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#if OMP == 1
#include <omp.h>
#endif
#include "openacc.h"

#ifndef _FLOAT_
#define _FLOAT_ double
#endif

#ifndef VERIFICATION
#define VERIFICATION 0
#endif

#define CHECK_RESULT

/* Helper function for converting strings to ints, with error checking */
int StrToInt(const char *token, size_t *retVal)
{
  const char *c ;
  char *endptr ;
  const int decimal_base = 10 ;

  if (token == NULL)
    return 0 ; 

  c = token ;
  *retVal = (size_t)strtoul(c, &endptr, decimal_base) ;
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

void jacobi(_FLOAT_ *outA_bU, _FLOAT_ *outA_m, _FLOAT_ *outA_bD,  _FLOAT_ *inA_gU, _FLOAT_ *inA_bU, _FLOAT_ *inA_m, _FLOAT_ *inA_bD, _FLOAT_ *inA_gD,  size_t tChunkSize, size_t ASIZE, int asyncID) {
	size_t i, j;
	size_t ASIZE_2 = ASIZE + 2;
	size_t numElements = (tChunkSize-2)*(ASIZE_2);
#pragma acc parallel loop gang present(outA_bU[0:ASIZE_2], outA_bD[0:ASIZE_2], outA_m[0:numElements], inA_gU[0:ASIZE_2], inA_bU[0:ASIZE_2], inA_m[0:numElements], inA_bD[0:ASIZE_2], inA_gD[0:ASIZE_2]) private(i) async(asyncID)
	for (i = 0; i < tChunkSize; i++)
	{
		if (i == 0) {
#pragma acc loop worker private(j)
			for (j = 1; j <= ASIZE; j++)
			{
				outA_bU[j] = (inA_gU[j] + inA_m[j] + inA_bU[j - 1] + inA_bU[j + 1]) / 4.0f;
			}
		} else if (i == 1) {
#pragma acc loop worker private(j)
			for (j = 1; j <= ASIZE; j++)
			{
				outA_m[j] = (inA_bU[j] + inA_m[ASIZE_2+j] + inA_m[j - 1] + inA_m[j + 1]) / 4.0f;
			}
		} else if(i == tChunkSize-2) {
#pragma acc loop worker private(j)
			for (j = 1; j <= ASIZE; j++)
			{
				outA_m[(i-1)*ASIZE_2+j] = (inA_m[(i - 2)*ASIZE_2+j] + inA_bD[j] + inA_m[(i-1)*ASIZE_2+(j - 1)] + inA_m[(i-1)*ASIZE_2+(j + 1)]) / 4.0f;
			}
		} else if(i == tChunkSize-1) {
#pragma acc loop worker private(j)
			for (j = 1; j <= ASIZE; j++)
			{
				outA_bD[j] = (inA_m[(i-2)*ASIZE_2+j] + inA_gD[j] + inA_bD[j - 1] + inA_bD[j + 1]) / 4.0f;
			}
		} else {
#pragma acc loop worker private(j)
			for (j = 1; j <= ASIZE; j++)
			{
				outA_m[(i-1)*ASIZE_2+j] = (inA_m[(i - 2)*ASIZE_2+j] + inA_m[i*ASIZE_2+j] + inA_m[(i-1)*ASIZE_2+(j - 1)] + inA_m[(i-1)*ASIZE_2+(j + 1)]) / 4.0f;
			}
		}
	}
}

void jacobi_cpu(_FLOAT_ *outA_bU, _FLOAT_ *outA_m, _FLOAT_ *outA_bD,  _FLOAT_ *inA_gU, _FLOAT_ *inA_bU, _FLOAT_ *inA_m, _FLOAT_ *inA_bD, _FLOAT_ *inA_gD,  size_t tChunkSize, size_t ASIZE, int asyncID) {
	size_t i, j;
	size_t ASIZE_2 = ASIZE + 2;
	size_t numElements = (tChunkSize-2)*(ASIZE_2);
#pragma omp parallel for private(i,j)
	for (i = 0; i < tChunkSize; i++)
	{
		if (i == 0) {
			for (j = 1; j <= ASIZE; j++)
			{
				outA_bU[j] = (inA_gU[j] + inA_m[j] + inA_bU[j - 1] + inA_bU[j + 1]) / 4.0f;
			}
		} else if (i == 1) {
			for (j = 1; j <= ASIZE; j++)
			{
				outA_m[j] = (inA_bU[j] + inA_m[ASIZE_2+j] + inA_m[j - 1] + inA_m[j + 1]) / 4.0f;
			}
		} else if(i == tChunkSize-2) {
			for (j = 1; j <= ASIZE; j++)
			{
				outA_m[(i-1)*ASIZE_2+j] = (inA_m[(i - 2)*ASIZE_2+j] + inA_bD[j] + inA_m[(i-1)*ASIZE_2+(j - 1)] + inA_m[(i-1)*ASIZE_2+(j + 1)]) / 4.0f;
			}
		} else if(i == tChunkSize-1) {
			for (j = 1; j <= ASIZE; j++)
			{
				outA_bD[j] = (inA_m[(i-2)*ASIZE_2+j] + inA_gD[j] + inA_bD[j - 1] + inA_bD[j + 1]) / 4.0f;
			}
		} else {
			for (j = 1; j <= ASIZE; j++)
			{
				outA_m[(i-1)*ASIZE_2+j] = (inA_m[(i - 2)*ASIZE_2+j] + inA_m[i*ASIZE_2+j] + inA_m[(i-1)*ASIZE_2+(j - 1)] + inA_m[(i-1)*ASIZE_2+(j + 1)]) / 4.0f;
			}
		}
	}
}

int main (int argc, char *argv[])
{
    size_t i, j, k, m;
	int is_even_itr = 1;
	size_t numItr = 1;
    size_t chunkSize = 0;
    size_t lastChunkSize = 0;
    size_t tChunkSize = 0;
    size_t numTasks = 0;
    size_t nDevices = 1;
    size_t asyncMode = 2;
    int asyncID;
	size_t ASIZE = 8192;
	size_t ASIZE_1 = ASIZE + 1;
	size_t ASIZE_2 = ASIZE + 2;
	_FLOAT_ **a_g = NULL;
	_FLOAT_ **a_b = NULL;
	_FLOAT_ **a_m = NULL;
	_FLOAT_ **b_g = NULL;
	_FLOAT_ **b_b = NULL;
	_FLOAT_ **b_m = NULL;
	_FLOAT_ *tArray1_g = NULL;
	_FLOAT_ *tArray1_b = NULL;
	_FLOAT_ *tArray1_bU = NULL;
	_FLOAT_ *tArray1_bD = NULL;
	_FLOAT_ *tArray1_m = NULL;
	_FLOAT_ *tArray2_g = NULL;
	_FLOAT_ *tArray2_gU = NULL;
	_FLOAT_ *tArray2_gD = NULL;
	_FLOAT_ *tArray2_b = NULL;
	_FLOAT_ *tArray2_bU = NULL;
	_FLOAT_ *tArray2_bD = NULL;
	_FLOAT_ *tArray2_m = NULL;
	_FLOAT_ *tmpArray = NULL;
#if VERIFICATION >= 1
	_FLOAT_ **a_CPU_g = NULL;
	_FLOAT_ **a_CPU_b = NULL;
	_FLOAT_ **a_CPU_m = NULL;
	_FLOAT_ **b_CPU_g = NULL;
	_FLOAT_ **b_CPU_b = NULL;
	_FLOAT_ **b_CPU_m = NULL;
	_FLOAT_ *cArray1_g = NULL;
	_FLOAT_ *cArray1_b = NULL;
	_FLOAT_ *cArray1_bU = NULL;
	_FLOAT_ *cArray1_bD = NULL;
	_FLOAT_ *cArray1_m = NULL;
	_FLOAT_ *cArray2_g = NULL;
	_FLOAT_ *cArray2_gU = NULL;
	_FLOAT_ *cArray2_gD = NULL;
	_FLOAT_ *cArray2_b = NULL;
	_FLOAT_ *cArray2_bU = NULL;
	_FLOAT_ *cArray2_bD = NULL;
	_FLOAT_ *cArray2_m = NULL;
	_FLOAT_ *cTmpArray = NULL;
#endif
	_FLOAT_* tmp1 = NULL;
	_FLOAT_* tmp2 = NULL;
    double strt_time, done_time;
#ifdef CHECK_RESULT
	double sum = 0.0;
#endif

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
                ok = StrToInt(argv[i+1], &(ASIZE));
                if(!ok) {
                    printf("Parse Error on option -c integer value required after argument\n");
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
                printf("    -s ASIZE  #set the value of array size with ASIZE\n");
                printf("    -i I  #set the value of numItr with I\n");
                printf("    -d D  #set the value of nDevices with D\n");
                printf("    -c C  #set the value of chunkSize with C\n");
                exit(0);
            } else {
                printf("Invalid commandline option: %s\n", argv[i]);
                exit(1);
            }
        }
    }

	ASIZE_1 = ASIZE + 1;
	ASIZE_2 = ASIZE + 2;

    if( chunkSize == 0 ) { 
        chunkSize = ASIZE/nDevices;
    }   
    numTasks = ASIZE/chunkSize;
    if( ASIZE%chunkSize != 0 ) { 
        lastChunkSize = chunkSize + (ASIZE - (numTasks)*chunkSize);
    } else {
        lastChunkSize = chunkSize;
    }   


	a_g = (_FLOAT_ **)malloc((numTasks+1) * sizeof(_FLOAT_ *));
	a_b = (_FLOAT_ **)malloc((numTasks+1) * sizeof(_FLOAT_ *));
	a_m = (_FLOAT_ **)malloc(numTasks * sizeof(_FLOAT_ *));
	b_g = (_FLOAT_ **)malloc((numTasks+1) * sizeof(_FLOAT_ *));
	b_b = (_FLOAT_ **)malloc((numTasks+1) * sizeof(_FLOAT_ *));
	b_m = (_FLOAT_ **)malloc(numTasks * sizeof(_FLOAT_ *));
	for(i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		a_g[i] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
		a_b[i] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
		a_m[i] = (_FLOAT_ *)malloc(ASIZE_2 * (tChunkSize-2) * sizeof(_FLOAT_));
		b_g[i] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
		b_b[i] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
		b_m[i] = (_FLOAT_ *)malloc(ASIZE_2 * (tChunkSize-2) * sizeof(_FLOAT_));
	}
	a_g[numTasks] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
	a_b[numTasks] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
	b_g[numTasks] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
	b_b[numTasks] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));

#if VERIFICATION >= 1
	a_CPU_g = (_FLOAT_ **)malloc((numTasks+1) * sizeof(_FLOAT_ *));
	a_CPU_b = (_FLOAT_ **)malloc((numTasks+1) * sizeof(_FLOAT_ *));
	a_CPU_m = (_FLOAT_ **)malloc(numTasks * sizeof(_FLOAT_ *));
	b_CPU_g = (_FLOAT_ **)malloc((numTasks+1) * sizeof(_FLOAT_ *));
	b_CPU_b = (_FLOAT_ **)malloc((numTasks+1) * sizeof(_FLOAT_ *));
	b_CPU_m = (_FLOAT_ **)malloc(numTasks * sizeof(_FLOAT_ *));
	for(i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		a_CPU_g[i] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
		a_CPU_b[i] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
		a_CPU_m[i] = (_FLOAT_ *)malloc(ASIZE_2 * (tChunkSize-2) * sizeof(_FLOAT_));
		b_CPU_g[i] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
		b_CPU_b[i] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
		b_CPU_m[i] = (_FLOAT_ *)malloc(ASIZE_2 * (tChunkSize-2) * sizeof(_FLOAT_));
	}
	a_CPU_g[numTasks] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
	a_CPU_b[numTasks] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
	b_CPU_g[numTasks] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
	b_CPU_b[numTasks] = (_FLOAT_ *)malloc(ASIZE_2 * sizeof(_FLOAT_));
#endif 

    for (i = 0; i < numTasks; i++)
    {
		tArray1_m = a_m[i];
		tArray2_m = b_m[i];
#if VERIFICATION >= 1
		cArray1_m = a_CPU_m[i];
		cArray2_m = b_CPU_m[i];
#endif
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
        for (j = 0; j < (tChunkSize-2); j++)
        {
        	for (k = 0; k < ASIZE_2; k++)
        	{
            	tArray1_m[j*ASIZE_2 + k] = 0.0;
            	tArray2_m[j*ASIZE_2 + k] = 0.0;
#if VERIFICATION >= 1
            	cArray1_m[j*ASIZE_2 + k] = 0.0;
            	cArray2_m[j*ASIZE_2 + k] = 0.0;
#endif 
			}
        }
    }

    for (i = 0; i <= numTasks; i++)
    {
		tArray1_g = a_g[i];
		tArray1_b = a_b[i];
		tArray2_g = b_g[i];
		tArray2_b = b_b[i];
#if VERIFICATION >= 1
		cArray1_g = a_CPU_g[i];
		cArray1_b = a_CPU_b[i];
		cArray2_g = b_CPU_g[i];
		cArray2_b = b_CPU_b[i];
#endif
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		for (k = 0; k < ASIZE_2; k++)
		{
			if( i==0 ) {
				tArray1_g[k] = 10.0;
				tArray2_g[k] = 10.0;
			} else {
				tArray1_g[k] = 0.0;
				tArray2_g[k] = 0.0;
			}
			if( i==numTasks ) {
				tArray1_b[k] = 10.0;
				tArray2_b[k] = 10.0;
			} else {
				tArray1_b[k] = 0.0;
				tArray2_b[k] = 0.0;
			}
#if VERIFICATION >= 1
			if( i==0 ) {
				cArray1_g[k] = 10.0;
				cArray2_g[k] = 10.0;
			} else {
				cArray1_g[k] = 0.0;
				cArray2_g[k] = 0.0;
			}
			if( i==numTasks ) {
				cArray1_b[k] = 10.0;
				cArray2_b[k] = 10.0;
			} else {
				cArray1_b[k] = 0.0;
				cArray2_b[k] = 0.0;
			}
#endif 
		}
    }

	for(i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		acc_create(a_m[i], ASIZE_2 * (tChunkSize-2) * sizeof(_FLOAT_));
		if(i == 0) {
			acc_copyin(a_g[i], ASIZE_2 * sizeof(_FLOAT_));
		} else {
			acc_create(a_g[i], ASIZE_2 * sizeof(_FLOAT_));
		}
		acc_create(a_b[i], ASIZE_2 * sizeof(_FLOAT_));
		acc_copyin(b_m[i], ASIZE_2 * (tChunkSize-2) * sizeof(_FLOAT_));
		acc_copyin(b_g[i], ASIZE_2 * sizeof(_FLOAT_));
		acc_copyin(b_b[i], ASIZE_2 * sizeof(_FLOAT_));
	}
	acc_create(a_g[numTasks], ASIZE_2 * sizeof(_FLOAT_));
	acc_copyin(a_b[numTasks], ASIZE_2 * sizeof(_FLOAT_));
	acc_copyin(b_g[numTasks], ASIZE_2 * sizeof(_FLOAT_));
	acc_copyin(b_b[numTasks], ASIZE_2 * sizeof(_FLOAT_));

    printf ("Performing %d iterations on a %d by %d array (nDeivces = %d, numTasks = %d, chunkSize = %d)\n", numItr, ASIZE, ASIZE, nDevices, numTasks, chunkSize);

    /* -- Timing starts before the main loop -- */
    printf("-------------------------------------------------------------\n");

    strt_time = my_timer ();

	is_even_itr = 1;
    for (m = 0; m < numItr; m++)
    {
#ifdef GRAPH
        HI_enter_subregion("jacobi-graph", 1);
#endif
		for( i = 0; i < numTasks; i++ ) {
            if( i == numTasks-1 ) {
                tChunkSize = lastChunkSize;
            } else {
                tChunkSize = chunkSize;
            }
			if( asyncMode == 0 ) {
				asyncID = acc_async_sync;
			} else if( asyncMode == 1 ) {
				asyncID = acc_async_noval;
            } else if( asyncMode == 2 ) {
                asyncID = i % nDevices;
            } else {
                asyncID = i;
            }
			if( is_even_itr == 1 ) {
				tArray1_bU = a_b[i];
				tArray1_m = a_m[i];
				tArray1_bD = a_b[i+1];
				tArray2_gU = b_g[i];
				tArray2_bU = b_b[i];
				tArray2_m = b_m[i];
				tArray2_bD = b_b[i+1];
				tArray2_gD = b_g[i+1];
			} else {
				tArray1_bU = b_b[i];
				tArray1_m = b_m[i];
				tArray1_bD = b_b[i+1];
				tArray2_gU = a_g[i];
				tArray2_bU = a_b[i];
				tArray2_m = a_m[i];
				tArray2_bD = a_b[i+1];
				tArray2_gD = a_g[i+1];
			}
			jacobi(tArray1_bU, tArray1_m, tArray1_bD, tArray2_gU, tArray2_bU, tArray2_m, tArray2_bD, tArray2_gD, tChunkSize, ASIZE, asyncID);
		}
#ifdef GRAPH
        HI_exit_subregion("jacobi-graph", 1);
#endif
		#pragma acc wait

		if( m==(numItr-1) ) {
#ifdef GRAPH
			HI_enter_subregion("jacobi-update-host-graph", 1);
#endif
			for( i = 0; i <= numTasks; i++ ) {
				if( i == numTasks-1 ) {
					tChunkSize = lastChunkSize;
				} else {
					tChunkSize = chunkSize;
				}
				if( asyncMode == 1 ) {
					asyncID = 1;
				} else if( asyncMode == 2 ) {
					asyncID = i % nDevices;
				} else {
					asyncID = i;
				}
				if( is_even_itr == 1 ) {
					tArray1_b = a_b[i];
					if( i<numTasks ) {
						tArray1_m = a_m[i];
					} 
				} else {
					tArray1_b = b_b[i];
					if( i<numTasks ) {
						tArray1_m = b_m[i];
					}
				}
				if( i<numTasks ) {
					#pragma acc update host(tArray1_m[0:(tChunkSize-2)*ASIZE_2]) async(asyncID)
				}
				#pragma acc update host(tArray1_b[0:ASIZE_2]) async(asyncID)
			}
#ifdef GRAPH
			HI_exit_subregion("jacobi-update-host-graph", 1);
#endif
			#pragma acc wait
		}
		if( is_even_itr == 1 ) {
			is_even_itr = 0;
		} else {
			is_even_itr = 1;
		}
    }

    done_time = my_timer ();
    printf ("Accelerator Elapsed time = %lf sec\n", done_time - strt_time);
	printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", done_time - strt_time);

#if VERIFICATION >= 1

    strt_time = my_timer ();

	is_even_itr = 1;
    for (m = 0; m < numItr; m++)
    {
		for( i = 0; i < numTasks; i++ ) {
            if( i == numTasks-1 ) {
                tChunkSize = lastChunkSize;
            } else {
                tChunkSize = chunkSize;
            }
            if( asyncMode == 1 ) {
                asyncID = 1;
            } else if( asyncMode == 2 ) {
                asyncID = i % nDevices;
            } else {
                asyncID = i;
            }
			if( is_even_itr == 1 ) {
				cArray1_bU = a_CPU_b[i];
				cArray1_m = a_CPU_m[i];
				cArray1_bD = a_CPU_b[i+1];
				cArray2_gU = b_CPU_g[i];
				cArray2_bU = b_CPU_b[i];
				cArray2_m = b_CPU_m[i];
				cArray2_bD = b_CPU_b[i+1];
				cArray2_gD = b_CPU_g[i+1];
			} else {
				cArray1_bU = b_CPU_b[i];
				cArray1_m = b_CPU_m[i];
				cArray1_bD = b_CPU_b[i+1];
				cArray2_gU = a_CPU_g[i];
				cArray2_bU = a_CPU_b[i];
				cArray2_m = a_CPU_m[i];
				cArray2_bD = a_CPU_b[i+1];
				cArray2_gD = a_CPU_g[i+1];
			}
			jacobi_cpu(cArray1_bU, cArray1_m, cArray1_bD, cArray2_gU, cArray2_bU, cArray2_m, cArray2_bD, cArray2_gD, tChunkSize, ASIZE, asyncID);
		}

		if( is_even_itr == 1 ) {
			is_even_itr = 0;
		} else {
			is_even_itr = 1;
		}
    }

    done_time = my_timer ();
    printf ("Reference CPU time = %lf sec\n", done_time - strt_time);
	is_even_itr = numItr%2;
#if VERIFICATION == 1
	{
		double cpu_sum = 0.0;
		double gpu_sum = 0.0;
    	double rel_err = 0.0;

		for( i = 0; i < numTasks; i++ ) {
			if( i == numTasks-1 ) {
				tChunkSize = lastChunkSize;
			} else {
				tChunkSize = chunkSize;
			}
			if( is_even_itr ) {
				tArray1_m = a_m[i];
				cArray1_m = a_CPU_m[i];
			} else {
				tArray1_m = b_m[i];
				cArray1_m = b_CPU_m[i];
			}
			for (j = 0; j < tChunkSize-2; j++)
			{
				cpu_sum += cArray1_m[j*ASIZE_2+j]*cArray1_m[j*ASIZE_2+j];
				gpu_sum += tArray1_m[j*ASIZE_2+j]*tArray1_m[j*ASIZE_2+j];
			}
		}

		cpu_sum = sqrt(cpu_sum);
		gpu_sum = sqrt(gpu_sum);
		if( cpu_sum > gpu_sum) {
			rel_err = (cpu_sum-gpu_sum)/cpu_sum;
		} else {
			rel_err = (gpu_sum-cpu_sum)/cpu_sum;
		}

		if(rel_err < 1e-9)
		{
	    	printf("Verification Successful err = %e\n", rel_err);
		}
		else
		{
	    	printf("Verification Fail err = %e\n", rel_err);
		}
	}
#else
	{
		double cpu_sum = 0.0;
		double gpu_sum = 0.0;
    	double rel_err = 0.0;
		int error_found = 0;

		for( i = 0; i < numTasks; i++ ) {
			if( i == numTasks-1 ) {
				tChunkSize = lastChunkSize;
			} else {
				tChunkSize = chunkSize;
			}
			if( is_even_itr ) {
				tArray1_m = a_m[i];
				cArray1_m = a_CPU_m[i];
			} else {
				tArray1_m = b_m[i];
				cArray1_m = b_CPU_m[i];
			}
			for (j = 0; j < (tChunkSize-2); j++)
			{
				for (k = 1; k <= ASIZE; k++)
				{
					cpu_sum = cArray1_m[j*ASIZE_2+k];
					gpu_sum = tArray_m[j*ASIZE_2+k];
					if( cpu_sum == gpu_sum ) {
						continue;
					}
					if( cpu_sum > gpu_sum) {
						if( cpu_sum == 0.0 ) {
							rel_err = cpu_sum-gpu_sum;
						} else {
							rel_err = (cpu_sum-gpu_sum)/cpu_sum;
						}
					} else {
						if( cpu_sum == 0.0 ) {
							rel_err = gpu_sum-cpu_sum;
						} else {
							rel_err = (gpu_sum-cpu_sum)/cpu_sum;
						}
					}
					if(rel_err < 0.0) {
						rel_err = -1*rel_err;
					}

					if(rel_err >= 1e-9)
					{
						error_found = 1;
						break;
					}
				}
				if( error_found == 1 ) {
					break;
				}
			}
			if( error_found == 1 ) {
				break;
			}
		}
		if( error_found == 0 )
		{
	    	printf("Verification Successful\n");
		}
		else
		{
	    	printf("Verification Fail err = %e\n", rel_err);
		}
	}
#endif
#endif


#ifdef CHECK_RESULT
	is_even_itr = numItr%2;
	for( i = 0; i < numTasks; i++ ) {
		if( i == numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		if( is_even_itr ) {
			tArray1_m = a_m[i];
		} else {
			tArray1_m = b_m[i];
		}
		for (j = 0; j < (tChunkSize-2); j++) {
			for (k = 1; k <= ASIZE; k++) {
				sum += tArray1_m[j*ASIZE_2+k];
				//printf("result[%d][%d] = %.10E\n", i, j*ASIZE_2+k, tArray1[j*ASIZE_2+k]);
			}
		}
	}
    printf("Sum = %.10E\n", sum);
#endif

	for(i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		acc_delete(a_m[i], ASIZE_2 * (tChunkSize-2) * sizeof(_FLOAT_));
		acc_delete(a_g[i], ASIZE_2 * sizeof(_FLOAT_));
		acc_delete(a_b[i], ASIZE_2 * sizeof(_FLOAT_));
		acc_delete(b_m[i], ASIZE_2 * (tChunkSize-2) * sizeof(_FLOAT_));
		acc_delete(b_g[i], ASIZE_2 * sizeof(_FLOAT_));
		acc_delete(b_b[i], ASIZE_2 * sizeof(_FLOAT_));
	}
	acc_delete(a_g[numTasks], ASIZE_2 * sizeof(_FLOAT_));
	acc_delete(a_b[numTasks], ASIZE_2 * sizeof(_FLOAT_));
	acc_delete(b_g[numTasks], ASIZE_2 * sizeof(_FLOAT_));
	acc_delete(b_b[numTasks], ASIZE_2 * sizeof(_FLOAT_));

    return 0;
}

