// This file implements the parallel matrix-based hierarchical clustering using persistent threads.
#include "matrix_hc_parallel.h"
#include "../common/distance.h"
#include <limits>
#include <atomic>

struct ThreadPool {
    const std::vector<std::vector<double>>* dist;
    const std::vector<bool>* active;
    int n;
    int num_threads;
    int a, b, size_a, size_b;
    Linkage linkage;

    std::vector<double> local_mins;
    std::vector<int> local_as, local_bs;

    std::mutex mtx;
    std::condition_variable cv_workers;
    std::condition_variable cv_main;
    int phase;
    int generation;   // incremented each time a new phase is launched
    int done_count;

    ThreadPool(int T) :
        local_mins(T), local_as(T), local_bs(T),
        phase(0), generation(0), done_count(0) {}
};

static void worker(ThreadPool& pool, int thread_id) {
    int last_generation = 0;

    while (true) {
        int current_phase;
        {
            std::unique_lock<std::mutex> lock(pool.mtx);
            pool.cv_workers.wait(lock, [&]{
                return pool.generation != last_generation;
            });
            last_generation = pool.generation;
            current_phase = pool.phase;
        }

        if (current_phase == 3) return;

        int n = pool.n;
        int T = pool.num_threads;

        if (current_phase == 1) {
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
            auto& dist = const_cast<std::vector<std::vector<double>>&>(*pool.dist);
            int a = pool.a, b = pool.b;
            int sa = pool.size_a, sb = pool.size_b;
            Linkage lk = pool.linkage;
            for (int k = thread_id; k < n; k += T) {
                if (!(*pool.active)[k] || k == a || k == b) continue;
                dist[a][k] = lance_williams(dist[a][k], dist[b][k], sa, sb, lk);
                dist[k][a] = dist[a][k];
            }
        }

        {
            std::unique_lock<std::mutex> lock(pool.mtx);
            pool.done_count++;
            if (pool.done_count == pool.num_threads)
                pool.cv_main.notify_one();
        }
    }
}

static void run_phase(ThreadPool& pool, int phase_id) {
    {
        std::lock_guard<std::mutex> lock(pool.mtx);
        pool.done_count = 0;
        pool.phase = phase_id;
        pool.generation++;
        pool.cv_workers.notify_all();
    }
    {
        std::unique_lock<std::mutex> lock(pool.mtx);
        pool.cv_main.wait(lock, [&]{
            return pool.done_count == pool.num_threads;
        });
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
    pool.dist        = &dist;
    pool.active      = &active;
    pool.n           = n;
    pool.num_threads = num_threads;
    pool.linkage     = linkage;

    std::vector<std::thread> threads(num_threads);
    for (int t = 0; t < num_threads; t++)
        threads[t] = std::thread(worker, std::ref(pool), t);

    for (int step = 0; step < n - 1; step++) {

        run_phase(pool, 1);

        double min_dist = std::numeric_limits<double>::infinity();
        int a = -1, b = -1;
        for (int t = 0; t < num_threads; t++) {
            if (pool.local_mins[t] < min_dist) {
                min_dist = pool.local_mins[t];
                a = pool.local_as[t];
                b = pool.local_bs[t];
            }
        }

        result.push_back({a, b, min_dist, size[a] + size[b]});

        active[b]   = false;
        pool.a      = a;
        pool.b      = b;
        pool.size_a = size[a];
        pool.size_b = size[b];

        run_phase(pool, 2);

        size[a] += size[b];
    }

    {
        std::lock_guard<std::mutex> lock(pool.mtx);
        pool.phase = 3;
        pool.generation++;
        pool.cv_workers.notify_all();
    }
    for (auto& th : threads) th.join();

    return result;
}