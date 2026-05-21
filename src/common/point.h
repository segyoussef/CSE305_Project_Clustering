// This file defines the Point struct, a point in R^d is just a vector of d coordinates.

#pragma once
#include <vector>

struct Point {
    std::vector<double> coords;  // dimension d = coords.size()
};
