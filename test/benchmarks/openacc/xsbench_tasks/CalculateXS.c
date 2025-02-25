#include "XSbench_header.h"


void run_simulation(int lookups, long n_isotopes, long n_gridpoints, int size_mats, \
	int *num_nucs, double *concs, int *mats, int *mats_ptr, double *energy_grid, \
	int *grid_ptrs, double *nuclide_grids, unsigned long long *verification, \
	unsigned int offset, unsigned int numElements, int asyncID) {
	int i;
#ifdef MERGE_TASKS
    HI_enter_subregion("xsbench", 0);
#endif
    #pragma acc update device(num_nucs[0:n_isotopes], concs[0:size_mats], \
		 mats[0:size_mats], mats_ptr[0:12], energy_grid[0:n_isotopes*n_gridpoints], \
		grid_ptrs[0:n_isotopes*n_isotopes*n_gridpoints], \
		nuclide_grids[0:n_isotopes*n_gridpoints*6])
	// XS Lookup Loop
	#pragma acc parallel loop independent gang worker \
	copyin(n_isotopes, n_gridpoints) \
	present(num_nucs[0:n_isotopes], concs[0:size_mats], mats[0:size_mats], \
			mats_ptr[0:12], energy_grid[0:n_isotopes*n_gridpoints], \
			grid_ptrs[0:n_isotopes*n_isotopes*n_gridpoints], \
			nuclide_grids[0:n_isotopes*n_gridpoints*6], verification[0:numElements])
	for(i=0; i<numElements; i++)
	{
		double maxV = -1.0;
		int max_idx = 0;
		double p_energy;
		int mat;
		double macro_xs_vector[5];
		// Set the initial seed value
		unsigned long seed = STARTING_SEED;
		#ifndef OPENACC
		// Status text
		if( INFO && mype == 0 && thread == 0 && (i+offset) % 1000 == 0 )
			printf("\rCalculating XS's... (%.0lf%% completed)",
				((i+offset) / ( (double)lookups / (double)nthreads ))
				/ (double)nthreads * 100.0);
		#endif

		// Forward seed to lookup index (we need 2 samples per lookup)
		seed = fast_forward_LCG(seed, 2*(i+offset));

		// Randomly pick an energy and material for the particle
		p_energy = LCG_random_double(&seed);
		mat         = pick_mat_new(&seed);
		
		// This returns the macro_xs_vector, but we're not going
		// to do anything with it in this program, so return value
		// is written over.
		calculate_macro_xs(p_energy, mat, n_isotopes, n_gridpoints,
			num_nucs, concs, energy_grid, nuclide_grids,
			grid_ptrs, mats, mats_ptr, macro_xs_vector);

		// Verification hash calculation
		for(int j = 0; j < 5; j++ )
		{   
			if( macro_xs_vector[j] > maxV )
			{   
				maxV = macro_xs_vector[j];
				max_idx = j;
			}   
		}   
		verification[i] = max_idx+1;
	}
    #pragma acc update host(verification[0:numElements]) async(asyncID)
#ifdef MERGE_TASKS
    HI_exit_subregion("xsbench", 0);
#endif
}

// Calculates the microscopic cross section for a given nuclide & energy
void calculate_micro_xs(double p_energy, int nuc, long n_isotopes, long n_gridpoints,
                        double * restrict energy_grid, double * restrict nuclide_grids,
                        int * restrict grid_ptrs, int idx, double * restrict xs_vector)
{	
	// Variables
	double f;
	double *low, *high;

	// pull ptr from energy grid and check to ensure that
	// we're not reading off the end of the nuclide's grid
	if(grid_ptrs[n_isotopes*idx + nuc] == n_gridpoints - 1){
		low = &nuclide_grids[nuc*n_gridpoints*6 + (grid_ptrs[n_isotopes*idx + nuc] - 1)*6];
		high = &nuclide_grids[nuc*n_gridpoints*6 + (grid_ptrs[n_isotopes*idx + nuc])*6];
	}
	else{
		low = &nuclide_grids[nuc*n_gridpoints*6 + (grid_ptrs[n_isotopes*idx + nuc])*6];
		high = &nuclide_grids[nuc*n_gridpoints*6 + (grid_ptrs[n_isotopes*idx + nuc] + 1)*6];
	}	

	// calculate the re-useable interpolation factor
	f = (high[0] - p_energy) / (high[0] - low[0]);

	// Total XS
	xs_vector[0] = high[1] - f * (high[1] - low[1]);
	
	// Elastic XS
	xs_vector[1] = high[2] - f * (high[2] - low[2]);
	
	// Absorbtion XS
	xs_vector[2] = high[3] - f * (high[3] - low[3]);
	
	// Fission XS
	xs_vector[3] = high[4] - f * (high[4] - low[4]);
	
	// Nu Fission XS
	xs_vector[4] = high[5] - f * (high[5] - low[5]);
	
	//test
	/*	
	if( omp_get_thread_num() == 0 )
	{
		printf("Lookup: Energy = %lf, nuc = %d\n", p_energy, nuc);
		printf("e_h = %lf e_l = %lf\n", high->energy , low->energy);
		printf("xs_h = %lf xs_l = %lf\n", high->elastic_xs, low->elastic_xs);
		printf("total_xs = %lf\n\n", xs_vector[1]);
	}
	*/
	
}

// Calculates macroscopic cross section based on a given material & energy 
void calculate_macro_xs(double p_energy, int mat, long n_isotopes, long n_gridpoints,
			int * restrict num_nucs, double * restrict concs,
			double * restrict energy_grid, double * restrict nuclide_grids,
			int * restrict grid_ptrs, int * restrict mats, int * restrict mats_ptr,
			double * restrict macro_xs_vector)
{
	double xs_vector[5];
	int p_nuc; // the nuclide we are looking up
	long idx = 0;	
	double conc; // the concentration of the nuclide in the material
	int j, k;

	// cleans out macro_xs_vector
	for(k=0; k<5; k++)
		macro_xs_vector[k] = 0;

	// binary search for energy on unionized energy grid (UEG)
	idx = grid_search(n_isotopes*n_gridpoints, p_energy, energy_grid);	
	
	// Once we find the pointer array on the UEG, we can pull the data
	// from the respective nuclide grids, as well as the nuclide
	// concentration data for the material
	// Each nuclide from the material needs to have its micro-XS array
	// looked up & interpolatied (via calculate_micro_xs). Then, the
	// micro XS is multiplied by the concentration of that nuclide
	// in the material, and added to the total macro XS array.
	for(j=0; j<num_nucs[mat]; j++){
		p_nuc = mats[mats_ptr[mat] + j];
		conc = concs[mats_ptr[mat] + j];
		calculate_micro_xs(p_energy, p_nuc, n_isotopes, n_gridpoints,
				   energy_grid, nuclide_grids, grid_ptrs, idx, xs_vector);
		for(k=0; k<5; k++)
			macro_xs_vector[k] += xs_vector[k] * conc;
	}
	
	//test
	/*
	for( int k = 0; k < 5; k++ )
		printf("Energy: %lf, Material: %d, XSVector[%d]: %lf\n",
		       p_energy, mat, k, macro_xs_vector[k]);
	*/
}


// (fixed) binary search for energy on unionized energy grid
// returns lower index
long grid_search(long n, double quarry, double * A)
{
	long lowerLimit = 0;
	long upperLimit = n-1;
	long examinationPoint;
	long length = upperLimit - lowerLimit;

	while(length > 1){
		examinationPoint = lowerLimit + (length/2);	
		if(A[examinationPoint] > quarry) upperLimit = examinationPoint;
		else lowerLimit = examinationPoint;
		length = upperLimit - lowerLimit;
	}
	return lowerLimit;
}
