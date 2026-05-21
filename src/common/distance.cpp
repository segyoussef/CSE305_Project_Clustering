// This file implements the distance functions declared in distance.h
#include "distance.h"
#include <cmath>

double euclidean(const Point& a, const Point& b) {
    return std::sqrt(euclidean_squared(a, b));
}

double euclidean_squared(const Point& a, const Point& b) {
    double sum = 0.0;
    for (size_t i = 0; i < a.coords.size(); i++) {
        double diff = a.coords[i] - b.coords[i];
        sum += diff * diff;
    }
    return sum;
}