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

void jacobi(_FLOAT_ *outArray, _FLOAT_ *inArray, size_t tChunkSize, size_t ASIZE, int asyncID) {
	size_t i, j;
	size_t ASIZE_2 = ASIZE + 2;
	size_t numElements = (tChunkSize+2)*(ASIZE_2);
#pragma acc parallel loop gang present(outArray[0:numElements], inArray[0:numElements]) private(i) async(asyncID)
	for (i = 1; i <= tChunkSize; i++)
	{
#pragma acc loop worker private(j)
		for (j = 1; j <= ASIZE; j++)
		{
			outArray[i*ASIZE_2+j] = (inArray[(i - 1)*ASIZE_2+j] + inArray[(i + 1)*ASIZE_2+j] + inArray[i*ASIZE_2+(j - 1)] + inArray[i*ASIZE_2+(j + 1)]) / 4.0f;
		}
	}
}

void jacobi_cpu(_FLOAT_ *outArray, _FLOAT_ *inArray, size_t tChunkSize, size_t ASIZE) {
	size_t i, j;
	size_t ASIZE_2 = ASIZE + 2;
#pragma omp parallel for private(i,j)
	for (i = 1; i <= tChunkSize; i++)
	{
		for (j = 1; j <= ASIZE; j++)
		{
			outArray[i*ASIZE_2+j] = (inArray[(i - 1)*ASIZE_2+j] + inArray[(i + 1)*ASIZE_2+j] + inArray[i*ASIZE_2+(j - 1)] + inArray[i*ASIZE_2+(j + 1)]) / 4.0f;
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
	_FLOAT_ **a = NULL;
	_FLOAT_ **b = NULL;
	_FLOAT_ *tArray1 = NULL;
	_FLOAT_ *tArray2 = NULL;
	_FLOAT_ *tmpArray = NULL;
#if VERIFICATION >= 1
	_FLOAT_ **a_CPU = NULL;
	_FLOAT_ **b_CPU = NULL;
	_FLOAT_ *cArray1 = NULL;
	_FLOAT_ *cArray2 = NULL;
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


	a = (_FLOAT_ **)malloc(numTasks * sizeof(_FLOAT_ *));
	b = (_FLOAT_ **)malloc(numTasks * sizeof(_FLOAT_ *));
	for(i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		a[i] = (_FLOAT_ *)malloc(ASIZE_2 * (tChunkSize+2) * sizeof(_FLOAT_));
		b[i] = (_FLOAT_ *)malloc(ASIZE_2 * (tChunkSize+2) * sizeof(_FLOAT_));
	}

#if VERIFICATION >= 1
	a_CPU = (_FLOAT_ **)malloc(numTasks * sizeof(_FLOAT_ *));
	b_CPU = (_FLOAT_ **)malloc(numTasks * sizeof(_FLOAT_ *));
	for(i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		a_CPU[i] = (_FLOAT_ *)malloc(ASIZE_2 * (tChunkSize+2) * sizeof(_FLOAT_));
		b_CPU[i] = (_FLOAT_ *)malloc(ASIZE_2 * (tChunkSize+2) * sizeof(_FLOAT_));
	}

#endif 

    for (i = 0; i < numTasks; i++)
    {
		tArray1 = a[i];
		tArray2 = b[i];
#if VERIFICATION >= 1
		cArray1 = a_CPU[i];
		cArray2 = b_CPU[i];
#endif
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
        for (j = 0; j < (tChunkSize+2); j++)
        {
        	for (k = 0; k < ASIZE_2; k++)
        	{
            	tArray1[j*ASIZE_2 + k] = 0.0;
            	tArray2[j*ASIZE_2 + k] = 0.0;
#if VERIFICATION >= 1
            	cArray1[j*ASIZE_2 + k] = 0.0;
            	cArray2[j*ASIZE_2 + k] = 0.0;
#endif 
			}
        }
    }

    for (i = 0; i < numTasks; i++)
    {
		tArray2 = b[i];
#if VERIFICATION >= 1
		cArray2 = b_CPU[i];
#endif
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
        for (j = 0; j < (tChunkSize+2); j++)
        {
            tArray2[j*ASIZE_2] = 10.0;
            tArray2[j*ASIZE_2 + ASIZE_1] = 10.0;
#if VERIFICATION >= 1
            cArray2[j*ASIZE_2] = 10.0;
            cArray2[j*ASIZE_2 + ASIZE_1] = 10.0;
#endif 
        }
    }

    for (i = 0; i < numTasks; i++)
    {
		tArray2 = b[i];
#if VERIFICATION >= 1
		cArray2 = b_CPU[i];
#endif
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		if( i==0 ) {
			for (k = 0; k < ASIZE_2; k++)
			{
				tArray2[k] = 10.0;
#if VERIFICATION >= 1
				cArray2[k] = 10.0;
#endif 
			}
		}
		if( i==numTasks-1 ) {
			for (k = 0; k < ASIZE_2; k++)
			{
				tArray2[(tChunkSize+1)*ASIZE_2 + k] = 10.0;
#if VERIFICATION >= 1
				cArray2[(tChunkSize+1)*ASIZE_2 + k] = 10.0;
#endif 
			}
		}
    }

	for(i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) {
			tChunkSize = lastChunkSize;
		} else {
			tChunkSize = chunkSize;
		}
		acc_copyin(a[i], ASIZE_2 * (tChunkSize+2) * sizeof(_FLOAT_));
		acc_copyin(b[i], ASIZE_2 * (tChunkSize+2) * sizeof(_FLOAT_));
	}

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
				tArray1 = a[i];
				tArray2 = b[i];
			} else {
				tArray1 = b[i];
				tArray2 = a[i];
			}
			jacobi(tArray1, tArray2, tChunkSize, ASIZE, asyncID);
		}
#ifdef GRAPH
        HI_exit_subregion("jacobi-graph", 1);
#endif
		#pragma acc wait
		//Exchange boundary data if there are more than one task.
		if( numTasks > 1 ) {
#ifdef GRAPH
        	HI_enter_subregion("jacobi-comm-graph", 1);
#endif
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
					tmp1 = (_FLOAT_ *)acc_deviceptr(a[i]);
				} else {
					tmp1 = (_FLOAT_ *)acc_deviceptr(b[i]);
				}
				if( i < (numTasks-1) ) {
					if( is_even_itr == 1 ) {
						tmp2 = (_FLOAT_ *)acc_deviceptr(a[i+1]);
					} else {
						tmp2 = (_FLOAT_ *)acc_deviceptr(b[i+1]);
					}
				}
				if( i < (numTasks-1) ) {
					acc_memcpy_device_async(tmp2, tmp1 + tChunkSize*ASIZE_2, ASIZE_2*sizeof(_FLOAT_), asyncID);
					acc_memcpy_device_async(tmp1 + (tChunkSize+1)*ASIZE_2, tmp2 + 1, ASIZE_2*sizeof(_FLOAT_), asyncID);
				} 
			}
#ifdef GRAPH
        	HI_exit_subregion("jacobi-comm-graph", 1);
#endif
			#pragma acc wait
		}
		if( m==(numItr-1) ) {
#ifdef GRAPH
			HI_enter_subregion("jacobi-update-host-graph", 1);
#endif
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
					tArray1 = a[i];
				} else {
					tArray1 = b[i];
				}
				#pragma acc update host(tArray1[0:(tChunkSize+2)*ASIZE_2]) async(asyncID)
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
    for (m = 0; m < numItr; m++) {
		for( i = 0; i < numTasks; i++ ) {
			if( i == numTasks-1 ) {
				tChunkSize = lastChunkSize;
			} else {
				tChunkSize = chunkSize;
			}
			if( is_even_itr == 1 ) {
				cArray1 = a_CPU[i];
				cArray2 = b_CPU[i];
			} else {
				cArray1 = b_CPU[i];
				cArray2 = a_CPU[i];
			}
			jacobi_cpu(cArray1, cArray2, tChunkSize, ASIZE);
		}
		//Exchange boundary data if there are more than one task.
		if( numTasks > 1 ) {
			for( i = 0; i < numTasks; i++ ) {
				if( i == numTasks-1 ) {
					tChunkSize = lastChunkSize;
				} else {
					tChunkSize = chunkSize;
				}
				if( is_even_itr == 1 ) {
					tmp1 = a_CPU[i];
				} else {
					tmp1 = b_CPU[i];
				}
				if( i < (numTasks-1) ) {
					if( is_even_itr == 1 ) {
						tmp2 = a_CPU[i+1];
					} else {
						tmp2 = b_CPU[i+1];
					}
				}
				if( i < (numTasks-1) ) {
					memcpy(tmp2, tmp1 + tChunkSize*ASIZE_2, ASIZE_2*sizeof(_FLOAT_));
					memcpy(tmp1 + (tChunkSize+1)*ASIZE_2, tmp2 + 1, ASIZE_2*sizeof(_FLOAT_));
				} 
			}
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
				tArray1 = a[i];
				cArray1 = a_CPU[i];
			} else {
				tArray1 = b[i];
				cArray1 = b_CPU[i];
			}
			for (j = 1; j <= tChunkSize; j++)
			{
				cpu_sum += cArray1[j*ASIZE_2+j]*cArray1[j*ASIZE_2+j];
				gpu_sum += tArray1[j*ASIZE_2+j]*tArray1[j*ASIZE_2+j];
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
				tArray1 = a[i];
				cArray1 = a_CPU[i];
			} else {
				tArray1 = b[i];
				cArray1 = b_CPU[i];
			}
			for (j = 1; j <= tChunkSize; j++)
			{
				for (k = 1; k <= ASIZE; k++)
				{
					cpu_sum = cArray1[j*ASIZE_2+k];
					gpu_sum = tArray[j*ASIZE_2+k];
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
			tArray1 = a[i];
		} else {
			tArray1 = b[i];
		}
		for (j = 1; j <= tChunkSize; j++) {
			for (k = 1; k <= ASIZE; k++) {
				sum += tArray1[j*ASIZE_2+k];
				//printf("result[%d][%d] = %.10E\n", i, j*ASIZE_2+k, tArray1[j*ASIZE_2+k]);
			}
		}
	}
    printf("Sum = %.10E\n", sum);
#endif

    return 0;
}

