#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "arcutil.h"
#include "openacc.h"

#if defined(_OPENMP)
#include <omp.h>
#endif

//////////////////////////
// Macros for debugging //
//////////////////////////
//#define DEBUG_ON1
#if DEBUG_PRINT == 1
#define DEBUG_ON2
#endif

#ifndef SPMUL_INPUTDIR
#define SPMUL_INPUTDIR        "/home/f6l/SPMULInput/"
#endif
#define ITER	100

/*
#define INPUTFILE  "nlpkkt240.rbC"
#define ASIZE  27993600
#define ASIZE2  27993600 //debugging purpose (should be replaced with ASIZE)
#define NZR    401232976
#ifdef _OPENARC_
#pragma openarc #define ASIZE  27993600
#pragma openarc #define ASIZE2  27993600 //debugging purpose (should be replaced with ASIZE)
#pragma openarc #define NZR    401232976
#endif
*/

/*
#define INPUTFILE  "af23560.rbC"
#define ASIZE  23560
#define ASIZE2  23560 //debugging purpose (should be replaced with ASIZE)
#define NZR    484256
#ifdef _OPENARC_
#pragma openarc #define ASIZE  23560
#pragma openarc #define ASIZE2  23560 //debugging purpose (should be replaced with ASIZE)
#pragma openarc #define NZR    484256
#endif
*/

/*
#define INPUTFILE	"rajat31.rbC"
#define ASIZE	4690002
#define ASIZE2	4690002 //debugging purpose (should be replaced with ASIZE)
#define NZR		20316253
#ifdef _OPENARC_
#pragma openarc #define ASIZE  4690002
#pragma openarc #define ASIZE2  4690002 //debugging purpose (should be replaced with ASIZE)
#pragma openarc #define NZR    20316253
#endif
*/

/*
#define INPUTFILE	"af_shell10.rbC"
#define ASIZE	1508065
#define ASIZE2	1508065 //debugging purpose (should be replaced with ASIZE)
#define NZR		27090195
#ifdef _OPENARC_
#pragma openarc #define ASIZE  1508065
#pragma openarc #define ASIZE2  1508065 //debugging purpose (should be replaced with ASIZE)
#pragma openarc #define NZR    27090195
#endif
*/

/*
#define INPUTFILE   "hood.rbC"
#define ASIZE    220542  
#define ASIZE2   220542  
#define NZR 5494489 
#ifdef _OPENARC_
#pragma openarc #define ASIZE	220542
#pragma openarc #define ASIZE2	220542 //debugging purpose (should be replaced with ASIZE)
#pragma openarc #define NZR		5494489
#endif
*/

#define INPUTFILE	"kkt_power.rbC"
#define ASIZE	2063494
#define ASIZE2	2063494 //debugging purpose (should be replaced with ASIZE)
#define NZR		8130343
#ifdef _OPENARC_
#pragma openarc #define ASIZE	2063494
#pragma openarc #define ASIZE2	2063494 //debugging purpose (should be replaced with ASIZE)
#pragma openarc #define NZR		8130343
#endif

/*
#define ITER	500
#define INPUTFILE	"msdoor.rbC"
#define ASIZE	415863
#define ASIZE2	415863 
#define NZR		10328399
*/

/*
#define INPUTFILE	"appu.rbC"
//#define INPUTFILE	"appu.rbCR"
//#define INPUTFILE	"appu.rbCRP"
#define ASIZE	14000
#define ASIZE2	14000 
#define NZR		1853104
//#define NZR		1857600
*/

/*
#define INPUTFILE	"nd24k.rbC"
#define ASIZE	72000
#define ASIZE2	72000 
#define NZR		14393817

//#define INPUTFILE	"F1.rbC"
#define INPUTFILE	"F1.rbCRP"
#define ASIZE	343791
#define ASIZE2	343791 
//#define NZR		13590452
#define NZR		13596431

//#define INPUTFILE	"ASIC_680k.rbC"
#define INPUTFILE	"ASIC_680k.rbCR"
#define ASIZE	682862
#define ASIZE2	682862
#define NZR		3871773

#define INPUTFILE	"ASIC_680ks.rbC"
#define ASIZE	682712
#define ASIZE2	682712 
#define NZR		2329176

#define INPUTFILE	"crankseg_2.rbC"
#define ASIZE	63838
#define ASIZE2	63838 
#define NZR		7106348

#define INPUTFILE	"darcy003.rbC"
#define ASIZE	389874
#define ASIZE2	389874 
#define NZR		1167685

#define INPUTFILE	"Si41Ge41H72.rbC"
#define ASIZE	185639
#define ASIZE2	185639 
#define NZR		7598452

#define INPUTFILE	"SiO2.rbC"
#define ASIZE	155331
#define ASIZE2	155331 
#define NZR		5719417
*/
/*
#define INPUTFILE   "sparsine.rbCR"
#define ASIZE    50000
#define ASIZE2   50000
#define NZR 799494

#define INPUTFILE   "sparsine.rbCRPF"
#define ASIZE    50000   
#define ASIZE2   50000   
#define NZR 3200000 
*/
/*
#define INPUTFILE   "ns3Da.rbCRPF"
#define ASIZE    20414   
#define ASIZE2   20414   
#define NZR 6533120 
*/

/*
#define INPUTFILE   "af23560.rand51M"
#define ASIZE    100000  
#define ASIZE2   100000
#define NZR 6400000
*/

/*
#define INPUTFILE   "af23560.rand200M"
#define ASIZE    100000  
#define ASIZE2   100000
#define NZR 25600000
*/

/*
int colind[NZR];
int rowptr[ASIZE+1];
float values[NZR];
float x[ASIZE];
float y[ASIZE];
*/
int *colind;
int *rowptr;
float *values;
float *x;
float *y;

/*
 * This version (version 1) distributes the output vector y only.
*/
void spmul(int *colind, int *rowptr, float *values, float *x, float *y, unsigned int numElements, int taskID, unsigned int start_index, int asyncID) {
	int i, j;
#ifdef MERGE_TASKS
    HI_enter_subregion("spmul", 0);
#endif
    #pragma acc update device(x[0:ASIZE]) async(asyncID)
#pragma acc kernels loop gang, worker present(x[0:ASIZE], values[0:NZR], colind[0:NZR], rowptr[0:ASIZE+1], y[0:numElements])
	for( i=0; i<numElements; i++ ) { 
		float tmp = 0.0f;
		for( j=0; j<(rowptr[1+i+start_index]-rowptr[i+start_index]); j++ ) { 
			tmp = tmp + values[rowptr[i+start_index]+j-1]*x[colind[rowptr[i+start_index]+j-1]-1];
		}   
		if( tmp != 0.0f ) {
			int exp0 = (int)(log10f(fabsf(tmp)));
			if( exp0 >= 0 ) {
				for( j=1; j<=(1+exp0); j++ ) {
					tmp = tmp/10.0f;
				}
			} else if( exp0 <= -1 ) {
				j = -1;
				for( j=1; j<=(-exp0); j++ ) {
					tmp = 10.0f*tmp;
				}
			} 
		}
		y[i] = tmp;
	}
    #pragma acc update host(y[0:numElements]) async(asyncID)
#ifdef MERGE_TASKS
    HI_exit_subregion("spmul", 0);
#endif
}

int main(int argc, char *argv[]) {
    unsigned int chunkSize = 0;
    unsigned int lastChunkSize = 0;
	unsigned int numElements = 0;
    unsigned int numTasks = 0;
    unsigned int nDevices = 1;
    unsigned int asyncMode = 2;
	int asyncID;
	int error;

	FILE *fp10;
	//FILE *fp12; //Result writing part is disabled
	char filename1[96] = SPMUL_INPUTDIR; 
	char filename2[32] = INPUTFILE; 

	float temp, x_sum;
	double s_time1, e_time1, s_time2, e_time2;
	double s_time3, e_time3;
	int exp0, i, j, k;
	int r_ncol, r_nnzero, r_nrow;
	int cpumemsize = 0;

	error = parse_inputs(argc, argv, &chunkSize, &lastChunkSize, &numTasks, &nDevices, &asyncMode);
    if( error > 0 ) { exit(1); }

    if( chunkSize == 0 ) {
        chunkSize = ASIZE/nDevices;
    }
    numTasks = ASIZE/chunkSize;
    if( ASIZE%chunkSize != 0 ) {
        lastChunkSize = chunkSize + (ASIZE - (numTasks)*chunkSize);
    } else {
        lastChunkSize = chunkSize;
    }

	colind = (int *)malloc(sizeof(int)*NZR);	
	rowptr = (int *)malloc(sizeof(int)*(ASIZE+1));	
	values = (float *)malloc(sizeof(float)*NZR);	
	x = (float *)malloc(sizeof(float)*ASIZE);	
	y = (float *)malloc(sizeof(float)*ASIZE);	

	acc_create(x, ASIZE*sizeof(float));
    for( i = 0; i < numTasks; i++ ) { 
        if( i==numTasks-1 ) { 
            numElements = lastChunkSize;
        } else {
            numElements = chunkSize;
        }   
        acc_create(y + i*chunkSize, numElements*sizeof(float));
    }   

	printf("**** SerialSpmul starts! ****\n");

#if defined(_OPENMP)
	omp_set_num_threads(8);
#endif
	strcat(filename1, filename2);

	printf("Input file: %s\n", filename2);

	s_time1 = my_timer();
	s_time2 = my_timer();
	if( (fp10 = fopen(filename1, "r")) == NULL ) {
		printf("FILE %s DOES NOT EXIST; STOP\n", filename1);
		exit(1);
	}
/*
	if( (fp12 = fopen("spmulSP.out", "w")) == NULL ) {
		exit(1);
	}
*/
	printf("FILE open done\n");

	fscanf(fp10, "%d %d %d", &r_nrow, &r_ncol, &r_nnzero);
	if (r_nrow != ASIZE) {
		printf("alarm: incorrect row\n");
		exit(1);
	}
	if (r_ncol != ASIZE) {
		printf("alarm: incorrect col\n");
		exit(1);
	}
	if (r_nnzero != NZR) {
		printf("alarm: incorrect nzero\n");
		exit(1);
	}
	for( i=0; i<=ASIZE; i++ ) {
		fscanf(fp10, "%d", (rowptr+i));
	}

	for( i=0; i<NZR; i++ ) {
		fscanf(fp10, "%d", (colind+i));
	}

	for( i=0; i<NZR; i++ ) {
		fscanf(fp10, "%E", (values+i)); //for float variables
	}
	fclose(fp10);

	j = 0;
    for( i=0; i<ASIZE; i++ ) {
LB99:
		temp = values[j];
		if( ((-0.1f) < temp)&&(temp < 0.1f) ) {
			j += 1;
			//goto LB99;
			//Added by SYLee
			if( temp == 0.0f )
				goto LB99;
			x[i] = temp; 
			continue;
		}
		exp0 = (int)(log10f(fabsf(temp)));
		x[i] = temp;
		if( (-exp0) <= 0 ) {
			for( k=1; k<=(1+exp0); k++ ) {
				x[i] = x[i]/10.0f;
			}
		} else if( (1+exp0) <= 0 ) {
			k = -1;
			for( k=1; k<=(-exp0); k++ ) {
				x[i] = 10.0f*x[i];
			}
		}
		if( (1.0f < x[i])||(x[i] < (-1.0f)) ) {
			printf("alarm initial i = %d\n", i);
			printf("x = %E\n", x[i]); 
			printf("value = %E\n", values[1000+i]);
			printf("exp = %d\n", exp0);
			exit(1);
		}
		j += 1;
	}

#ifdef DEBUG_ON1
	x_sum = 0.0f;
	for( i=0; i<ASIZE; i++ ) {
		x_sum += x[i];
	}
	printf("0: x_sum = %.12E\n", x_sum);
#endif
	cpumemsize += sizeof(int) * (NZR + ASIZE + 1);
	cpumemsize += sizeof(float) * (NZR + 2*ASIZE);
	printf("Used CPU memory: %d bytes\n", cpumemsize);

	printf("initialization done\n");
	e_time2 = my_timer();
	s_time3 = my_timer();

#pragma acc data copyin(values[0:NZR], colind[0:NZR], rowptr[0:ASIZE+1])
	for( k=0; k<ITER; k++ ) {
#ifdef GRAPH
        HI_enter_subregion("spmul-graph", 1);
#endif
        for( i = 0; i < numTasks; i++ ) {
            if( i == numTasks-1 ) {
                numElements = lastChunkSize;
            } else {
                numElements = chunkSize;
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
			spmul(colind, rowptr, values, x, y + i*chunkSize, numElements, i, i*chunkSize, asyncID);
		}
#ifdef GRAPH
        HI_exit_subregion("spmul-graph", 1);
#endif
		#pragma acc wait
        for( i = 0; i < numTasks; i++ ) {
            if( i == numTasks-1 ) {
                numElements = lastChunkSize;
            } else {
                numElements = chunkSize;
            }
			memcpy(x + i*chunkSize, y + i*chunkSize, numElements*sizeof(float));
		}
	}

	e_time3 = my_timer();
	e_time1 = my_timer();
	printf("chunkSize:%u, nDevices:%u, asyncMode:%u\n", chunkSize, nDevices, asyncMode);
	printf("Total time = %f seconds\n", (e_time1 - s_time1));
	printf("Initialize time = %f seconds\n", (e_time2 - s_time2));
	printf("Accelerator Elapsed time = %f seconds\n", (e_time3 - s_time3));
	printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", (e_time3 - s_time3));

#ifdef DEBUG_ON2
	x_sum = 0.0f;
	for( i=0; i<ASIZE2; i++ ) {
		x_sum += x[i];
	}
	printf("%d: x_sum = %.12E\n",(k+1), x_sum);
#endif

/*
	for( i=0; i< ASIZE; i++ ) {
		fprintf(fp12, "%.9E\n", x[i]);
	} 

	fclose(fp12);
*/
	acc_delete(x, ASIZE*sizeof(float));
    for( i = 0; i < numTasks; i++ ) { 
        if( i==numTasks-1 ) { 
            numElements = lastChunkSize;
        } else {
            numElements = chunkSize;
        }   
        acc_delete(y + i*chunkSize, numElements*sizeof(float));
    }   

	return 0;
}
