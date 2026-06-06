# CSE305 Project: Parallel Hierarchical Clustering

This repository contains our CSE305 project on parallel hierarchical
clustering.

We implemented three versions:

- `hc_matrix_cpu`: matrix-based CPU hierarchical clustering using `std::thread`
- `hc_matrix_cuda`: matrix-based CUDA hierarchical clustering
- `hc_mst_cpu`: MST/Boruvka-based CPU implementation for single-link clustering

All implementations read CSV datasets from `eval/datasets/` and write the
dendrogram to standard output. Runtime is written to standard error as:

```text
runtime_ms: ...
```

This separation allows dendrogram output to be redirected to a file while the
benchmark runner extracts timing information independently.

## Clone the Repository

```bash
git clone https://github.com/Allegra0077/CSE305_Project_Clustering.git
cd CSE305_Project_Clustering
```

## Build on Salle Info

From the repository root:

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc
make -j4
cd ..
```

This builds the following executables:

```text
build/hc_matrix_cpu
build/hc_matrix_cuda
build/hc_mst_cpu
```

## Run the Implementations

### MST-Based CPU Implementation

The MST implementation supports single linkage only:

```bash
./build/hc_mst_cpu eval/datasets/random_n1000.csv --threads 4 > mst_out.csv
```

With `--threads 1`, the standalone sequential implementation is used. With two
or more threads, the persistent-thread parallel implementation is used.

For labelled datasets, pass `--has-labels` so that the final label column is not
treated as an additional coordinate:

```bash
./build/hc_mst_cpu \
  eval/datasets/complex9.csv \
  --has-labels \
  --threads 8 \
  > mst_complex9_out.csv
```

This option is required for `donut3`, `complex9`, `diamond9`, and `optdigits`
when running the MST executable manually.

### Matrix-based CPU Implementation

```bash
./build/hc_matrix_cpu eval/datasets/random_n1000.csv --threads 4 --linkage single > matrix_cpu_out.csv
```

Supported linkage criteria are:

```text
single
complete
average
``` 

### Matrix-Based CUDA Implementation

```bash
./build/hc_matrix_cuda eval/datasets/random_n1000.csv --linkage single --block-size 256 > cuda_out.csv
```

Supported CUDA block sizes used in the evaluation are:

```text
32 64 128 256 512
```

## Python Dependencies

The evaluation and plotting scripts require Python 3 and the following
packages:

```bash
python3 -m pip install --user numpy pandas scipy matplotlib
```

## Benchmarking

The benchmark runner is `eval/benchmark.py`. It records:

- the dataset name, source, number of points `n`, and dimension `d`
- whether the dataset contains labels
- the executable and linkage criterion
- the thread count or CUDA block size
- every individual runtime
- the median runtime and CPU speedup
- the date, hostname, platform, and supplied hardware description

The benchmark detects labelled datasets automatically and passes
`--has-labels` when running the MST executable.
### MST Benchmark

Run the two-dimensional random and structured datasets with five repetitions:

```bash
python3 eval/benchmark.py \
  --executables build/hc_mst_cpu \
  --datasets random_n100 random_n500 random_n1000 random_n2000 random_n5000 donut3 complex9 diamond9 \
  --threads 1 2 3 4 8 12 16 \
  --reps 5 \
  --out results/runtimes_persistent.csv \
  --summary-out results/summary_persistent.csv \
  --hardware-note "Salle Info: Intel Xeon W-1270P 3.80GHz, 16 logical cores"
```

Run the high-dimensional dataset separately:

```bash
python3 eval/benchmark.py \
  --executables build/hc_mst_cpu \
  --datasets optdigits \
  --threads 1 2 3 4 8 12 16 \
  --reps 5 \
  --out results/runtimes_persistent_optdigits.csv \
  --summary-out results/summary_persistent_optdigits.csv \
  --hardware-note "Salle Info: Intel Xeon W-1270P 3.80GHz, 16 logical cores"
```

Separate output filenames prevent earlier benchmark results from being
overwritten.

### Complete Benchmark

To benchmark all available implementations:

```bash
python3 eval/benchmark.py \
  --threads 1 2 3 4 8 12 16 \
  --block-sizes 32 64 128 256 512 \
  --reps 3 \
  --out results/runtimes.csv \
  --summary-out results/summary.csv \
  --hardware-note "Salle Info: Intel Xeon W-1270P 3.80GHz, 16 logical cores; NVIDIA GeForce RTX 3090"
```

Executables that have not been built are skipped. This complete benchmark can
take a long time, particularly for the matrix implementations on the largest
datasets.

## Plotting

### General Benchmark Plots

The general plotting script reads `results/runtimes.csv`:

```bash
python3 eval/plots.py \
  --in-csv results/runtimes.csv \
  --out-dir results/plots
```

It generates runtime, CPU speedup, CUDA block-size, and per-dataset plots.

### MST Report Plots

This dedicated script combines the main MST summary with the separately collected
`optdigits` results and generates the exact PDF and PNG figures used in the
report.

```bash
python3 eval/mst_report_plots.py \
  --persistent-summary results/summary_persistent.csv \
  --optdigits-summary results/summary_persistent_optdigits.csv \
  --out-dir results/plots
```

## Correctness Validation

### Comparison Against SciPy

Validate the sequential MST dendrogram against SciPy single linkage:

```bash
python3 eval/scipy_check.py \
  tests/hand_trace_10points.csv \
  build/hc_mst_cpu
```

A successful run reports that the merge distances agree within the numerical
tolerance.

The same check can be run on another dataset:

```bash
python3 eval/scipy_check.py \
  eval/datasets/complex9.csv \
  build/hc_mst_cpu
```

### Sequential and Parallel Equivalence

Compare complete sequential and parallel dendrogram outputs:

```bash
./build/hc_mst_cpu \
  eval/datasets/optdigits.csv \
  --has-labels \
  --threads 1 \
  > out_1.csv

./build/hc_mst_cpu \
  eval/datasets/optdigits.csv \
  --has-labels \
  --threads 16 \
  > out_16.csv

diff out_1.csv out_16.csv
rm out_1.csv out_16.csv
```

No output from `diff` means that the two dendrogram files are identical.

## Dataset Suite

The benchmark suite contains two-dimensional synthetic and structured datasets,
together with one high-dimensional real dataset.

Synthetic random datasets, used to study scaling with input size:

- `random_n100`
- `random_n500`
- `random_n1000`
- `random_n2000`
- `random_n5000`

Structured labelled datasets:

These two-dimensional datasets were obtained from the
[milaan9 Clustering-Datasets collection](https://github.com/milaan9/Clustering-Datasets)

- `donut3`, `N=999`
- `complex9`, `N=3031`
- `diamond9`, `N=3000`

The milaan9 conversion script is:

```bash
python3 eval/convert_milaan9.py
```

High-dimensional labelled dataset:

`optdigits` contains `N=5620` observations with `d=64` features and one class
label. It was obtained from the
[UCI Optical Recognition of Handwritten Digits dataset](https://doi.org/10.24432/C50P49).

The original training and test portions were combined because the dataset is
used for clustering rather than classification.

The conversion script is:

```bash
python3 eval/convert_optdigits.py
```

