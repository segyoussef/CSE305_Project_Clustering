#pragma once
#include <vector>
#include "../common/point.h"
#include "../common/dendrogram.h"
#include "../matrix_cpu/lance_williams.h"

Dendrogram matrix_hc_cuda(const std::vector<Point>& points, Linkage linkage, int block_size = 256);
