
/* Helper function for converting strings to ints, with error checking */
int StrToInt(const char *token, unsigned int *retVal);

double my_timer ();

int parse_inputs(int argc, char *argv[], unsigned int *chunkSize, unsigned int *lastChunkSize,
    unsigned int *numTasks, unsigned int *nDevices, unsigned int *asyncMode);
