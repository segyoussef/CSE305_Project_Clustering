// converts an MST into a single-link dendrogram
//
// ref: Rajasekaran (2005) Sec. 2.3 "Hierarchical Clustering Primitives":
//   "The dendrogram is nothing but a tree that shows which clusters were
//   merged at each step. In fact, this tree can be easily constructed from
//   a Euclidean minimum spanning tree of the n input points."
//
#pragma once

#include "boruvka_sequential.h"   // for MSTEdge
#include "common/dendrogram.h"    // for Dendrogram, Merge
#include <vector>

Dendrogram mst_to_dendrogram(const std::vector<MSTEdge>& mst, int n_points);