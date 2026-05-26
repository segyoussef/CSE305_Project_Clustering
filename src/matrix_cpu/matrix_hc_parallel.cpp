// This file implements the parallel matrix-based hierarchical clustering using std::thread.
#include "matrix_hc_parallel.h"
#include "../common/distance.h"
#include <thread>
#include <limits>
#include <vector>

// Each thread finds the minimum distance in its chunk of the matrix
static void find_min_chunk(
    const std::vector<std::vector<double>>& dist,
    const std::vector<bool>& active,
    int n, int thread_id, int num_threads,
    double& local_min, int& local_a, int& local_b)
{
    local_min = std::numeric_limits<double>::infinity();
    local_a = -1;
    local_b = -1;

    for (int i = thread_id; i < n; i += num_threads) {
        if (!active[i]) continue;
        for (int j = i + 1; j < n; j++) {
            if (!active[j]) continue;
            if (dist[i][j] < local_min) {
                local_min = dist[i][j];
                local_a = i;
                local_b = j;
            }
        }
    }
}

// Each thread updates a chunk of the row after a merge
static void update_chunk(
    std::vector<std::vector<double>>& dist,
    const std::vector<bool>& active,
    int n, int a, int b, int size_a, int size_b,
    Linkage linkage, int thread_id, int num_threads)
{
    for (int k = thread_id; k < n; k += num_threads) {
        if (!active[k] || k == a || k == b) continue;
        dist[a][k] = lance_williams(dist[a][k], dist[b][k], size_a, size_b, linkage);
        dist[k][a] = dist[a][k];
    }
}

Dendrogram matrix_hc_parallel(const std::vector<Point>& points, Linkage linkage, int num_threads) {
    int n = points.size();

    // Build initial distance matrix
    std::vector<std::vector<double>> dist(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            dist[i][j] = euclidean(points[i], points[j]);

    std::vector<int> size(n, 1);
    std::vector<bool> active(n, true);
    Dendrogram result;

    std::vector<double> local_mins(num_threads);
    std::vector<int> local_as(num_threads), local_bs(num_threads);

    for (int step = 0; step < n - 1; step++) {

        // Phase 1: parallel min search
        std::vector<std::thread> threads(num_threads);
        for (int t = 0; t < num_threads; t++)
            threads[t] = std::thread(find_min_chunk,
                std::cref(dist), std::cref(active), n, t, num_threads,
                std::ref(local_mins[t]), std::ref(local_as[t]), std::ref(local_bs[t]));
        for (auto& th : threads) th.join();

        // Combine local minimums
        double min_dist = std::numeric_limits<double>::infinity();
        int a = -1, b = -1;
        for (int t = 0; t < num_threads; t++) {
            if (local_mins[t] < min_dist) {
                min_dist = local_mins[t];
                a = local_as[t];
                b = local_bs[t];
            }
        }

        // Record merge
        result.push_back({a, b, min_dist, size[a] + size[b]});

        // Phase 2: parallel matrix update
        active[b] = false;
        for (int t = 0; t < num_threads; t++)
            threads[t] = std::thread(update_chunk,
                std::ref(dist), std::cref(active), n, a, b,
                size[a], size[b], linkage, t, num_threads);
        for (auto& th : threads) th.join();

        size[a] += size[b];
    }

    return result;
}