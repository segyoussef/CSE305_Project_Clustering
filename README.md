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

This separation lets the benchmark script save clustering output separately
from timing information.

## Build on Salle Info

From the repository root:

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
cd ..
```

If CUDA is available, CMake builds `hc_matrix_cuda`. If CUDA is not available,
the CUDA target is skipped and the CPU targets can still be built.

## Run One Experiment

MST-based single-link clustering:

```bash
./build/hc_mst_cpu eval/datasets/random_n1000.csv --threads 4 > mst_out.csv
```

Matrix-based CPU clustering:

```bash
./build/hc_matrix_cpu eval/datasets/random_n1000.csv --threads 4 --linkage single > matrix_cpu_out.csv
```

CUDA clustering:

```bash
./build/hc_matrix_cuda eval/datasets/random_n1000.csv --linkage single --block-size 256 > cuda_out.csv
```

For labelled datasets such as `donut3`, `complex9`, and `diamond9`, pass
`--has-labels` to `hc_mst_cpu` so that the final label column is not treated as
a coordinate:

```bash
./build/hc_mst_cpu eval/datasets/complex9.csv --has-labels --threads 8 > mst_complex9_out.csv
```

## Benchmarking

The benchmark script runs the same dataset suite across the selected
executables. CPU executables are tested over different thread counts. CUDA is
tested over different block sizes.

Recommended MST benchmark:

```bash
python3 eval/benchmark.py \
  --executables build/hc_mst_cpu \
  --threads 1 2 3 4 8 12 16 \
  --reps 5 \
  --out results/runtimes_persistent.csv \
  --summary-out results/summary_persistent.csv \
  --hardware-note "Salle Info: Intel Xeon W-1270P 3.80GHz, 16 logical cores"
```

Full benchmark, including CUDA block-size experiments:

```bash
python3 eval/benchmark.py \
  --threads 1 2 3 4 8 12 16 \
  --block-sizes 32 64 128 256 512 \
  --reps 3 \
  --hardware-note "Salle Info: Intel Xeon W-1270P 3.80GHz, 16 logical cores; NVIDIA RTX 3090 when using CUDA"
```

The benchmark writes:

- `results/runtimes.csv`: raw per-run timings
- `results/summary.csv`: median runtime per configuration, including speedup
  versus one thread for CPU executables

Each row records the dataset name, number of points `n`, dimension `d`, dataset
source, thread count or CUDA block size, runtime, timestamp, and hardware note.

## Plotting

Plot generation requires `pandas` and `matplotlib`:

```bash
python3 -m pip install --user pandas matplotlib
```

Then run:

```bash
python3 eval/plots.py
```

Plots and a plot-ready summary table are written to `results/plots/`.

## Dataset Suite

The benchmark uses two-dimensional datasets.

Synthetic random datasets, used to study scaling with input size:

- `random_n100`
- `random_n500`
- `random_n1000`
- `random_n2000`
- `random_n5000`

Structured labelled datasets converted from the milaan9 collection:

- `donut3`, `N=999`
- `complex9`, `N=3031`
- `diamond9`, `N=3000`

The milaan9 conversion script is:

```bash
python3 eval/convert_milaan9.py
```

## Notes on MST Parallelization

The MST implementation uses Boruvka's algorithm for single-link clustering.

The initial parallel version created and joined worker threads at every Boruvka
phase. Following feedback, we implemented a persistent-thread version: worker
threads are created once and reused across phases, with condition variables used
to signal the start and end of each phase.

The sequential implementation is still used when running with:

```bash
--threads 1
```

The persistent-thread parallel implementation is used when running with two or
more threads.
