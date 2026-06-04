#include "kernels.h"
#include <cuda_runtime.h>
#include <cfloat>
#include <cmath>

// ker1 compute all the N×N pairwise euclidean distance 
//2-D grid of 16×16 blocks , thread (j,i) writes dist[i*n+j].
// and d_coords: row-major [n][dim] array of all point coordinates.

__global__ static void compute_distances_kernel(const double* __restrict__ d_coords,double* __restrict__ dist, int n, int dim){
    int i = blockIdx.y * blockDim.y + threadIdx.y;
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n && j < n) {
        double sum = 0.0;
        const double* pi = d_coords + i * dim;
        const double* pj = d_coords + j * dim;
        for (int d = 0; d < dim; d++) {
            double diff = pi[d] - pj[d];
            sum += diff * diff;
        }
        dist[i * n + j] = sqrt(sum);
    }
}


// ker2 find the minimum distance among all active pairs (i < j)
//1-D grid with a grid-stride loop + shared-memory tree reduction.
//each block write one MinResult to partial[blockIdx.x]
__global__ static void find_min_kernel(const double* __restrict__ dist,const uint8_t* __restrict__ active, int n, MinResult* __restrict__ partial){
    extern __shared__ MinResult smem[];
    int tid = threadIdx.x;
    long long total = (long long)n * n;

    MinResult local;
    local.val = DBL_MAX;
    local.row = -1;
    local.col = -1;

    for (long long idx = (long long)blockIdx.x * blockDim.x + tid;
         idx < total;
         idx += (long long)gridDim.x * blockDim.x)
    {
        int i = (int)(idx / n);
        int j = (int)(idx % n);
        if (i < j && active[i] && active[j]) {
            double d = dist[i * n + j];
            if (d < local.val) {
                local.val = d;
                local.row  = i;
                local.col  = j;
            }
        }
    }

    smem[tid] = local;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            if (smem[tid + s].val < smem[tid].val)
                smem[tid] = smem[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0)
        partial[blockIdx.x] = smem[0];
}

// then ker3 is Lance-Williams distance gets like update after merging clusters a and b
// thread k updates dist[a][k] and dist[k][a] for every active k ≠ a, b.
// linkage_int: 0 = single, 1 = complete, 2 = average.

__global__ static void lance_williams_update_kernel(double* __restrict__ dist,const uint8_t* __restrict__ active,const int* __restrict__ size_d,int n, int a, int b, int linkage_int){
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= n) return;
    if (!active[k] || k == a || k == b) return;

    double d_ak = dist[(long long)a * n + k];
    double d_bk = dist[(long long)b * n + k];
    int sa = size_d[a];
    int sb = size_d[b];

    double nd;
    if (linkage_int == 0)
        nd = fmin(d_ak, d_bk);
    else if (linkage_int == 1)
        nd = fmax(d_ak, d_bk);
    else
        nd = (sa * d_ak + sb * d_bk) / (sa + sb);

    dist[(long long)a * n + k] = nd;
    dist[(long long)k * n + a] = nd;
}

// then host-side helpers


int compute_find_min_blocks(int n, int block_size) {
    long long total = (long long)n * n;
    int blocks = (int)((total + block_size - 1) / block_size);
    return blocks < 1024 ? blocks : 1024;
}

void launch_compute_distances(const double* d_coords, double* d_dist, int n, int dim){
    dim3 block(16, 16);
    dim3 grid((n + 15) / 16, (n + 15) / 16);
    compute_distances_kernel<<<grid, block>>>(d_coords, d_dist, n, dim);
}

void launch_find_min(const double* d_dist, const uint8_t* d_active, int n, MinResult* d_partial, int num_blocks, int block_size){
    size_t smem_bytes = block_size * sizeof(MinResult);
    find_min_kernel<<<num_blocks, block_size, smem_bytes>>>(d_dist, d_active, n, d_partial);
}

void launch_lance_williams_update(double* d_dist, const uint8_t* d_active,const int* d_size, int n, int a, int b, int linkage_int){
    int threads = 256;
    int blocks  = (n + threads - 1) / threads;
    lance_williams_update_kernel<<<blocks, threads>>>(
        d_dist, d_active, d_size, n, a, b, linkage_int);
}
