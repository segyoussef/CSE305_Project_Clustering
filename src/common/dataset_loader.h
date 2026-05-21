// This file declares the function to load a dataset from a CSV file into a vector of Points.
#pragma once
#include "point.h"
#include <string>
#include <vector>

std::vector<Point> load_dataset(const std::string& path, std::vector<int>* labels = nullptr);
