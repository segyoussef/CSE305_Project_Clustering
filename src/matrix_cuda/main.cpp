#include <iostream>
#include <chrono>
#include <string>
#include "../common/dataset_loader.h"
#include "../common/dendrogram.h"
#include "matrix_hc_cuda.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./hc_matrix_cuda <dataset_path> [--linkage single|complete|average] [--block-size 32|64|128|256|512]\n";
        return 1;
    }

    std::string path = argv[1];
    Linkage linkage = Linkage::SINGLE;
    int block_size = 256;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--linkage" && i + 1 < argc) {
            std::string val = argv[++i];
            if      (val == "complete") linkage = Linkage::COMPLETE;
            else if (val == "average")  linkage = Linkage::AVERAGE;
        } else if (arg == "--block-size" && i + 1 < argc) {
            block_size = std::stoi(argv[++i]);
        }
    }

    std::vector<int>   labels;
    std::vector<Point> points = load_dataset(path, &labels);

    auto start = std::chrono::high_resolution_clock::now();
    Dendrogram d = matrix_hc_cuda(points, linkage, block_size);
    auto end   = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    write_dendrogram(std::cout, d);
    std::cerr << "runtime_ms: " << ms << "\n";

    return 0;
}
