#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "openacc.h"

#ifndef _FLOAT_
#define _FLOAT_ double
#endif

// complexity :  nb_iters*resolution^2

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

 #pragma acc routine
_FLOAT_ mdb(_FLOAT_ x0, _FLOAT_ y0, unsigned int numItr) {

_FLOAT_ temp;
_FLOAT_ x=x0;
_FLOAT_ y=y0;

#pragma acc loop seq
for(int i=0; i < numItr; i++) {
    temp = x;
    x = x*x - y*y + x0;
    y = 2*temp*y + y0;
}

return x*x+y*y;
}

void run_mdb(_FLOAT_ *res, unsigned int numElements, unsigned int resolution, unsigned int offset, unsigned int numItr, int asyncID) {
	int i, j;
#ifdef MERGE_TASKS
    HI_enter_subregion("mdb", 0);
#endif
	#pragma acc parallel loop present(res[0:numElements*resolution]) 
	for(i=0; i<numElements; i++) { 
 		#pragma acc loop 
		for(j=0; j<resolution; j++) 
		{
			_FLOAT_ x = -2.0+(4.0/resolution)*(i+offset);
			_FLOAT_ y = -2.0+(4.0/resolution)*j;
			res[i*resolution+j] = mdb(x,y, numItr);
		}
	}
    #pragma acc update host(res[0:numElements*resolution]) async(asyncID)
#ifdef MERGE_TASKS
    HI_exit_subregion("mdb", 0);
#endif
}

double my_timer ()
{
    struct timeval time;
    gettimeofday (&time, 0); 
    return time.tv_sec + time.tv_usec / 1000000.0;
}

int main(int argc, char** argv) {

	unsigned int resolution = 5000;
	unsigned int incr;
	_FLOAT_ *res;
	int i,j;
	unsigned int numItr = 40000;
	unsigned int chunkSize = 0;
    unsigned int lastChunkSize = 0;
    unsigned int numTasks = 0;
    unsigned int nDevices = 1;
	unsigned int numElements;
	unsigned int offset = 0;
	unsigned int asyncMode = 2;
    int asyncID;
	double strt_time, done_time;

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
            } else if(strcmp(argv[i], "-r") == 0) {
                if (i+1 >= argc) {
                    printf("Missing integer argument to -r");
                }   
                ok = StrToInt(argv[i+1], &(resolution));
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
                printf("    -r R  #set the value of resolution with R\n");
                printf("    -d D  #set the value of nDevices with D\n");
                printf("    -c C  #set the value of chunkSize with C\n");
                printf("    -i I  #set the value of numItr with I\n");
                printf("    -am AM  #set the asynchronous mode\n");
                printf("            #1 : use a single queue\n");
                printf("            #2 : use nDevices number of queues\n");
                printf("            #3 : use as many number of queues as total asynchronous tasks\n");
                exit(0);
            } else {
                printf("Invalid commandline option: %s\n", argv[i]);
                exit(1);
            }
        }
    }

    if( chunkSize == 0 ) {
        chunkSize = resolution/nDevices;
    }
    numTasks = resolution/chunkSize;
    if( resolution%chunkSize != 0 ) {
        lastChunkSize = chunkSize + (resolution - (numTasks)*chunkSize);
    } else {
        lastChunkSize = chunkSize;
    }

	incr = resolution/100;

	res = (_FLOAT_ *)malloc(sizeof(_FLOAT_)*resolution*resolution);

	strt_time = my_timer ();

	for(i=0; i<numTasks; i++) {
		if(i==numTasks-1) {
			numElements = lastChunkSize*resolution;
		} else {
			numElements = chunkSize*resolution;
		}   
		acc_create(res + i*chunkSize*resolution, numElements*sizeof(_FLOAT_));
	}   

#ifdef GRAPH
    HI_enter_subregion("mdb-graph", 1);
#endif
	offset = 0;
	for(i=0; i<numTasks; i++) {
		if(i==numTasks-1) {
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
		run_mdb(res +i*chunkSize*resolution, numElements, resolution, offset, numItr, asyncID);
		offset += numElements;
	}
#ifdef GRAPH
    HI_exit_subregion("mdb-graph", 1);
#endif
	#pragma acc wait

	done_time = my_timer ();

	//print
	printf("\n");
	for(i=incr*22; i<(resolution-22*incr); i+=incr)
	{
		for(j=incr*5; j<resolution; j+=incr) {
 			if (res[j*resolution+i] < 4) {
 				printf("X");
 			} else {
 				printf(" ");
 			}
		}
		printf("\n");
	}

	for(i=0; i<numTasks; i++) {
		if(i==numTasks-1) {
			numElements = lastChunkSize*resolution;
		} else {
			numElements = chunkSize*resolution;
		}   
		acc_delete(res + i*chunkSize*resolution, numElements*sizeof(_FLOAT_));
	}   

	printf("\nResolution = %d\n", resolution);
	printf("Number of iterations = %d\n", numItr);
	printf ("Accelerator Elapsed time = %lf sec\n", done_time - strt_time);
	printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", done_time - strt_time);
	
	return 0;
}
