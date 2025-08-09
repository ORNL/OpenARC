#include "XSbench_header.h"

#ifdef MPI
#include<mpi.h>
#endif

int main(int argc, char* argv[])
{
	// =====================================================================
	// Initialization & Command Line Read-In
	// =====================================================================
	int version = 13;
	int mype = 0;
	int max_procs = 1;
	int i, m, thread;
	unsigned long long vhash = 0;
	int nprocs;
	double acc_start, acc_end;

  //Inputs
	int nthreads;
	long n_isotopes;
	long n_gridpoints;
	int lookups;
	char HM[6];

	double *nuclide_grids;
	double *energy_grid;
	int *grid_ptrs;
	int *index_data;
	int size_mats, *num_nucs, *mats_ptr, *mats;
	double *concs;
	int bench_n; // benchmark loop index
	unsigned long long *verification;
	unsigned int numItr = 1;
    unsigned int chunkSize = 0;
    unsigned int lastChunkSize = 0;
    unsigned int numTasks = 0;
    unsigned int nDevices = 1;
	unsigned int asyncMode = 2;
    int asyncID;
    unsigned int numElements;
	unsigned int offset;

	#ifdef MPI
	MPI_Status stat;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &mype);
	#endif
	
	// rand() is only used in the serial initialization stages.
	// A custom RNG is used in parallel portions.
	#if (VERIFICATION == 1)
	srand(26);
	#else
	srand(time(NULL));
	#endif

	// Process CLI Fields -- store in "Inputs" structure
	read_CLI(argc, argv, &nthreads, &n_isotopes, &n_gridpoints, &lookups, HM, &nDevices, &numItr, &asyncMode);

	// Print-out of Input Summary
  if(mype == 0) print_inputs(nthreads, n_isotopes, n_gridpoints, lookups, HM, nprocs, version);

	// =====================================================================
	// Prepare Nuclide Energy Grids, Unionized Energy Grid, & Material Data
	// =====================================================================

	// Allocate & fill energy grids
	#ifndef BINARY_READ
	if(mype == 0) printf("Generating Nuclide Energy Grids...\n");
	#endif
	
	nuclide_grids = (double *) malloc(n_isotopes *n_gridpoints * 6 * sizeof(double));

	#if (VERIFICATION == 1)
	generate_grids_v(nuclide_grids,n_isotopes,n_gridpoints);	
	#else
	generate_grids(nuclide_grids,n_isotopes,n_gridpoints);	
	#endif

	// Sort grids by energy
	#ifndef BINARY_READ
	if(mype == 0) printf("Sorting Nuclide Energy Grids...\n");
	sort_nuclide_grids(nuclide_grids,n_isotopes,n_gridpoints);
	#endif

	// Prepare Unionized Energy Grid Framework
	// Double Indexing. Filling in energy_grid with pointers to the
	// nuclide_energy_grids.
	#ifndef BINARY_READ
	energy_grid = generate_energy_grid(n_isotopes,n_gridpoints, nuclide_grids);
	grid_ptrs = generate_grid_ptrs(n_isotopes,n_gridpoints, nuclide_grids, energy_grid);	
	#else
	energy_grid = malloc(n_isotopes*n_gridpoints*sizeof(double));
	grid_ptrs = (int *) malloc(n_isotopes*n_gridpoints*n_isotopes*sizeof(int));
	#endif

	#ifdef BINARY_READ
	if(mype == 0) printf("Reading data from \"XS_data.dat\" file...\n");
	binary_read(n_isotopes,n_gridpoints, nuclide_grids, energy_grid, grid_ptrs);
	#endif
	
	// Get material data
	if(mype == 0) printf("Loading Mats...\n");
	if(n_isotopes == 68) size_mats = 197;
	else size_mats = 484;
	num_nucs  = load_num_nucs(n_isotopes);
	mats_ptr  = load_mats_ptr(num_nucs);
	mats      = load_mats(num_nucs, mats_ptr, size_mats,n_isotopes);

	#if (VERIFICATION == 1)
	concs = load_concs_v(size_mats);
	#else
	concs = load_concs(size_mats);
	#endif

	#ifdef BINARY_DUMP
	if(mype == 0) printf("Dumping data to binary file...\n");
	binary_dump(n_isotopes,n_gridpoints, nuclide_grids, energy_grid, grid_ptrs);
	if(mype == 0) printf("Binary file \"XS_data.dat\" written! Exiting...\n");
	return 0;
	#endif

	// =====================================================================
	// Cross Section (XS) Parallel Lookup Simulation Begins
	// =====================================================================

	if(mype == 0)
	{
		printf("\n");
		border_print();
		center_print("SIMULATION", 79);
		border_print();
	}

	acc_start = timer();

	//initialize papi with one thread (master) here
	#ifdef PAPI
	if ( PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT){
		fprintf(stderr, "PAPI library init error!\n");
		exit(1);
	}
	#endif	

	verification = (unsigned long long *)malloc(lookups * sizeof(unsigned long long));

	#pragma acc data \
	create(num_nucs[0:n_isotopes], concs[0:size_mats], mats[0:size_mats], \
			mats_ptr[0:12], energy_grid[0:n_isotopes*n_gridpoints], \
			grid_ptrs[0:n_isotopes*n_isotopes*n_gridpoints], \
			nuclide_grids[0:n_isotopes*n_gridpoints*6])
	{
		// Initialize parallel PAPI counters
		#ifdef PAPI
		int eventset = PAPI_NULL; 
		int num_papi_events;
		counter_init(&eventset, &num_papi_events);
		#endif
		
		if( chunkSize == 0 ) {
			chunkSize = lookups/nDevices;
		}
		numTasks = lookups/chunkSize;	
		if( lookups%chunkSize != 0 ) { 
			lastChunkSize = chunkSize + (lookups - (numTasks)*chunkSize);
		} else {
			lastChunkSize = chunkSize;
		}   

		for(i=0; i<numTasks; i++) {
			if(i==numTasks-1) {
				numElements = lastChunkSize;
			} else {
				numElements = chunkSize;
			}
			acc_create(verification + i*chunkSize, numElements*sizeof(unsigned long long));
		}

		offset = 0;
		for(m=0; m<numItr; m++) {
#ifdef GRAPH
    		HI_enter_subregion("xsbench-graph", 1);
#endif
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
				run_simulation(lookups, n_isotopes, n_gridpoints, size_mats, \
				num_nucs, concs, mats, mats_ptr, energy_grid, grid_ptrs, \
				nuclide_grids, verification, offset, numElements, asyncID);
				offset += chunkSize;
			}
#ifdef GRAPH
    		HI_exit_subregion("xsbench", 1);
#endif
			#pragma acc wait
		}

		vhash = 0;
#if (VERIFICATION == 1)
		// Reduce validation hash on the host
		for( int i = 0; i < lookups; i++ ) {
			vhash += verification[i];
		}
#endif

		// Prints out thread local PAPI counters
		#ifdef PAPI
		if( mype == 0 && thread == 0 )
		{
			printf("\n");
			border_print();
			center_print("PAPI COUNTER RESULTS", 79);
			border_print();
			printf("Count          \tSmybol      \tDescription\n");
		}
		counter_stop(&eventset, num_papi_events);
		#endif
	}

	#ifndef PAPI
	if( mype == 0) printf("\nSimulation complete.\n" );
	#endif

	acc_end = timer();
	print_results(nthreads, n_isotopes, n_gridpoints, lookups, HM, mype, acc_end-acc_start, nprocs, vhash);

	for(i=0; i<numTasks; i++) {
		if(i==numTasks-1) {
			numElements = lastChunkSize;
		} else {
			numElements = chunkSize;
		}
		acc_delete(verification + i*chunkSize, numElements*sizeof(unsigned long long));
	}

	#ifdef MPI
	MPI_Finalize();
	#endif

	return 0;
}
