"""
Credits: Script generated with AI assistance; revised by the team.
Benchmark runner for the parallel hierarchical clustering project.

The runner records raw runtimes and a median summary for the shared report
benchmark suite. CPU executables are run over thread counts; CUDA executables
are run over block sizes.

Typical usage from the repository root:
    python3 eval/benchmark.py
    python3 eval/benchmark.py --executables build/hc_mst_cpu
    python3 eval/benchmark.py --datasets random_n1000 donut3
    python3 eval/benchmark.py --threads 1 2 3 4 8 12 16
    python3 eval/benchmark.py --block-sizes 32 64 128 256 512
    python3 eval/benchmark.py --reps 5
"""

import argparse
import csv
import platform
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent

DEFAULT_EXECUTABLES = [
    "build/hc_mst_cpu",
    "build/hc_matrix_cpu",
    "build/hc_matrix_cuda",
]

DEFAULT_DATASETS = [
    "random_n100",
    "random_n500",
    "random_n1000",
    "random_n2000",
    "random_n5000",
    "donut3",
    "complex9",
    "diamond9",
]

DEFAULT_THREADS = [1, 2, 3, 4, 8, 12, 16]
DEFAULT_BLOCK_SIZES = [32, 64, 128, 256, 512]
DEFAULT_REPS = 3

OUT_CSV = REPO_ROOT / "results" / "runtimes.csv"
SUMMARY_CSV = REPO_ROOT / "results" / "summary.csv"

RUNTIME_RE = re.compile(r"runtime_ms:\s*([0-9.eE+-]+)")


def is_cuda_executable(executable):
    return "cuda" in executable.name.lower()


def is_mst_executable(executable):
    return "mst" in executable.name.lower()


def dataset_source(dataset_name):
    if dataset_name in {"donut3", "complex9", "diamond9"}:
        return "milaan9 structured dataset"
    if dataset_name.startswith("random_n"):
        return "synthetic random 2D dataset"
    return "unknown"


def dataset_metadata(csv_path):
    with csv_path.open(newline="") as f:
        reader = csv.reader(f)
        header = next(reader)
        rows = sum(1 for _ in reader)

    has_labels = "label" in header
    dim = len(header) - (1 if has_labels else 0)
    return rows, dim, has_labels


def parse_runtime(stderr):
    match = RUNTIME_RE.search(stderr)
    return float(match.group(1)) if match else None


def command_for_run(executable, csv_path, has_labels, linkage, threads, block_size):
    cmd = [str(executable), str(csv_path)]

    if has_labels:
        cmd.append("--has-labels")

    if is_cuda_executable(executable):
        cmd += ["--linkage", linkage, "--block-size", str(block_size)]
    elif is_mst_executable(executable):
        cmd += ["--threads", str(threads)]
    else:
        cmd += ["--threads", str(threads), "--linkage", linkage]

    return cmd


def run_one(cmd, timeout):
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=True,
        )
    except subprocess.CalledProcessError as exc:
        return None, f"Non-zero exit: {exc.returncode}; stderr={exc.stderr[:300]}"
    except subprocess.TimeoutExpired:
        return None, "Timeout"
    except Exception as exc:
        return None, f"Exception: {exc}"

    runtime = parse_runtime(result.stderr)
    if runtime is None:
        return None, f"No runtime_ms in stderr: {result.stderr[:300]}"
    return runtime, None


def resolve_executables(paths):
    executables = []
    for executable in paths:
        path = REPO_ROOT / executable
        if path.exists():
            executables.append(path)
        else:
            print(f"  Skipping {executable} (not built)")
    if not executables:
        sys.exit("No executables found. Did you build the project?")
    return executables


def resolve_datasets(names):
    datasets = []
    for name in names:
        path = REPO_ROOT / "eval" / "datasets" / f"{name}.csv"
        if path.exists():
            n, dim, has_labels = dataset_metadata(path)
            datasets.append(
                {
                    "name": name,
                    "path": path,
                    "n": n,
                    "d": dim,
                    "has_labels": has_labels,
                    "source": dataset_source(name),
                }
            )
        else:
            print(f"  Skipping dataset {name} (not found at {path})")
    if not datasets:
        sys.exit("No datasets found.")
    return datasets


def run_parameters(executable, threads, block_sizes):
    if is_cuda_executable(executable):
        return [{"threads": "", "block_size": block} for block in block_sizes]
    return [{"threads": thread, "block_size": ""} for thread in threads]


def write_summary(raw_rows, summary_path):
    groups = {}
    for row in raw_rows:
        if row["runtime_ms"] == "":
            continue
        key = (
            row["executable"],
            row["dataset"],
            row["n"],
            row["d"],
            row["dataset_source"],
            row["linkage"],
            row["threads"],
            row["block_size"],
        )
        groups.setdefault(key, []).append(float(row["runtime_ms"]))

    summary_rows = []
    for key, runtimes in sorted(groups.items()):
        (
            executable,
            dataset,
            n,
            dim,
            source,
            linkage,
            threads,
            block_size,
        ) = key
        summary_rows.append(
            {
                "executable": executable,
                "dataset": dataset,
                "n": n,
                "d": dim,
                "dataset_source": source,
                "linkage": linkage,
                "threads": threads,
                "block_size": block_size,
                "reps": len(runtimes),
                "median_runtime_ms": statistics.median(runtimes),
                "speedup_vs_1_thread": "",
            }
        )

    baseline = {
        (row["executable"], row["dataset"], row["linkage"]): row["median_runtime_ms"]
        for row in summary_rows
        if row["threads"] == 1 and row["block_size"] == ""
    }

    for row in summary_rows:
        if row["threads"] == "" or row["block_size"] != "":
            continue
        base = baseline.get((row["executable"], row["dataset"], row["linkage"]))
        if base:
            row["speedup_vs_1_thread"] = base / row["median_runtime_ms"]

    summary_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "executable",
        "dataset",
        "n",
        "d",
        "dataset_source",
        "linkage",
        "threads",
        "block_size",
        "reps",
        "median_runtime_ms",
        "speedup_vs_1_thread",
    ]
    with summary_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(summary_rows)


def host_metadata(hardware_note):
    return {
        "host": platform.node(),
        "platform": platform.platform(),
        "cpu": platform.processor(),
        "hardware_note": hardware_note,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executables", nargs="+", default=DEFAULT_EXECUTABLES)
    parser.add_argument("--datasets", nargs="+", default=DEFAULT_DATASETS)
    parser.add_argument("--threads", "--cpu-threads", nargs="+", type=int,
                        default=DEFAULT_THREADS)
    parser.add_argument("--block-sizes", nargs="+", type=int,
                        default=DEFAULT_BLOCK_SIZES)
    parser.add_argument("--linkage", default="single",
                        choices=["single", "complete", "average"])
    parser.add_argument("--reps", type=int, default=DEFAULT_REPS)
    parser.add_argument("--out", default=str(OUT_CSV))
    parser.add_argument("--summary-out", default=str(SUMMARY_CSV))
    parser.add_argument("--timeout", type=float, default=600.0,
                        help="Per-run timeout in seconds")
    parser.add_argument("--hardware-note", default="",
                        help="Human-readable machine description for the report")
    args = parser.parse_args()

    executables = resolve_executables(args.executables)
    datasets = resolve_datasets(args.datasets)
    machine = host_metadata(args.hardware_note)

    out_path = Path(args.out)
    summary_path = Path(args.summary_out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "executable",
        "implementation_type",
        "dataset",
        "n",
        "d",
        "dataset_source",
        "has_labels",
        "linkage",
        "threads",
        "block_size",
        "rep",
        "runtime_ms",
        "error",
        "timestamp",
        "host",
        "platform",
        "cpu",
        "hardware_note",
        "command",
    ]

    total = 0
    for executable in executables:
        total += len(datasets) * len(run_parameters(
            executable, args.threads, args.block_sizes
        )) * args.reps

    rows = []
    done = 0
    with out_path.open("w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        for executable in executables:
            implementation_type = "cuda" if is_cuda_executable(executable) else "cpu"
            for dataset in datasets:
                for params in run_parameters(executable, args.threads,
                                             args.block_sizes):
                    for rep in range(args.reps):
                        done += 1
                        threads = params["threads"]
                        block_size = params["block_size"]
                        label = (
                            f"block={block_size}"
                            if implementation_type == "cuda"
                            else f"threads={threads}"
                        )
                        print(
                            f"[{done}/{total}] {executable.name} on "
                            f"{dataset['name']} (N={dataset['n']}, d={dataset['d']}, "
                            f"{label}, rep={rep + 1})... ",
                            end="",
                            flush=True,
                        )

                        cmd = command_for_run(
                            executable,
                            dataset["path"],
                            dataset["has_labels"],
                            args.linkage,
                            threads,
                            block_size,
                        )
                        runtime, err = run_one(cmd, args.timeout)
                        if err:
                            print(f"ERROR: {err}")
                        else:
                            print(f"{runtime:.2f} ms")

                        row = {
                            "executable": executable.name,
                            "implementation_type": implementation_type,
                            "dataset": dataset["name"],
                            "n": dataset["n"],
                            "d": dataset["d"],
                            "dataset_source": dataset["source"],
                            "has_labels": dataset["has_labels"],
                            "linkage": args.linkage,
                            "threads": threads,
                            "block_size": block_size,
                            "rep": rep + 1,
                            "runtime_ms": runtime if runtime is not None else "",
                            "error": err or "",
                            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
                            "host": machine["host"],
                            "platform": machine["platform"],
                            "cpu": machine["cpu"],
                            "hardware_note": machine["hardware_note"],
                            "command": " ".join(str(part) for part in cmd),
                        }
                        writer.writerow(row)
                        csvfile.flush()
                        rows.append(row)

    write_summary(rows, summary_path)
    print(f"\nWrote raw runtimes: {out_path}")
    print(f"Wrote median summary: {summary_path}")


if __name__ == "__main__":
    main()
