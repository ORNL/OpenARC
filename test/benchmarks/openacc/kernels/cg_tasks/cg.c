#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "arcutil.h"
#include "openacc.h"

#if !defined(DEBUG_PRINT)
#define DEBUG_PRINT 1
#endif

#define MAX_LINE 1024

unsigned int nDevices = 1;
unsigned int asyncMode = 2;

// Read Matrix Market file (COO format)
void read_matrix_market(const char *filename, int *rows, int *cols, int *nnz,
                        int **row_idx, int **col_idx, double **values) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE];
    // Skip headers and comments
    do {
        fgets(line, MAX_LINE, f);
    } while (line[0] == '%');

    // Read matrix dimensions
    sscanf(line, "%d %d %d", rows, cols, nnz);

    *row_idx = (int *)malloc((*nnz) * sizeof(int));
    *col_idx = (int *)malloc((*nnz) * sizeof(int));
    *values  = (double *)malloc((*nnz) * sizeof(double));

    for (int i = 0; i < *nnz; i++) {
        int r, c;
        double v;
        fscanf(f, "%d %d %lf", &r, &c, &v);
        (*row_idx)[i] = r - 1; // Convert to 0-based indexing
        (*col_idx)[i] = c - 1;
        (*values)[i] = v;
    }

    fclose(f);
}


// Convert COO to CSR format
void coo_to_csr(int rows, int nnz, int *row_coo, int *col_coo, double *val_coo,
                int **row_ptr, int **col_idx, double **values) {
    *row_ptr = (int *)calloc(rows + 1, sizeof(int));
    *col_idx = (int *)malloc(nnz * sizeof(int));
    *values  = (double *)malloc(nnz * sizeof(double));

    // Count entries per row
    for (int i = 0; i < nnz; i++)
        (*row_ptr)[row_coo[i] + 1]++;

    // Prefix sum to get row_ptr
    for (int i = 0; i < rows; i++)
        (*row_ptr)[i + 1] += (*row_ptr)[i];

    // Fill col_idx and values
    int *temp = (int *)malloc((rows + 1) * sizeof(int));
    memcpy(temp, *row_ptr, (rows + 1) * sizeof(int));

    for (int i = 0; i < nnz; i++) {
        int row = row_coo[i];
        int dest = temp[row]++;
        (*col_idx)[dest] = col_coo[i];
        (*values)[dest]  = val_coo[i];
    }

    free(temp);
}


// Dot product: return x^T * y
double dot(int n, double *x, double *y) {
    double result = 0.0;
#pragma acc parallel loop present(x, y) reduction(+:result)
    for (int i = 0; i < n; i++)
        result += x[i] * y[i];
    return result;
}

// y = x (copy)
void vec_copy(int n, double *x, double *y, int asyncID) {
#pragma acc parallel loop present(x, y) async(asyncID)
    for (int i = 0; i < n; i++)
        y[i] = x[i];
}

void vec_sub(int n, double *b, double *r, int asyncID) {
#pragma acc parallel loop present(r, b) async(asyncID)
	for (int i = 0; i < n; i++)
		r[i] = b[i] - r[i];
}

void vec_sum(int n, double beta, double *r, double *p, int asyncID) {
#pragma acc parallel loop present(p, r) async(asyncID)
			for (int i = 0; i < n; i++)
				p[i] = r[i] + beta * p[i];
}

// y = y + alpha * x
void axpy(int n, double alpha, double *x, double *y, int asyncID) {
#pragma acc parallel loop present(x, y) async(asyncID)
    for (int i = 0; i < n; i++)
        y[i] += alpha * x[i];
}

// SpMV: y = A * x
void spmv(int rows, int *row_ptr, int *col_idx, double *values,
          double *x_d, double *y, unsigned int start_index, int asyncID) {
#pragma acc parallel loop present(row_ptr, col_idx, values, y) deviceptr(x_d) async(asyncID)
    for (int i = 0; i < rows; i++) {
        y[i] = 0.0;
        for (int j = row_ptr[i + start_index]; j < row_ptr[i + 1 + start_index]; j++) {
            y[i] += values[j] * x_d[col_idx[j]];
        }
    }
}

// Macros to compute 1D index from 3D coordinates
#define INDEX(i,j,k,Nx,Ny) ((i) + (j)*(Nx) + (k)*(Nx)*(Ny))

// Generate CSR matrix for 3D heat equation
void generate_3d_heat_csr(int Nx, int Ny, int Nz,
                          int **row_ptr, int **col_idx, double **val) {
    int N = Nx * Ny * Nz;
    int max_nnz = N * 7;  // upper bound

    *row_ptr = (int *)malloc((N + 1) * sizeof(int));
    *col_idx = (int *)malloc(max_nnz * sizeof(int));
    *val     = (double *)malloc(max_nnz * sizeof(double));

    int nnz = 0;
    (*row_ptr)[0] = 0;

    for (int k = 0; k < Nz; k++) {
        for (int j = 0; j < Ny; j++) {
            for (int i = 0; i < Nx; i++) {
                int row = INDEX(i, j, k, Nx, Ny);

                // Center point
                (*col_idx)[nnz] = row;
                (*val)[nnz++] = 6.0;

                // -x neighbor
                if (i > 0) {
                    (*col_idx)[nnz] = INDEX(i - 1, j, k, Nx, Ny);
                    (*val)[nnz++] = -1.0;
                }
                // +x neighbor
                if (i < Nx - 1) {
                    (*col_idx)[nnz] = INDEX(i + 1, j, k, Nx, Ny);
                    (*val)[nnz++] = -1.0;
                }
                // -y neighbor
                if (j > 0) {
                    (*col_idx)[nnz] = INDEX(i, j - 1, k, Nx, Ny);
                    (*val)[nnz++] = -1.0;
                }
                // +y neighbor
                if (j < Ny - 1) {
                    (*col_idx)[nnz] = INDEX(i, j + 1, k, Nx, Ny);
                    (*val)[nnz++] = -1.0;
                }
                // -z neighbor
                if (k > 0) {
                    (*col_idx)[nnz] = INDEX(i, j, k - 1, Nx, Ny);
                    (*val)[nnz++] = -1.0;
                }
                // +z neighbor
                if (k < Nz - 1) {
                    (*col_idx)[nnz] = INDEX(i, j, k + 1, Nx, Ny);
                    (*val)[nnz++] = -1.0;
                }

                (*row_ptr)[row + 1] = nnz;
            }
        }
    }
}

// Conjugate Gradient solver
// Distributed data: b, r, Ap
// Whole data: row_ptr, col_idx, val
// Data having both whole and distribute copies: x, p
int conjugate_gradient(int n, int *row_ptr, int *col_idx, double *val,
                       double *b, double *x, double *x_d, int *nits, double tol, int max_iter,
						unsigned int numTasks, unsigned int chunkSize, unsigned int lastChunkSize) {
	int iter, i;
	int asyncID;
	unsigned int numElements;
    // Allocate temporal storage
    double *r = (double *)malloc(n * sizeof(double));
    double *p = (double *)malloc(n * sizeof(double));
    double *p_h = (double *)malloc(n * sizeof(double));
    double *Ap = (double *)malloc(n * sizeof(double));
	for (i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) { 
			numElements = lastChunkSize;
		} else {
			numElements = chunkSize;
		}   
		acc_create(r + i*chunkSize, numElements*sizeof(double));
		acc_create(p + i*chunkSize, numElements*sizeof(double));
		acc_create(Ap + i*chunkSize, numElements*sizeof(double));
	}
	double* p_d = (double *)acc_malloc(n * sizeof(double));

#pragma acc data present(row_ptr, col_idx, val)
	{
		double rsold = 0.0;
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
			// r = b - A*x
			spmv(numElements, row_ptr, col_idx, val, x_d, r + i*chunkSize, i*chunkSize, asyncID);
			vec_sub(numElements, b + i*chunkSize, r + i*chunkSize, asyncID);
			vec_copy(numElements, r + i*chunkSize, p + i*chunkSize, asyncID);
		}
		if( asyncMode > 0 ) {
			#pragma acc wait
		}
        for( i = 0; i < numTasks; i++ ) { 
            if( i == numTasks-1 ) { 
                numElements = lastChunkSize;
            } else {
                numElements = chunkSize;
            }   
			rsold += dot(numElements, r + i*chunkSize, r + i*chunkSize);
		}

		double rsnew;

		for ( iter = 0; iter < max_iter; iter++) {
			//Update p to p_d
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
				acc_update_self_async(p + i*chunkSize, numElements * sizeof(double), asyncID);
			}
			if( asyncMode == 0 ) {
				acc_memcpy_to_device(p_d, p, n * sizeof(double));
			} else {
				#pragma acc wait
				//[DEBUG] acc_memcpy_to_device(p_d, p, ...) fails 
				//for the CUDA backend due to partial memory pinning on p.
    			memcpy(p_h, p, n * sizeof(int));
				acc_memcpy_to_device(p_d, p_h, n * sizeof(double));
			}
			rsnew = 0.0;
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
				spmv(numElements, row_ptr, col_idx, val, p_d, Ap + i*chunkSize, i*chunkSize, asyncID);
			}
			if( asyncMode > 0 ) {
				#pragma acc wait
			}
			for( i = 0; i < numTasks; i++ ) { 
				if( i == numTasks-1 ) { 
					numElements = lastChunkSize;
				} else {
					numElements = chunkSize;
				}   
				rsnew += dot(numElements, p + i*chunkSize, Ap + i*chunkSize);
			}

			double alpha = rsold / rsnew;
			rsnew = 0.0;
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
				axpy(numElements, alpha, p + i*chunkSize, x + i*chunkSize, asyncID);         // x = x + alpha*p
				axpy(numElements, -alpha, Ap + i*chunkSize, r + i*chunkSize, asyncID);    // r = r - alpha*Ap
			}
			if( asyncMode > 0 ) {
				#pragma acc wait
			}
			for( i = 0; i < numTasks; i++ ) { 
				if( i == numTasks-1 ) { 
					numElements = lastChunkSize;
				} else {
					numElements = chunkSize;
				}   
				rsnew += dot(numElements, r + i*chunkSize, r + i*chunkSize);
			}

			if (sqrt(rsnew) < tol) {
				printf("==> Converged in %d iterations.\n", iter + 1);
				break;
			}

			double beta = rsnew / rsold;
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
				vec_sum(numElements, beta, r + i*chunkSize, p + i*chunkSize, asyncID);
			}
			rsold = rsnew;
		}
	}

    *nits = iter;

    // Free temporal storage
	for( i = 0; i < numTasks; i++ ) {
		if( i==numTasks-1 ) {
			numElements = lastChunkSize;
		} else {
			numElements = chunkSize;
		}
		acc_delete(r + i*chunkSize, numElements*sizeof(double));
		acc_delete(p + i*chunkSize, numElements*sizeof(double));
		acc_delete(Ap + i*chunkSize, numElements*sizeof(double));
	}
	acc_free(p_d);

    free(r);
    free(p);
    free(Ap);
    return 0;
}

int main(int argc, char *argv[]) {
    unsigned int chunkSize = 0;
    unsigned int lastChunkSize = 0;
    unsigned int numTasks = 0;
    unsigned int numElements = 0;
	unsigned int inputSize = 8;
    int asyncID;
	int error;
    int Nx, Ny, Nz;
    int *row_ptr, *col_idx;
    int N;
    int nits;
    double *val;
	double strt_time, end_time;
    int max_nnz;  // upper bound
	int i;

	error = parse_inputs(argc, argv, &chunkSize, &lastChunkSize, &numTasks, &nDevices, &asyncMode, &inputSize);
	if( error > 0 ) { exit(1); }
	else {
		Nx = inputSize;
		Ny = inputSize;
		Nz = inputSize;
    	N = Nx * Ny * Nz;
    	max_nnz = N * 7;
	}

	if( chunkSize == 0 ) {
		chunkSize = N/nDevices;
	}
	numTasks = N/chunkSize;
	if( N%chunkSize != 0 ) {
		lastChunkSize = chunkSize + (N - (numTasks)*chunkSize);
	} else {
		lastChunkSize = chunkSize;
	}

    generate_3d_heat_csr(Nx, Ny, Nz, &row_ptr, &col_idx, &val);


    // Set up vectors
    double *x = (double *)malloc(N * sizeof(double));
    double *b = (double *)malloc(N * sizeof(double));

    // Exact Solution  x = [1.1, 1.1, ..., 1.1]
    for (i = 0; i < N; i++) x[i] = 1.1;

	for (i = 0; i < numTasks; i++) {
		if( i==numTasks-1 ) { 
			numElements = lastChunkSize;
		} else {
			numElements = chunkSize;
		}   
		acc_create(x + i*chunkSize, numElements*sizeof(double));
		acc_create(b + i*chunkSize, numElements*sizeof(double));
	}
	double* x_d = (double *)acc_malloc(N * sizeof(double));

#pragma acc data copyin(row_ptr[0:N+1], col_idx[0:max_nnz], val[0:max_nnz])
	{
		acc_memcpy_to_device(x_d, x, N * sizeof(double));
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
			// set up the right hand side vector b using Ax=b
			spmv(numElements, row_ptr, col_idx, val, x_d, b + i*chunkSize, i*chunkSize, asyncID);
		}

		// Initial guess of x = [0.1, 0.1, ..., 0.1]
		for (int i = 0; i < N; i++) x[i] = 0.1;
		acc_memcpy_to_device(x_d, x, N * sizeof(double));

		strt_time = my_timer ();
		conjugate_gradient(N, row_ptr, col_idx, val, b, x, x_d, &nits,  1e-8, 1000, numTasks, chunkSize, lastChunkSize);
		end_time = my_timer ();
		printf("chunkSize:%u, nDevices:%u, asyncMode:%u\n", chunkSize, nDevices, asyncMode);
		printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", end_time - strt_time);
	}
    
#if DEBUG_PRINT == 1
    // Print result vector y
    printf("==> Number of iteration is %d\n",nits);
    printf("==> Result y = A * x:\n");
    for (i = 0; i < N; i++) {
        printf("%f\n", x[i]);
    }
#endif

    for( i = 0; i < numTasks; i++ ) {
        if( i==numTasks-1 ) {
            numElements = lastChunkSize;
        } else {
            numElements = chunkSize;
        }
        acc_delete(x + i*chunkSize, numElements*sizeof(double));
        acc_delete(b + i*chunkSize, numElements*sizeof(double));
    }
	acc_free(x_d);
    free(row_ptr); free(col_idx); free(val);
    free(x); free(b);
    return 0;
}
