#!/usr/bin/env python3
"""
Plot benchmark results from circuit-c-backend.

Reads the JSON output from run.py and generates gate count, depth, and timing
plots for each arithmetic operation.

Usage:
    python benchmarks/plot.py [--input results.json] [--output-dir plots/]

Requirements:
    pip install matplotlib numpy
"""

import argparse
import json
import os
import sys

try:
    import matplotlib
    matplotlib.use("Agg")  # Non-interactive backend for CI/headless
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    print("ERROR: matplotlib and numpy are required. Install with:",
          file=sys.stderr)
    print("  pip install matplotlib numpy", file=sys.stderr)
    sys.exit(1)


# Publication-quality style
plt.rcParams.update({
    "font.family": "serif",
    "font.size": 11,
    "mathtext.fontset": "stix",
    "axes.grid": True,
    "grid.alpha": 0.3,
})

# Colors and markers per operation
STYLE = {
    "qft_add":      {"color": "#1f77b4", "marker": "o", "label": "QFT Addition"},
    "toffoli_cdkm": {"color": "#ff7f0e", "marker": "s", "label": "Toffoli CDKM"},
    "toffoli_cla":  {"color": "#2ca02c", "marker": "^", "label": "Toffoli CLA"},
    "qft_mul":      {"color": "#d62728", "marker": "D", "label": "QFT Multiplication"},
    "toffoli_mul":  {"color": "#9467bd", "marker": "v", "label": "Toffoli Multiplication"},
}


def load_results(path: str) -> list:
    """Load benchmark results JSON file."""
    with open(path) as f:
        data = json.load(f)
    return [r for r in data["benchmarks"] if "error" not in r]


def group_by_operation(results: list) -> dict:
    """Group results by operation name, sorted by width."""
    groups = {}
    for r in results:
        op = r["operation"]
        groups.setdefault(op, []).append(r)
    for op in groups:
        groups[op].sort(key=lambda r: r["width"])
    return groups


def plot_gate_counts(groups: dict, output_dir: str):
    """Plot total gate count vs width for all operations."""
    fig, ax = plt.subplots(figsize=(8, 5))

    for op, entries in sorted(groups.items()):
        style = STYLE.get(op, {"color": "gray", "marker": "x", "label": op})
        widths = [e["width"] for e in entries]
        gates = [e["total_gates"] for e in entries]
        ax.plot(widths, gates, marker=style["marker"], color=style["color"],
                label=style["label"], linewidth=1.5, markersize=5)

    ax.set_xlabel("Register width (qubits)")
    ax.set_ylabel("Total gate count")
    ax.set_title("Gate Count Scaling by Operation")
    ax.set_yscale("log")
    ax.legend(loc="upper left")
    fig.tight_layout()
    fig.savefig(os.path.join(output_dir, "gate_counts.png"), dpi=150)
    fig.savefig(os.path.join(output_dir, "gate_counts.pdf"))
    plt.close(fig)
    print(f"  -> gate_counts.png / .pdf")


def plot_depth(groups: dict, output_dir: str):
    """Plot circuit depth vs width for all operations."""
    fig, ax = plt.subplots(figsize=(8, 5))

    for op, entries in sorted(groups.items()):
        style = STYLE.get(op, {"color": "gray", "marker": "x", "label": op})
        widths = [e["width"] for e in entries]
        depths = [e["depth"] for e in entries]
        ax.plot(widths, depths, marker=style["marker"], color=style["color"],
                label=style["label"], linewidth=1.5, markersize=5)

    ax.set_xlabel("Register width (qubits)")
    ax.set_ylabel("Circuit depth (layers)")
    ax.set_title("Circuit Depth Scaling by Operation")
    ax.set_yscale("log")
    ax.legend(loc="upper left")
    fig.tight_layout()
    fig.savefig(os.path.join(output_dir, "circuit_depth.png"), dpi=150)
    fig.savefig(os.path.join(output_dir, "circuit_depth.pdf"))
    plt.close(fig)
    print(f"  -> circuit_depth.png / .pdf")


def plot_timing(groups: dict, output_dir: str):
    """Plot wall-clock time vs width for all operations."""
    fig, ax = plt.subplots(figsize=(8, 5))
    has_data = False

    for op, entries in sorted(groups.items()):
        style = STYLE.get(op, {"color": "gray", "marker": "x", "label": op})
        timed = [e for e in entries if e.get("wall_time_s", 0) > 0]
        if not timed:
            continue
        has_data = True
        widths = [e["width"] for e in timed]
        times = [e["wall_time_s"] for e in timed]
        ax.plot(widths, times, marker=style["marker"], color=style["color"],
                label=style["label"], linewidth=1.5, markersize=5)

    if not has_data:
        plt.close(fig)
        print("  -> timing: no data, skipping")
        return

    ax.set_xlabel("Register width (qubits)")
    ax.set_ylabel("Wall-clock time (seconds)")
    ax.set_title("Generation Time by Operation")
    ax.set_yscale("log")
    ax.legend(loc="upper left")
    fig.tight_layout()
    fig.savefig(os.path.join(output_dir, "timing.png"), dpi=150)
    fig.savefig(os.path.join(output_dir, "timing.pdf"))
    plt.close(fig)
    print(f"  -> timing.png / .pdf")


def main():
    parser = argparse.ArgumentParser(description="Plot circuit-c-backend benchmarks")
    parser.add_argument(
        "--input", "-i", default="benchmarks/results.json",
        help="Input JSON results file (default: benchmarks/results.json)",
    )
    parser.add_argument(
        "--output-dir", "-o", default="benchmarks/plots",
        help="Output directory for plots (default: benchmarks/plots/)",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"ERROR: results file not found: {args.input}", file=sys.stderr)
        print("Run benchmarks first: python benchmarks/run.py", file=sys.stderr)
        sys.exit(1)

    os.makedirs(args.output_dir, exist_ok=True)
    results = load_results(args.input)
    if not results:
        print("ERROR: no valid results found in input file", file=sys.stderr)
        sys.exit(1)

    groups = group_by_operation(results)
    print(f"Loaded {len(results)} results for {len(groups)} operations")

    plot_gate_counts(groups, args.output_dir)
    plot_depth(groups, args.output_dir)
    plot_timing(groups, args.output_dir)

    print(f"\nAll plots saved to {args.output_dir}/")


if __name__ == "__main__":
    main()
