// This file implements the parallel matrix-based hierarchical clustering using persistent threads.
// Threads are created once and reused across all merge steps via condition variables, avoiding the overhead of repeated thread creation and destruction.

#include "matrix_hc_parallel.h"
#include "../common/distance.h"
#include <limits>

// Shared state between main thread and worker threads
struct ThreadPool {
    // Input
    const std::vector<std::vector<double>>* dist;
    const std::vector<bool>* active;
    int n;
    int num_threads;
    int a, b, size_a, size_b;
    Linkage linkage;

    // Output 
    std::vector<double> local_mins;
    std::vector<int> local_as, local_bs;

    // Synchronization
    std::mutex mtx;
    std::condition_variable cv;
    int phase;      
    int ready_count; 

    ThreadPool(int T) :
        local_mins(T), local_as(T), local_bs(T),
        phase(0), ready_count(0) {}
};

static void worker(ThreadPool& pool, int thread_id) {
    while (true) {
        int current_phase;
        {
            std::unique_lock<std::mutex> lock(pool.mtx);
            pool.cv.wait(lock, [&]{ return pool.phase != 0; });
            current_phase = pool.phase;
        }

        if (current_phase == 3) return;

        int n = pool.n;
        int T = pool.num_threads;

        if (current_phase == 1) {
            // Phase 1: we find local minimum
            double local_min = std::numeric_limits<double>::infinity();
            int local_a = -1, local_b = -1;
            for (int i = thread_id; i < n; i += T) {
                if (!(*pool.active)[i]) continue;
                for (int j = i + 1; j < n; j++) {
                    if (!(*pool.active)[j]) continue;
                    if ((*pool.dist)[i][j] < local_min) {
                        local_min = (*pool.dist)[i][j];
                        local_a = i;
                        local_b = j;
                    }
                }
            }
            pool.local_mins[thread_id] = local_min;
            pool.local_as[thread_id]   = local_a;
            pool.local_bs[thread_id]   = local_b;

        } else if (current_phase == 2) {
            // Phase 2: we update matrix row/column
            auto& dist = const_cast<std::vector<std::vector<double>>&>(*pool.dist);
            for (int k = thread_id; k < n; k += T) {
                if (!(*pool.active)[k] || k == pool.a || k == pool.b) continue;
                dist[pool.a][k] = lance_williams(dist[pool.a][k], dist[pool.b][k],
                                                  pool.size_a, pool.size_b, pool.linkage);
                dist[k][pool.a] = dist[pool.a][k];
            }
        }

        // Signal main thread that this worker is done
        {
            std::lock_guard<std::mutex> lock(pool.mtx);
            pool.ready_count++;
            if (pool.ready_count == pool.num_threads) {
                pool.phase = 0;
                pool.cv.notify_all();
            }
        }
    }
}

Dendrogram matrix_hc_parallel(const std::vector<Point>& points, Linkage linkage, int num_threads) {
    int n = points.size();

    std::vector<std::vector<double>> dist(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            dist[i][j] = euclidean(points[i], points[j]);

    std::vector<int> size(n, 1);
    std::vector<bool> active(n, true);
    Dendrogram result;

    ThreadPool pool(num_threads);
    pool.dist   = &dist;
    pool.active = &active;
    pool.n      = n;
    pool.num_threads = num_threads;
    pool.linkage = linkage;

    std::vector<std::thread> threads(num_threads);
    for (int t = 0; t < num_threads; t++)
        threads[t] = std::thread(worker, std::ref(pool), t);

    for (int step = 0; step < n - 1; step++) {

        // Signal phase 1: min search
        {
            std::lock_guard<std::mutex> lock(pool.mtx);
            pool.ready_count = 0;
            pool.phase = 1;
            pool.cv.notify_all();
        }
        // We wait for all workers to finish phase 1
        {
            std::unique_lock<std::mutex> lock(pool.mtx);
            pool.cv.wait(lock, [&]{ return pool.phase == 0; });
        }

        // we combine local minimums
        double min_dist = std::numeric_limits<double>::infinity();
        int a = -1, b = -1;
        for (int t = 0; t < num_threads; t++) {
            if (pool.local_mins[t] < min_dist) {
                min_dist = pool.local_mins[t];
                a = pool.local_as[t];
                b = pool.local_bs[t];
            }
        }

        // We record merge
        result.push_back({a, b, min_dist, size[a] + size[b]});

        // We update shared state for phase 2
        active[b] = false;
        pool.a      = a;
        pool.b      = b;
        pool.size_a = size[a];
        pool.size_b = size[b];

        // Signal phase 2: matrix update
        {
            std::lock_guard<std::mutex> lock(pool.mtx);
            pool.ready_count = 0;
            pool.phase = 2;
            pool.cv.notify_all();
        }
        // We wait for all workers to finish phase 2
        {
            std::unique_lock<std::mutex> lock(pool.mtx);
            pool.cv.wait(lock, [&]{ return pool.phase == 0; });
        }

        size[a] += size[b];
    }

    // We signal threads to exit
    {
        std::lock_guard<std::mutex> lock(pool.mtx);
        pool.phase = 3;
        pool.cv.notify_all();
    }
    for (auto& th : threads) th.join();

    return result;
}
