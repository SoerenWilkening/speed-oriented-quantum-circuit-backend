#!/usr/bin/env python3
"""
Benchmark runner for circuit-c-backend (libquantum).

Runs the compiled benchmark_runner executable for various arithmetic operations
and widths, collecting gate counts, circuit depth, and wall-clock time.
Results are written to a JSON file for plotting.

Usage:
    python benchmarks/run.py [--output results.json] [--build-dir build]
"""

import argparse
import json
import os
import subprocess
import sys
import time


def find_benchmark_binary(build_dir: str) -> str:
    """Locate the benchmark_runner executable in the build directory."""
    candidates = [
        os.path.join(build_dir, "benchmark_runner"),
        os.path.join(build_dir, "Release", "benchmark_runner"),
        os.path.join(build_dir, "Debug", "benchmark_runner"),
    ]
    for path in candidates:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return candidates[0]  # fallback; will fail with clear error


def run_single_benchmark(binary: str, operation: str, width: int) -> dict:
    """Run benchmark_runner for one operation/width and parse JSON output."""
    try:
        t0 = time.perf_counter()
        result = subprocess.run(
            [binary, operation, str(width)],
            capture_output=True,
            text=True,
            timeout=120,
        )
        elapsed = time.perf_counter() - t0
    except subprocess.TimeoutExpired:
        return {"operation": operation, "width": width, "error": "timeout"}
    except FileNotFoundError:
        print(f"ERROR: benchmark_runner not found at {binary}", file=sys.stderr)
        print("Build with: cmake --build build --target benchmark_runner",
              file=sys.stderr)
        sys.exit(1)

    if result.returncode != 0:
        return {
            "operation": operation,
            "width": width,
            "error": result.stderr.strip() or f"exit code {result.returncode}",
        }

    try:
        data = json.loads(result.stdout)
    except json.JSONDecodeError:
        return {"operation": operation, "width": width, "error": "bad JSON output"}

    data["wall_time_s"] = elapsed
    return data


# Operation name -> (max_width, description)
BENCHMARKS = {
    "qft_add":         (64, "QFT addition (Draper adder)"),
    "toffoli_cdkm":    (64, "Toffoli CDKM ripple-carry addition"),
    "toffoli_cla":     (64, "Toffoli Brent-Kung CLA addition"),
    "qft_mul":         (32, "QFT multiplication"),
    "toffoli_mul":     (32, "Toffoli multiplication"),
}


def main():
    parser = argparse.ArgumentParser(description="Run circuit-c-backend benchmarks")
    parser.add_argument(
        "--output", "-o", default="benchmarks/results.json",
        help="Output JSON file (default: benchmarks/results.json)",
    )
    parser.add_argument(
        "--build-dir", "-b", default="build",
        help="CMake build directory (default: build)",
    )
    parser.add_argument(
        "--operations", "-ops", nargs="*", default=None,
        help="Operations to benchmark (default: all)",
    )
    parser.add_argument(
        "--max-width", "-w", type=int, default=None,
        help="Override maximum width for all operations",
    )
    parser.add_argument(
        "--ci", action="store_true",
        help="CI mode: smaller widths, no timing, deterministic output",
    )
    args = parser.parse_args()

    binary = find_benchmark_binary(args.build_dir)
    ops = args.operations or list(BENCHMARKS.keys())
    results = []

    for op in ops:
        if op not in BENCHMARKS:
            print(f"WARNING: unknown operation '{op}', skipping", file=sys.stderr)
            continue

        default_max, desc = BENCHMARKS[op]
        max_w = args.max_width or default_max

        # In CI mode, use smaller widths to keep runtime short
        if args.ci:
            max_w = min(max_w, 16)

        # Generate width sequence: 1..min(16, max_w) then powers of 2
        widths = list(range(1, min(17, max_w + 1)))
        w = 32
        while w <= max_w:
            if w not in widths:
                widths.append(w)
            w *= 2

        # CLA requires width >= 2
        if op == "toffoli_cla":
            widths = [w for w in widths if w >= 2]

        print(f"--- {desc} (widths 1-{max_w}) ---")
        for width in widths:
            entry = run_single_benchmark(binary, op, width)
            results.append(entry)
            if "error" in entry:
                print(f"  width={width:3d}  ERROR: {entry['error']}")
            else:
                gc = entry.get("total_gates", "?")
                depth = entry.get("depth", "?")
                wt = entry.get("wall_time_s", 0)
                print(f"  width={width:3d}  gates={gc:>10}  depth={depth:>8}"
                      f"  time={wt:.4f}s")

    # Write results
    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    with open(args.output, "w") as f:
        json.dump({"benchmarks": results}, f, indent=2)
    print(f"\nResults written to {args.output}")


if __name__ == "__main__":
    main()
