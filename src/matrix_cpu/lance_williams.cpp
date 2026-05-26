// This file implements the Lance-Williams update formula for each linkage type.
#include "lance_williams.h"
#include <algorithm>

double lance_williams(double d_ac, double d_bc, int size_a, int size_b, Linkage linkage) {
    switch (linkage) {
        case Linkage::SINGLE:
            return std::min(d_ac, d_bc);
        case Linkage::COMPLETE:
            return std::max(d_ac, d_bc);
        case Linkage::AVERAGE:
            return (size_a * d_ac + size_b * d_bc) / (size_a + size_b);
    }
    return 0.0;
}