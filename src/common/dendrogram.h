// This file defines the Dendrogram structure and read/write functions for the clustering output.
#pragma once
#include <vector>
#include <iostream>

struct Merge {
    int cluster_a;   
    int cluster_b;  
    double distance; 
    int new_size;  
};

using Dendrogram = std::vector<Merge>;

void write_dendrogram(std::ostream& out, const Dendrogram& d);
Dendrogram read_dendrogram(std::istream& in);