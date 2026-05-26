// This file declares the Lance-Williams update formula for the different linkage types.
#pragma once

enum class Linkage { SINGLE, COMPLETE, AVERAGE };

double lance_williams(double d_ac, double d_bc, int size_a, int size_b, Linkage linkage);