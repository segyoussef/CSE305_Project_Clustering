// Parallel Boruvka's algorithm for the Euclidean MST.
//
// Ref: Rajasekaran (2005) Sec. 3.2 (Sollin's algorithm), adapted from
// the AROB model to shared-memory CPU using std::thread.
//
// Parallelization strategy: persistent worker threads are created once and
// reused across Boruvka phases. Each phase, the main thread snapshots the
// current component root of every point, then signals workers with a condition
// variable. Each worker scans a disjoint subset of all (i, j) point pairs and
// tracks the cheapest outgoing edge per component locally. At the end of each
// phase, results are merged sequentially. Union-find is sequential between
// phases.
//
#pragma once

#include "boruvka_sequential.h"   // for MSTEdge
#include "common/point.h"
#include <vector>

std::vector<MSTEdge> boruvka_mst_parallel(const std::vector<Point>& points,
                                          int num_threads);