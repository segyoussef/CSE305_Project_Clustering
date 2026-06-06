"""
Convert the UCI Optical Digit Recognition dataset to the team's CSV format.

The original dataset is split into training (3823 rows) and test (1797 rows)
files, both with 64 features + 1 class label per row, no header.
We concatenate them since we're using the data for clustering, not classification.

Run from the repo root:
    python3 eval/convert_optdigits.py
"""

import pandas as pd
from pathlib import Path

SRC_DIR = Path("eval/datasets/uci_raw")
OUT = Path("eval/datasets/optdigits.csv")

dfs = []
for fname in ["optdigits.tra", "optdigits.tes"]:
    df = pd.read_csv(SRC_DIR / fname, header=None)
    dfs.append(df)

df = pd.concat(dfs, ignore_index=True)

# 64 pixel-intensity features + 1 label column
n_features = df.shape[1] - 1
df.columns = [f"x{i}" for i in range(n_features)] + ["label"]

# Label is already integer 0-9
df.to_csv(OUT, index=False)
print(f"Wrote {OUT} with N={len(df)}, d={n_features}")