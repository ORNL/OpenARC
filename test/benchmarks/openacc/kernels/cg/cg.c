#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#if !defined(_Nx)
#define _Nx 8
#endif
#if !defined(_Ny)
#define _Ny 8
#endif
#if !defined(_Nz)
#define _Nz 8
#endif
#if !defined(DEBUG_PRINT)
#define DEBUG_PRINT 1
#endif

double my_timer ()
{
    struct timeval time;

    gettimeofday (&time, 0); 

    return time.tv_sec + time.tv_usec / 1000000.0;
}

#define MAX_LINE 1024

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
void vec_copy(int n, double *x, double *y) {
#pragma acc parallel loop present(x, y)
    for (int i = 0; i < n; i++)
        y[i] = x[i];
}

// y = y + alpha * x
void axpy(int n, double alpha, double *x, double *y) {
#pragma acc parallel loop present(x, y)
    for (int i = 0; i < n; i++)
        y[i] += alpha * x[i];
}

// SpMV: y = A * x
void spmv(int rows, int *row_ptr, int *col_idx, double *values,
          double *x, double *y) {
#pragma acc parallel loop present(row_ptr, col_idx, values, x, y)
    for (int i = 0; i < rows; i++) {
        y[i] = 0.0;
        for (int j = row_ptr[i]; j < row_ptr[i + 1]; j++) {
            y[i] += values[j] * x[col_idx[j]];
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
int conjugate_gradient(int n, int *row_ptr, int *col_idx, double *val,
                       double *b, double *x, int *nits, double tol, int max_iter) {
	int iter;
    // Allocate temporal storage
    double *r = (double *)malloc(n * sizeof(double));
    double *p = (double *)malloc(n * sizeof(double));
    double *Ap = (double *)malloc(n * sizeof(double));

#pragma acc data create(r[0:n], p[0:n], Ap[0:n]) present(row_ptr, col_idx, val, b, x)
	{
		// r = b - A*x
		spmv(n, row_ptr, col_idx, val, x, r);
#pragma acc parallel loop present(r, b)
		for (int i = 0; i < n; i++)
			r[i] = b[i] - r[i];

		vec_copy(n, r, p);

		double rsold = dot(n, r, r);
		double rsnew;

		for ( iter = 0; iter < max_iter; iter++) {
			spmv(n, row_ptr, col_idx, val, p, Ap);

			double alpha = rsold / dot(n, p, Ap);
			axpy(n, alpha, p, x);         // x = x + alpha*p
			axpy(n, -alpha, Ap, r);    // r = r - alpha*Ap

			rsnew = dot(n, r, r);

			if (sqrt(rsnew) < tol) {
				printf("==> Converged in %d iterations.\n", iter + 1);
				break;
			}

			double beta = rsnew / rsold;
#pragma acc parallel loop present(p, r)
			for (int i = 0; i < n; i++)
				p[i] = r[i] + beta * p[i];

			rsold = rsnew;
		}
	}

    *nits = iter;

    // Free temporal storage
    free(r);
    free(p);
    free(Ap);
    return 0;
}


int main(int argc, char *argv[]) {
    int Nx = _Nx, Ny = _Ny, Nz = _Nz;
    int *row_ptr, *col_idx;
    int N;
    int nits;
    double *val;
	double strt_time, end_time;
    int max_nnz;  // upper bound

    generate_3d_heat_csr(Nx, Ny, Nz, &row_ptr, &col_idx, &val);

    N = Nx * Ny * Nz;
    max_nnz = N * 7;

    // Set up vectors
    double *x = (double *)malloc(N * sizeof(double));
    double *b = (double *)malloc(N * sizeof(double));

    // Exact Solution  x = [1.1, 1.1, ..., 1.1]
    for (int i = 0; i < N; i++) x[i] = 1.1;

#pragma acc data copyin(row_ptr[0:N+1], col_idx[0:max_nnz], val[0:max_nnz]) copy(x[0:N]) create(b[0:N])
	{
		// set up the right hand side vector b using Ax=b
		spmv(N, row_ptr, col_idx, val, x, b);

		// Initial guess of x = [0.1, 0.1, ..., 0.1]
		for (int i = 0; i < N; i++) x[i] = 0.1;

#pragma acc update device(x[0:N])

		strt_time = my_timer ();
		conjugate_gradient(N, row_ptr, col_idx, val, b, x, &nits,  1e-8, 1000);
		end_time = my_timer ();
		printf("[OPENARC-PROFILE] Main Comp Time: %10.6f (s)\n", end_time - strt_time);
	}
    
#if DEBUG_PRINT == 1
    // Print result vector y
    printf("==> Number of iteration is %d\n",nits);
    printf("==> Result y = A * x:\n");
    for (int i = 0; i < N; i++) {
        printf("%f\n", x[i]);
    }
#endif

    free(row_ptr); free(col_idx); free(val);
    free(x); free(b);
    return 0;
}
