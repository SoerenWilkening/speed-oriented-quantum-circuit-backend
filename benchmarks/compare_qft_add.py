"""Compare QFT addition generation times: C backend vs monolith results.csv.

The monolith's results.csv has two methods:
  - cq: original Python/Cython QFT addition (generate sequence + add to circuit)
  - cq_impr: improved C backend QFT addition

This script runs the C backend benchmark_runner for qft_add at the same
widths and compares wall-clock times.
"""

import csv
import json
import subprocess
import os
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
RESULTS_CSV = REPO_ROOT / "Quantum_Assembly" / "circuit-gen-results" / "results.csv"
BENCHMARK_BIN = REPO_ROOT / "circuit-c-backend" / "build" / "benchmark_runner"


def load_monolith_results():
    """Load cq and cq_impr times from results.csv."""
    cq = {}
    cq_impr = {}
    with open(RESULTS_CSV) as f:
        reader = csv.DictReader(f)
        for row in reader:
            n = int(row["n"])
            t = float(row["t"])
            meth = row["meth"]
            if meth == "cq":
                cq[n] = t
            elif meth == "cq_impr":
                cq_impr[n] = t
    return cq, cq_impr


def run_c_benchmark(width):
    """Run the C benchmark_runner for qft_add at given width, return time in seconds."""
    result = subprocess.run(
        [str(BENCHMARK_BIN), "qft_add", str(width)],
        capture_output=True, text=True, timeout=60,
    )
    if result.returncode != 0:
        print(f"  ERROR at width {width}: {result.stderr.strip()}", file=sys.stderr)
        return None
    data = json.loads(result.stdout)
    return data["elapsed_ms"] / 1000.0  # convert ms to seconds


def main():
    if not RESULTS_CSV.exists():
        sys.exit(f"results.csv not found at {RESULTS_CSV}")
    if not BENCHMARK_BIN.exists():
        sys.exit(f"benchmark_runner not found at {BENCHMARK_BIN}")

    cq, cq_impr = load_monolith_results()
    widths = sorted(cq.keys())

    print(f"{'Width':>6}  {'cq (monolith)':>14}  {'cq_impr (mono)':>14}  {'C backend now':>14}  {'vs cq':>10}  {'vs cq_impr':>12}")
    print("-" * 82)

    for w in widths:
        t_now = run_c_benchmark(w)
        if t_now is None:
            continue

        t_cq = cq[w]
        t_impr = cq_impr.get(w)

        speedup_cq = t_cq / t_now if t_now > 0 else float("inf")
        speedup_impr = (t_impr / t_now if t_now > 0 else float("inf")) if t_impr else None

        impr_str = f"{speedup_impr:>10.1f}x" if speedup_impr is not None else "N/A"

        print(f"{w:>6}  {t_cq:>14.6f}s  ", end="")
        if t_impr is not None:
            print(f"{t_impr:>14.6f}s  ", end="")
        else:
            print(f"{'N/A':>14}  ", end="")
        print(f"{t_now:>14.6f}s  {speedup_cq:>8.1f}x  {impr_str}")


if __name__ == "__main__":
    main()
