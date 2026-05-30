#pragma once
#include <cstdint>

struct MinResult {
    double val;
    int row;
    int col;
};

// ret  nb of blocks to launch for the find_min kernel
int compute_find_min_blocks(int n);

// kernel launch wrappers her i chose to all defin thm in kernels.cu
//also like the d_coords: flat row-major array of shape [n][dim].
void launch_compute_distances(const double* d_coords, double* d_dist, int n, int dim);

void launch_find_min(const double* d_dist, const uint8_t* d_active, int n, MinResult* d_partial, int num_blocks);

void launch_lance_williams_update(double* d_dist, const uint8_t* d_active, const int* d_size, int n, int a, int b, int linkage_int);
