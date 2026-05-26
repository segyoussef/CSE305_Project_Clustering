// This file declares the parallel matrix-based hierarchical clustering algorithm.
#pragma once
#include "../common/point.h"
#include "../common/dendrogram.h"
#include "lance_williams.h"
#include <vector>

Dendrogram matrix_hc_parallel(const std::vector<Point>& points, Linkage linkage, int num_threads);