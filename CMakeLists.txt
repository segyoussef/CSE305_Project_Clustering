cmake_minimum_required(VERSION 3.10)
project(CSE305_HC LANGUAGES CXX CUDA)

# RTX 3090 and RTX A5000 (in the cs rooms) are both ampere sm_86
set(CMAKE_CUDA_ARCHITECTURES 86)
set(CMAKE_CUDA_STANDARD 17)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -pthread")

# Allegra's MST executable
# TODO: uncomment as source files are added
 add_executable(hc_mst_cpu
     src/mst/main.cpp
     src/mst/boruvka_sequential.cpp
     src/mst/union_find.cpp
     src/mst/mst_to_dendrogram.cpp
     src/common/distance.cpp
     src/common/dataset_loader.cpp
     src/common/dendrogram.cpp
 )
 target_include_directories(hc_mst_cpu PRIVATE src)


# Kamil's matrix CPU executable
# TODO: uncomment as source files are added
add_executable(hc_matrix_cpu
     src/matrix_cpu/main.cpp
     src/matrix_cpu/matrix_hc_sequential.cpp
     src/matrix_cpu/lance_williams.cpp
     src/common/distance.cpp
     src/common/dataset_loader.cpp
     src/common/dendrogram.cpp
     src/matrix_cpu/matrix_hc_parallel.cpp
 )
target_include_directories(hc_matrix_cpu PRIVATE src)

# Youssef's matrix CUDA executable
#add_executable(hc_matrix_cuda
#    src/matrix_cuda/main.cpp
#    src/matrix_cuda/matrix_hc_cuda.cu
#    src/matrix_cuda/kernels.cu
#    src/common/dataset_loader.cpp
#    src/common/dendrogram.cpp
#    src/common/distance.cpp
#)
#target_include_directories(hc_matrix_cuda PRIVATE src)

# Standalone tests
add_executable(test_union_find tests/test_union_find.cpp src/mst/union_find.cpp)
target_include_directories(test_union_find PRIVATE src)