#include "matrix_hc_cuda.h"
#include "kernels.h"
#include <cuda_runtime.h>
#include <cstdint>
#include <limits>
#include <vector>

Dendrogram matrix_hc_cuda(const std::vector<Point>& points, Linkage linkage, int block_size) {
    int n   = (int)points.size();
    int dim = (n > 0) ? (int)points[0].coords.size() : 0;

    std::vector<double> h_coords(n * dim);
    for (int i = 0; i < n; i++)
        for (int d = 0; d < dim; d++)
            h_coords[i * dim + d] = points[i].coords[d];

    double   *d_coords, *d_dist;
    uint8_t  *d_active;
    int      *d_size;
    cudaMalloc(&d_coords, (long long)n * dim * sizeof(double));
    cudaMalloc(&d_dist,   (long long)n * n   * sizeof(double));
    cudaMalloc(&d_active, n * sizeof(uint8_t));
    cudaMalloc(&d_size,   n * sizeof(int));

    cudaMemcpy(d_coords, h_coords.data(), (long long)n * dim * sizeof(double),  cudaMemcpyHostToDevice);

    std::vector<uint8_t> h_active(n, 1);
    std::vector<int>     h_size(n, 1);
    cudaMemcpy(d_active, h_active.data(), n * sizeof(uint8_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_size,   h_size.data(),   n * sizeof(int),     cudaMemcpyHostToDevice);

    launch_compute_distances(d_coords, d_dist, n, dim);

    int num_blocks = compute_find_min_blocks(n, block_size);
    MinResult *d_partial;
    cudaMalloc(&d_partial, num_blocks * sizeof(MinResult));
    std::vector<MinResult> h_partial(num_blocks);

    int linkage_int = (linkage == Linkage::SINGLE)   ? 0 : (linkage == Linkage::COMPLETE)  ? 1 : 2;

    Dendrogram result;
    result.reserve(n - 1);

    for (int step = 0; step < n - 1; step++) {

        launch_find_min(d_dist, d_active, n, d_partial, num_blocks, block_size);
        cudaMemcpy(h_partial.data(), d_partial, num_blocks * sizeof(MinResult), cudaMemcpyDeviceToHost);

        MinResult global;
        global.val = std::numeric_limits<double>::infinity();
        global.row = -1; global.col = -1;
        for (int i = 0; i < num_blocks; i++) {
            if (h_partial[i].val < global.val)
                global = h_partial[i];
        }

        int a = global.row, b = global.col;

        result.push_back({a, b, global.val, h_size[a] + h_size[b]});

        launch_lance_williams_update(d_dist, d_active, d_size, n, a, b, linkage_int);

        h_size[a]   += h_size[b];
        h_active[b]  = 0;

        cudaMemcpy(d_size   + a, &h_size[a],   sizeof(int),     cudaMemcpyHostToDevice);
        cudaMemcpy(d_active + b, &h_active[b],  sizeof(uint8_t), cudaMemcpyHostToDevice);
    }

    cudaFree(d_coords); cudaFree(d_dist);
    cudaFree(d_active); cudaFree(d_size); cudaFree(d_partial);

    return result;
}
