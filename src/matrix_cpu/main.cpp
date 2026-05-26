// Entry point for the matrix-based CPU hierarchical clustering executable
#include <iostream>
#include <chrono>
#include <string>
#include "../common/dataset_loader.h"
#include "../common/dendrogram.h"
#include "matrix_hc_sequential.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./hc_matrix_cpu <dataset_path> [--linkage single|complete|average]\n";
        return 1;
    }

    std::string path = argv[1];
    Linkage linkage = Linkage::SINGLE;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--linkage" && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "complete") linkage = Linkage::COMPLETE;
            else if (val == "average") linkage = Linkage::AVERAGE;
        }
    }

    std::vector<int> labels;
    std::vector<Point> points = load_dataset(path, &labels);

    auto start = std::chrono::high_resolution_clock::now();
    Dendrogram d = matrix_hc_sequential(points, linkage);
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    write_dendrogram(std::cout, d);
    std::cerr << "runtime_ms: " << ms << "\n";

    return 0;
}