// This file declares the parallel matrix-based hierarchical clustering algorithm.
// Uses persistent threads with condition variables to avoid repeated thread creation overhead.
#pragma once
#include "../common/point.h"
#include "../common/dendrogram.h"
#include "lance_williams.h"
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

Dendrogram matrix_hc_parallel(const std::vector<Point>& points, Linkage linkage, int num_threads);