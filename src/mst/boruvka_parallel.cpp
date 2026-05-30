#include "boruvka_parallel.h"
#include "union_find.h"
#include "common/distance.h"

#include <algorithm>
#include <thread>
#include <vector>
#include <limits>

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

// worker: scans pairs (i, j) where i is in [i_start, i_end), j > i.
// for each pair in different components, updates this worker's local
// cheapest_edge[root] map (encoded as a vector indexed by vertex)
void scan_worker(int i_start,
                 int i_end,
                 int n,
                 const std::vector<Point>& points,
                 UnionFind& uf,
                 std::vector<CheapestEdge>& local_cheapest) {
    for (int i = i_start; i < i_end; ++i) {
        int ri = uf.find(i);
        for (int j = i + 1; j < n; ++j) {
            int rj = uf.find(j);
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

    while (uf.num_components() > 1) {
        // each thread gets its own cheapest_edge array, indexed by vertex.
        // only entries at union-find roots are meaningful
        std::vector<std::vector<CheapestEdge>> local_results(num_threads);
        for (auto& v : local_results) {
            v.assign(n, {NO_EDGE, {}, false});
        }

        // split the outer i-loop across threads. static block partitioning
        // by i is fine; the inner j-loop length is n-i, so threads with
        // smaller i do more work, but the imbalance is at most 2x.
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        for (int t = 0; t < num_threads; ++t) {
            int i_start = (n * t) / num_threads;
            int i_end   = (n * (t + 1)) / num_threads;
            threads.emplace_back(scan_worker, i_start, i_end, n,
                                 std::cref(points), std::ref(uf),
                                 std::ref(local_results[t]));
        }
        for (auto& th : threads) th.join();

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

    return mst;
}