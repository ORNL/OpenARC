#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#if OMP == 1
#include <omp.h>
#endif

#ifndef _FLOAT_
#define _FLOAT_ double
#endif

#ifndef VERIFICATION
#define VERIFICATION 0
#endif

#ifndef ASIZE
#define ASIZE    8192 //256 * 32
#ifdef _OPENARC_
#pragma openarc #define ASIZE 8192
#pragma openarc #define ASIZE_2 (ASIZE+2)
#endif
#endif

#define ASIZE_1 	(ASIZE+1)
#define ASIZE_2 	(ASIZE+2)

#define CHECK_RESULT

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

void jacobi(_FLOAT_ (*outArray)[ASIZE_2], _FLOAT_ (*inArray)[ASIZE_2]) {
	size_t i, j;
#pragma acc parallel loop gang present(outArray[0:ASIZE_2][0:ASIZE_2], inArray[0:ASIZE_2][0:ASIZE_2])
	for (i = 1; i <= ASIZE; i++)
	{
#pragma acc loop worker
		for (j = 1; j <= ASIZE; j++)
		{
			outArray[i][j] = (inArray[i - 1][j] + inArray[i + 1][j] + inArray[i][j - 1] + inArray[i][j + 1]) / 4.0f;
		}
	}
}

void jacobi_cpu(_FLOAT_** outArray, _FLOAT_** inArray) {
	size_t i, j;
#pragma omp parallel for shared(outArray,inArray) private(i,j)
	for (i = 1; i <= ASIZE; i++)
	{
		for (j = 1; j <= ASIZE; j++)
		{
			outArray[i][j] = (inArray[i - 1][j] + inArray[i + 1][j] + inArray[i][j - 1] + inArray[i][j + 1]) / 4.0f;
		}
	}
}

int main (int argc, char *argv[])
{
    size_t i, j, k;
	unsigned int numItr = 1;
    //int c;
    _FLOAT_ sum = 0.0f;

    if( argc > 1 ) {
        i = 1;
        while( i<argc ) {
            int ok;
            if(strcmp(argv[i], "-i") == 0) {
                if (i+1 >= argc) {
                    printf("Missing integer argument to -i");
                }
                ok = StrToInt(argv[i+1], &(numItr));
                if(!ok) {
                    printf("Parse Error on option -i integer value required after argument\n");
                }
                i+=2;
            } else if(strcmp(argv[i], "-h") == 0) {
                printf("==> Available commandline options:\n");
                printf("    -h    #print help message\n");
                printf("    -i I  #set the value of numItr with I\n");
                exit(0);
            } else {
                printf("Invalid commandline option: %s\n", argv[i]);
                exit(1);
            }
        }
    }


    double strt_time, done_time;
	_FLOAT_ (*a)[ASIZE_2] = (_FLOAT_ (*)[ASIZE_2])malloc(sizeof(_FLOAT_)*ASIZE_2*ASIZE_2);
	_FLOAT_ (*b)[ASIZE_2] = (_FLOAT_ (*)[ASIZE_2])malloc(sizeof(_FLOAT_)*ASIZE_2*ASIZE_2);
	_FLOAT_ (*tArray1)[ASIZE_2] = NULL;
	_FLOAT_ (*tArray2)[ASIZE_2] = NULL;
	_FLOAT_ (*tmpArray)[ASIZE_2] = NULL;
#if VERIFICATION >= 1
	_FLOAT_** a_CPU = (_FLOAT_**)malloc(sizeof(_FLOAT_*) * ASIZE_2);
	_FLOAT_** b_CPU = (_FLOAT_**)malloc(sizeof(_FLOAT_*) * ASIZE_2);
	_FLOAT_** cArray1 = NULL;
	_FLOAT_** cArray2 = NULL;
	_FLOAT_** cTmpArray = NULL;

	_FLOAT_* a_data = (_FLOAT_*)malloc(sizeof(_FLOAT_) * ASIZE_2 * ASIZE_2);
	_FLOAT_* b_data = (_FLOAT_*)malloc(sizeof(_FLOAT_) * ASIZE_2 * ASIZE_2);

	for(i = 0; i < ASIZE_2; i++)
	{
		a_CPU[i] = &a_data[i * ASIZE_2];
		b_CPU[i] = &b_data[i * ASIZE_2];
	}

#endif 

    //while ((c = getopt (argc, argv, "")) != -1);

    for (i = 0; i < ASIZE_2; i++)
    {
        for (j = 0; j < ASIZE_2; j++)
        {
            b[i][j] = 0;
#if VERIFICATION >= 1
			b_CPU[i][j] = 0;
#endif 
        }
    }

    for (j = 0; j <= ASIZE_1; j++)
    {
        b[j][0] = 1.0;
        b[j][ASIZE_1] = 1.0;

#if VERIFICATION >= 1
		b_CPU[j][0] = 1.0;
		b_CPU[j][ASIZE_1] = 1.0;
#endif 

    }
    for (i = 1; i <= ASIZE; i++)
    {
        b[0][i] = 1.0;
        b[ASIZE_1][i] = 1.0;

#if VERIFICATION >= 1
		b_CPU[0][i] = 1.0;
		b_CPU[ASIZE_1][i] = 1.0;
#endif 
    }

    printf ("Performing %d iterations on a %d by %d array\n", numItr, ASIZE, ASIZE);

    /* -- Timing starts before the main loop -- */
    printf("-------------------------------------------------------------\n");

    strt_time = my_timer ();

	tArray1 = a;
	tArray2 = b;
#pragma acc data copyin(b[0:ASIZE_2][0:ASIZE_2]), create(a[0:ASIZE_2][0:ASIZE_2])
    for (k = 0; k < numItr; k++)
    {
		
		jacobi(tArray1, tArray2);
		tmpArray = tArray1;
		tArray1 = tArray2;
		tArray2 = tmpArray;
		if( k==(numItr-1) ) {
			#pragma acc update host(tArray2[0:ASIZE_2][0:ASIZE_2])
		}
    }

    done_time = my_timer ();
    printf ("Accelerator Elapsed time = %lf sec\n", done_time - strt_time);
	printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", done_time - strt_time);

#if VERIFICATION >= 1

    strt_time = my_timer ();

	cArray1 = a_CPU;
	cArray2 = b_CPU;
    for (k = 0; k < numItr; k++)
    {
		
		jacobi_cpu(cArray1, cArray2);
		cTmpArray = cArray1;
		cArray1 = cArray2;
		cArray2 = cTmpArray;
    }

    done_time = my_timer ();
    printf ("Reference CPU time = %lf sec\n", done_time - strt_time);
#if VERIFICATION == 1
	{
		double cpu_sum = 0.0;
		double gpu_sum = 0.0;
    	double rel_err = 0.0;

		for (i = 1; i <= ASIZE; i++)
    	{
        	cpu_sum += cArray2[i][i]*cArray2[i][i];
			gpu_sum += tArray2[i][i]*tArray2[i][i];
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

        for (i = 1; i <= ASIZE; i++)
        {
            for (j = 1; j <= ASIZE; j++)
            {
        		cpu_sum = cArray2[i][j];
				gpu_sum = tArray2[i][j];
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
    for (i = 1; i <= ASIZE; i++)
    {
        sum += tArray2[i][i];
    }
    printf("Diagonal sum = %.10E\n", sum);
#endif

    return 0;
}

