#include "mst_to_dendrogram.h"
#include "union_find.h"

#include <algorithm>
#include <vector>

namespace {

// same tie-breaking rule as in boruvka_sequential.cpp
bool edge_less(const MSTEdge& a, const MSTEdge& b) {
    if (a.weight != b.weight) return a.weight < b.weight;
    int a_lo = std::min(a.u, a.v), a_hi = std::max(a.u, a.v);
    int b_lo = std::min(b.u, b.v), b_hi = std::max(b.u, b.v);
    if (a_lo != b_lo) return a_lo < b_lo;
    return a_hi < b_hi;
}

} 

Dendrogram mst_to_dendrogram(const std::vector<MSTEdge>& mst, int n_points) {
    // sort MST edges by weight ascending, with consistent tie-breaking
    std::vector<MSTEdge> sorted_edges = mst;
    std::sort(sorted_edges.begin(), sorted_edges.end(), edge_less);

    // cluster_id[i] = the scipy-style ID of the cluster currently containing point i
    // initially, point i is in cluster i
    std::vector<int> cluster_id(n_points);
    std::vector<int> cluster_size(n_points, 1);
    for (int i = 0; i < n_points; ++i) cluster_id[i] = i;

    UnionFind uf(n_points);
    Dendrogram dendro;
    dendro.reserve(sorted_edges.size());

    // each merge gets a fresh cluster ID starting at n_points
    int next_id = n_points;

    for (const MSTEdge& e : sorted_edges) {
        int ra = uf.find(e.u);
        int rb = uf.find(e.v);

        Merge m;
        m.cluster_a = cluster_id[ra];
        m.cluster_b = cluster_id[rb];
        m.distance  = e.weight;
        m.new_size  = cluster_size[ra] + cluster_size[rb];
        dendro.push_back(m);

        uf.unite(e.u, e.v);
        int new_root = uf.find(e.u);
        cluster_id[new_root] = next_id;
        cluster_size[new_root] = m.new_size;
        ++next_id;
    }

    return dendro;
}