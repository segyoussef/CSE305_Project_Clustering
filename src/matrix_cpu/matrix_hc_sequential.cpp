// This file implements the sequential matrix-based hierarchical clustering algorithm.
#include "matrix_hc_sequential.h"
#include "../common/distance.h"
#include <limits>

Dendrogram matrix_hc_sequential(const std::vector<Point>& points, Linkage linkage) {
    int n = points.size();
    
    // We first initialize distance matrix
    std::vector<std::vector<double>> dist(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            dist[i][j] = euclidean(points[i], points[j]);

    // We then track cluster sizes and which clusters are still active
    std::vector<int> size(n, 1);
    std::vector<bool> active(n, true);

    Dendrogram result;

    for (int step = 0; step < n - 1; step++) {
        // We find the minimum distance among active clusters
        double min_dist = std::numeric_limits<double>::infinity();
        int a = -1, b = -1;
        for (int i = 0; i < n; i++) {
            if (!active[i]) continue;
            for (int j = i + 1; j < n; j++) {     // We will need to parallelize it
                if (!active[j]) continue;
                if (dist[i][j] < min_dist) {
                    min_dist = dist[i][j];
                    a = i;
                    b = j;
                }
            }
        }

        // We now record the merge
        result.push_back({a, b, min_dist, size[a] + size[b]});

        // We update the distances from merged cluster (a) to all other active clusters
        for (int k = 0; k < n; k++) {
            if (!active[k] || k == a || k == b) continue;
            dist[a][k] = lance_williams(dist[a][k], dist[b][k], size[a], size[b], linkage);
            dist[k][a] = dist[a][k];
        }

        // Merge b into a
        size[a] += size[b];
        active[b] = false;
    }

    return result;
}