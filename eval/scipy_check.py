"""
Validates the C++ dendrogram output against scipy's reference implementation.

Usage:
    python eval/scipy_check.py <dataset.csv> <executable>

Example:
    python eval/scipy_check.py tests/hand_trace_10points.csv build/hc_mst_cpu
"""

import argparse
import csv
import subprocess
import sys
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.cluster.hierarchy import linkage


def load_points(csv_path):
    """Load points from CSV. If a 'label' column exists, it is excluded."""
    df = pd.read_csv(csv_path)
    if "label" in df.columns:
        df = df.drop(columns=["label"])
    return df.values


def run_cpp(executable, csv_path, threads=1):
    df_header = pd.read_csv(csv_path, nrows=0)
    has_labels = "label" in df_header.columns

    cmd = [executable, csv_path]
    if threads > 1:
        cmd += ["--threads", str(threads)]
    if has_labels:
        cmd += ["--has-labels"]

    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    from io import StringIO
    return pd.read_csv(StringIO(result.stdout))


def scipy_dendrogram(points):
    """Run scipy single-link and return as a comparable DataFrame."""
    # scipy's linkage matrix: each row is [cluster_a, cluster_b, distance, new_size]
    Z = linkage(points, method='single')
    df = pd.DataFrame(Z, columns=['cluster_a', 'cluster_b', 'distance', 'new_size'])
    df['cluster_a'] = df['cluster_a'].astype(int)
    df['cluster_b'] = df['cluster_b'].astype(int)
    df['new_size'] = df['new_size'].astype(int)
    return df


def compare_dendrograms(cpp_df, scipy_df, tol=1e-6):
    """Compare two dendrograms by their sorted distance multisets."""
    if len(cpp_df) != len(scipy_df):
        return False, f"Different number of merges: {len(cpp_df)} vs {len(scipy_df)}"

    cpp_dists = sorted(cpp_df['distance'].tolist())
    scipy_dists = sorted(scipy_df['distance'].tolist())

    for d1, d2 in zip(cpp_dists, scipy_dists):
        if abs(d1 - d2) > tol:
            return False, f"Distance mismatch: {d1} vs {d2}"

    return True, "OK"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", help="Path to dataset CSV")
    parser.add_argument("executable", help="Path to C++ executable")
    parser.add_argument("--tol", type=float, default=1e-6,
                        help="Numerical tolerance for distance comparison")
    args = parser.parse_args()

    print(f"Dataset: {args.csv_path}")
    points = load_points(args.csv_path)
    print(f"  N = {len(points)}, d = {points.shape[1]}")

    print("Running C++ binary...")
    cpp_df = run_cpp(args.executable, args.csv_path)

    print("Running scipy...")
    scipy_df = scipy_dendrogram(points)

    print("Comparing...")
    ok, msg = compare_dendrograms(cpp_df, scipy_df, args.tol)
    if ok:
        print(f"  ✓ Match. {len(cpp_df)} merges agree.")
    else:
        print(f"  ✗ MISMATCH: {msg}")
        print("\nC++ dendrogram (first 10 rows):")
        print(cpp_df.head(10))
        print("\nscipy dendrogram (first 10 rows):")
        print(scipy_df.head(10))
        sys.exit(1)


if __name__ == "__main__":
    main()