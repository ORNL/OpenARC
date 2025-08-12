#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

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

int parse_inputs(int argc, char *argv[], unsigned int *chunkSize, unsigned int *lastChunkSize, 
	unsigned int *numTasks, unsigned int *nDevices, unsigned int *asyncMode) { 
	int i;
	int error = 0;
	if( argc > 1 ) {
		i = 1;
		while( i<argc ) {
			int ok;
			if(strcmp(argv[i], "-d") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -d");
				}
				ok = StrToInt(argv[i+1], nDevices);
				if(!ok) {
					printf("Parse Error on option -d integer value required after argument\n");
					error++;
				}
				i+=2;
			} else if(strcmp(argv[i], "-c") == 0) {
				if (i+1 >= argc) {
					printf("Missing integer argument to -c");
				}
				ok = StrToInt(argv[i+1], chunkSize);
				if(!ok) {
					printf("Parse Error on option -c integer value required after argument\n");
					error++;
				}
				i+=2;
            } else if(strcmp(argv[i], "-am") == 0) {
                if (i+1 >= argc) {
                    printf("Missing integer argument to -am");
					error++;
                }
                ok = StrToInt(argv[i+1], asyncMode);
                if(!ok) {
                    printf("Parse Error on option -am integer value required after argument\n");
					error++;
                }
                i+=2;
			} else if(strcmp(argv[i], "-h") == 0) {
				printf("==> Available commandline options:\n");
				printf("    -h    #print help message\n");
				printf("    -d D  #set the value of nDevices with D\n");
				printf("    -c C  #set the value of chunkSize with C\n");
				printf("    -am AM  #set the value of asyncMode with AM\n");
				exit(0);
			} else {
				printf("Invalid commandline option: %s\n", argv[i]);
				error++;
			}
		}
	}
	return error;
}
