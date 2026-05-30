// CLI entry point for the MST-based single-link clustering executable.
// Usage: ./hc_mst_cpu <dataset.csv> [--threads N]
//
// For now, this just prints the MST. We'll add dendrogram output in the next step.

#include "boruvka_sequential.h"
#include "boruvka_parallel.h"
#include "mst_to_dendrogram.h"
#include "common/dataset_loader.h"
#include "common/dendrogram.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <dataset.csv> [--threads N]\n";
        return 1;
    }

    std::string dataset_path = argv[1];
    int num_threads = 1;  // default to sequential
    bool has_labels = false;

    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            num_threads = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--has-labels") == 0) {
            has_labels = true;
        }
    }

    std::vector<int> labels;
    std::vector<Point> points = load_dataset(
        dataset_path,
        has_labels ? &labels : nullptr
    );
    std::cerr << "Loaded " << points.size() << " points (d="
              << (points.empty() ? 0 : points[0].coords.size())
              << "). threads=" << num_threads << "\n";

    auto t0 = std::chrono::steady_clock::now();
    std::vector<MSTEdge> mst;
    if (num_threads <= 1) {
        mst = boruvka_mst_sequential(points);
    } else {
        mst = boruvka_mst_parallel(points, num_threads);
    }
    Dendrogram dendro = mst_to_dendrogram(mst, static_cast<int>(points.size()));
    auto t1 = std::chrono::steady_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cerr << "runtime_ms: " << ms << "\n";

    // For now, print MST edges to stdout.
    //std::cout << "u,v,weight\n";
    //std::cout << std::setprecision(10);
    //for (const auto& e : mst) {
    //    std::cout << e.u << "," << e.v << "," << e.weight << "\n";
    //}

    // write dendogram (after mst --> dendogram conversion)
    write_dendrogram(std::cout, dendro);

    return 0;
}