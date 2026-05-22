// sequential implementation of Boruvka's algorithm for the Euclidean MST
//
// ref: Rajasekaran (2005), Sec. 3.2 "An AROB Algorithm"
//   "We adapt Sollin's algorithm... There are O(log n) phases in Sollin's
//   algorithm. The algorithm has a forest of trees at any time... Phase s
//   of the algorithm runs as follows: For each tree T in F_s, determine
//   the edge e_T with the minimum weight that connects a node in T to a
//   node outside T. Add these edges to the forest F_s to obtain F_{s+1}."
//
// We adapt this to shared-memory CPU (rather than the AROB model) using
// std::thread (parallel version in boruvka_parallel.cpp).

// we compute distances on the complete graph implicitly: for every pair (i,j)
// the edge weight is the Euclidean distance between points[i] and points[j]

#include "boruvka_sequential.h"
#include "union_find.h"
#include "common/distance.h"

#include <limits>
#include <utility>

namespace {

// tie-breaking rule: when two edges have equal weight, prefer the one with the
// lexicographically smaller (min(u,v), max(u,v)) pair. matches what I did in the hand-trace (see docs/boruvka_hand_trace.md)
bool edge_is_better(const MSTEdge& a, const MSTEdge& b) {
    if (a.weight != b.weight) return a.weight < b.weight;
    int a_lo = std::min(a.u, a.v), a_hi = std::max(a.u, a.v);
    int b_lo = std::min(b.u, b.v), b_hi = std::max(b.u, b.v);
    if (a_lo != b_lo) return a_lo < b_lo;
    return a_hi < b_hi;
}

constexpr int NO_EDGE = -1;

}  

std::vector<MSTEdge> boruvka_mst_sequential(const std::vector<Point>& points) {
    const int n = static_cast<int>(points.size());
    std::vector<MSTEdge> mst;
    mst.reserve(n - 1);

    if (n <= 1) return mst;

    UnionFind uf(n);

    // boruvka proceeds in phases. each phase, every current component finds
    // its cheapest outgoing edge. all those edges are then added at once.
    while (uf.num_components() > 1) {
        // for each component (identified by its UnionFind root), store the
        // index of the cheapest outgoing edge found this phase, or NO_EDGE.
        // we use a flat vector indexed by vertex, only the root index per
        // component is meaningful.
        std::vector<int> cheapest_edge_idx(n, NO_EDGE);
        std::vector<MSTEdge> candidate_edges;

        // scan every pair (i, j) with i < j. For each pair in different
        // components, see if it's a new minimum for either component's root.
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                int ri = uf.find(i);
                int rj = uf.find(j);
                if (ri == rj) continue;  // already in same component

                MSTEdge e{i, j, euclidean(points[i], points[j])};
                int edge_idx = static_cast<int>(candidate_edges.size());
                candidate_edges.push_back(e);

                if (cheapest_edge_idx[ri] == NO_EDGE ||
                    edge_is_better(e, candidate_edges[cheapest_edge_idx[ri]])) {
                    cheapest_edge_idx[ri] = edge_idx;
                }
                if (cheapest_edge_idx[rj] == NO_EDGE ||
                    edge_is_better(e, candidate_edges[cheapest_edge_idx[rj]])) {
                    cheapest_edge_idx[rj] = edge_idx;
                }
            }
        }

        // add each component's cheapest edge (deduplicate: an edge can be
        // chosen by both endpoints' components).
        for (int v = 0; v < n; ++v) {
            int idx = cheapest_edge_idx[v];
            if (idx == NO_EDGE) continue;
            const MSTEdge& e = candidate_edges[idx];
            // unite() returns true iff a real merge happened. If false, this
            // edge was already chosen and added via the other endpoint
            if (uf.unite(e.u, e.v)) {
                mst.push_back(e);
            }
        }
    }

    return mst;
}