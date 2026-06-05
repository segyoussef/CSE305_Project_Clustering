#include "boruvka_parallel.h"
#include "union_find.h"
#include "common/distance.h"

#include <algorithm>
#include <thread>
#include <vector>
#include <limits>
#include <condition_variable>
#include <mutex>

namespace {

// same tie-breaking rule as in boruvka_sequential.cpp to keep results
// directly comparable across the two implementations
bool edge_is_better(const MSTEdge& a, const MSTEdge& b) {
    if (a.weight != b.weight) return a.weight < b.weight;
    int a_lo = std::min(a.u, a.v), a_hi = std::max(a.u, a.v);
    int b_lo = std::min(b.u, b.v), b_hi = std::max(b.u, b.v);
    if (a_lo != b_lo) return a_lo < b_lo;
    return a_hi < b_hi;
}

constexpr int NO_EDGE = -1;

// a "found edge" with its weight already computed. We track these per thread.
struct CheapestEdge {
    int root;       // the component root this edge is the cheapest for
    MSTEdge edge;
    bool valid;
};

CheapestEdge empty_cheapest_edge() {
    return {NO_EDGE, {}, false};
}

// scans pairs (i, j) where i is in [i_start, i_end), j > i.
// for each pair in different components, updates this worker's local
// cheapest_edge[root] map.
// The component_roots vector is a per-phase snapshot, so workers do not call
// UnionFind::find() concurrently.
void scan_range(int i_start,
                 int i_end,
                 int n,
                 const std::vector<Point>& points,
                 const std::vector<int>& component_roots,
                 std::vector<CheapestEdge>& local_cheapest) {
    for (int i = i_start; i < i_end; ++i) {
        int ri = component_roots[i];
        for (int j = i + 1; j < n; ++j) {
            int rj = component_roots[j];
            if (ri == rj) continue;

            MSTEdge e{i, j, euclidean(points[i], points[j])};

            // Update for ri's component
            if (!local_cheapest[ri].valid ||
                edge_is_better(e, local_cheapest[ri].edge)) {
                local_cheapest[ri] = {ri, e, true};
            }
            // Update for rj's component
            if (!local_cheapest[rj].valid ||
                edge_is_better(e, local_cheapest[rj].edge)) {
                local_cheapest[rj] = {rj, e, true};
            }
        }
    }
}

}  // namespace

std::vector<MSTEdge> boruvka_mst_parallel(const std::vector<Point>& points,
                                          int num_threads) {
    const int n = static_cast<int>(points.size());
    std::vector<MSTEdge> mst;
    mst.reserve(n - 1);

    if (n <= 1) return mst;
    if (num_threads < 1) num_threads = 1;

    UnionFind uf(n);

    // Persistent worker state. Threads are created once and reused for every
    // Boruvka phase, avoiding repeated fork/join overhead.
    std::vector<int> component_roots(n);
    std::vector<std::vector<CheapestEdge>> local_results(num_threads);
    for (auto& v : local_results) {
        v.assign(n, empty_cheapest_edge());
    }

    std::mutex phase_mutex;
    std::condition_variable phase_start;
    std::condition_variable phase_done;
    int phase_id = 0;
    int finished_workers = 0;
    bool stop_workers = false;

    auto worker_loop = [&](int t) {
        const int i_start = (n * t) / num_threads;
        const int i_end = (n * (t + 1)) / num_threads;
        int seen_phase = 0;

        while (true) {
            {
                std::unique_lock<std::mutex> lock(phase_mutex);
                phase_start.wait(lock, [&] {
                    return stop_workers || phase_id > seen_phase;
                });
                if (stop_workers) return;
                seen_phase = phase_id;
            }

            std::fill(local_results[t].begin(), local_results[t].end(),
                      empty_cheapest_edge());
            scan_range(i_start, i_end, n, points, component_roots,
                       local_results[t]);

            {
                std::lock_guard<std::mutex> lock(phase_mutex);
                ++finished_workers;
            }
            phase_done.notify_one();
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back(worker_loop, t);
    }

    while (uf.num_components() > 1) {
        {
            std::lock_guard<std::mutex> lock(phase_mutex);
            for (int i = 0; i < n; ++i) {
                component_roots[i] = uf.find(i);
            }
            finished_workers = 0;
            ++phase_id;
        }
        phase_start.notify_all();

        {
            std::unique_lock<std::mutex> lock(phase_mutex);
            phase_done.wait(lock, [&] {
                return finished_workers == num_threads;
            });
        }

        // merge per-thread results: for each vertex v, take the best edge
        // found across all threads
        std::vector<CheapestEdge> global(n, {NO_EDGE, {}, false});
        for (const auto& thread_result : local_results) {
            for (int v = 0; v < n; ++v) {
                if (!thread_result[v].valid) continue;
                if (!global[v].valid ||
                    edge_is_better(thread_result[v].edge, global[v].edge)) {
                    global[v] = thread_result[v];
                }
            }
        }

        // apply the chosen edges. The deduplication via unite() handles the
        // case where the same edge was chosen by both endpoints' components
        for (int v = 0; v < n; ++v) {
            if (!global[v].valid) continue;
            const MSTEdge& e = global[v].edge;
            if (uf.unite(e.u, e.v)) {
                mst.push_back(e);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(phase_mutex);
        stop_workers = true;
    }
    phase_start.notify_all();
    for (auto& worker : workers) {
        worker.join();
    }

    return mst;
}