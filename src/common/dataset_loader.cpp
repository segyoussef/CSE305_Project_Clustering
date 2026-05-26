// This file implements the dataset loader, reading points and optional labels from a CSV file.
#include "dataset_loader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

std::vector<Point> load_dataset(const std::string& path, std::vector<int>* labels) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Could not open file: " + path);

    std::vector<Point> points;
    std::string line;

    // Read header and check if there is a label column
    std::getline(file, line); 
    bool has_label = (line.find("label") != std::string::npos);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        std::vector<double> coords;

        while (std::getline(ss, token, ','))
            coords.push_back(std::stod(token));

        if (has_label && labels != nullptr) {
            int label = (int)coords.back();
            coords.pop_back();
            labels->push_back(label);
        }

        points.push_back({coords});
    }
    return points;
}
