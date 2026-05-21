// This file implements the read/write functions for the Dendrogram structure.
#include "dendrogram.h"
#include <sstream>
#include <string>

void write_dendrogram(std::ostream& out, const Dendrogram& d) {
    out << "cluster_a,cluster_b,distance,new_size\n";
    for (const Merge& m : d) {
        out << m.cluster_a << ","
            << m.cluster_b << ","
            << m.distance  << ","
            << m.new_size  << "\n";
    }
}

Dendrogram read_dendrogram(std::istream& in) {
    Dendrogram d;
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string token;
        Merge m;
        std::getline(ss, token, ','); m.cluster_a = std::stoi(token);
        std::getline(ss, token, ','); m.cluster_b = std::stoi(token);
        std::getline(ss, token, ','); m.distance  = std::stod(token);
        std::getline(ss, token, ','); m.new_size  = std::stoi(token);
        d.push_back(m);
    }
    return d;
}