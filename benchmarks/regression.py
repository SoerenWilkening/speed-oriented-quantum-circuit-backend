#!/usr/bin/env python3
"""
Benchmark regression suite for circuit-c-backend (libquantum).

Runs C backend benchmarks at representative widths, compares results against
a stored baseline, and flags any performance regressions exceeding a threshold.

Can be run as a standalone script or via pytest.

Usage (standalone):
    python benchmarks/regression.py [--baseline benchmarks/baseline.json]
                                    [--build-dir build]
                                    [--threshold 10]
                                    [--update-baseline]

Usage (pytest):
    pytest benchmarks/regression.py -v

Issue: refactor-05l (Phase 8, Module 8.5)
"""

import argparse
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BASELINE = REPO_ROOT / "benchmarks" / "baseline.json"
DEFAULT_BUILD_DIR = REPO_ROOT / "build"
REGRESSION_THRESHOLD_PCT = 10.0  # flag if > 10% slower

# Operations and representative widths for regression testing.
# Kept small for fast CI runs (< 30 seconds total).
REGRESSION_BENCHMARKS = {
    "qft_add":      [4, 8, 16, 32],
    "toffoli_cdkm": [4, 8, 16, 32],
    "toffoli_cla":  [4, 8, 16, 32],
    "qft_mul":      [4, 8, 16],
    "toffoli_mul":  [4, 8, 16],
}

# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class BenchmarkResult:
    """Single benchmark measurement."""
    operation: str
    width: int
    total_gates: int = 0
    depth: int = 0
    circuit_width: int = 0
    peak_qubits: int = 0
    elapsed_ms: float = 0.0
    wall_time_s: float = 0.0
    error: Optional[str] = None


@dataclass
class ComparisonEntry:
    """Comparison of one benchmark point against baseline."""
    operation: str
    width: int
    # Gate counts (deterministic -- exact match expected)
    baseline_gates: int = 0
    current_gates: int = 0
    gates_match: bool = True
    # Timing (non-deterministic -- threshold comparison)
    baseline_ms: float = 0.0
    current_ms: float = 0.0
    timing_change_pct: float = 0.0
    timing_verdict: str = "PASS"  # PASS, REGRESSION, IMPROVED, SKIP


@dataclass
class RegressionReport:
    """Full regression report."""
    overall_verdict: str = "PASS"
    threshold_pct: float = REGRESSION_THRESHOLD_PCT
    total_benchmarks: int = 0
    regressions: int = 0
    improvements: int = 0
    gate_mismatches: int = 0
    entries: list = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "overall_verdict": self.overall_verdict,
            "threshold_pct": self.threshold_pct,
            "total_benchmarks": self.total_benchmarks,
            "regressions": self.regressions,
            "improvements": self.improvements,
            "gate_mismatches": self.gate_mismatches,
            "entries": [asdict(e) for e in self.entries],
        }


# ---------------------------------------------------------------------------
# Benchmark runner
# ---------------------------------------------------------------------------

def find_benchmark_binary(build_dir: Path) -> Path:
    """Locate the benchmark_runner executable."""
    candidates = [
        build_dir / "benchmark_runner",
        build_dir / "Release" / "benchmark_runner",
        build_dir / "Debug" / "benchmark_runner",
    ]
    for p in candidates:
        if p.is_file() and os.access(p, os.X_OK):
            return p
    return candidates[0]


def run_single(binary: Path, operation: str, width: int) -> BenchmarkResult:
    """Run benchmark_runner for one operation/width and parse JSON output."""
    try:
        t0 = time.perf_counter()
        result = subprocess.run(
            [str(binary), operation, str(width)],
            capture_output=True, text=True, timeout=60,
        )
        wall = time.perf_counter() - t0
    except subprocess.TimeoutExpired:
        return BenchmarkResult(operation=operation, width=width, error="timeout")
    except FileNotFoundError:
        return BenchmarkResult(operation=operation, width=width,
                               error=f"binary not found: {binary}")

    if result.returncode != 0:
        err = result.stderr.strip() or f"exit code {result.returncode}"
        return BenchmarkResult(operation=operation, width=width, error=err)

    try:
        data = json.loads(result.stdout)
    except json.JSONDecodeError:
        return BenchmarkResult(operation=operation, width=width,
                               error="bad JSON output")

    return BenchmarkResult(
        operation=data.get("operation", operation),
        width=data.get("width", width),
        total_gates=data.get("total_gates", 0),
        depth=data.get("depth", 0),
        circuit_width=data.get("circuit_width", 0),
        peak_qubits=data.get("peak_qubits", 0),
        elapsed_ms=data.get("elapsed_ms", 0.0),
        wall_time_s=wall,
    )


def run_all_benchmarks(build_dir: Path,
                       benchmarks: Optional[dict] = None) -> list:
    """Run all regression benchmarks and return results."""
    binary = find_benchmark_binary(build_dir)
    if not binary.is_file():
        print(f"ERROR: benchmark_runner not found at {binary}", file=sys.stderr)
        print("Build with: cmake --build build --target benchmark_runner",
              file=sys.stderr)
        sys.exit(1)

    benchmarks = benchmarks or REGRESSION_BENCHMARKS
    results = []
    for op, widths in benchmarks.items():
        for w in widths:
            r = run_single(binary, op, w)
            results.append(r)
    return results


# ---------------------------------------------------------------------------
# Baseline management
# ---------------------------------------------------------------------------

def save_baseline(results: list, path: Path) -> None:
    """Save benchmark results as the new baseline."""
    data = {}
    for r in results:
        if r.error:
            continue
        key = f"{r.operation}:{r.width}"
        data[key] = {
            "operation": r.operation,
            "width": r.width,
            "total_gates": r.total_gates,
            "depth": r.depth,
            "elapsed_ms": r.elapsed_ms,
        }
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
    print(f"Baseline saved to {path} ({len(data)} entries)")


def load_baseline(path: Path) -> dict:
    """Load baseline data. Returns empty dict if file missing."""
    if not path.is_file():
        return {}
    with open(path) as f:
        return json.load(f)


# ---------------------------------------------------------------------------
# Comparison logic
# ---------------------------------------------------------------------------

def compare(results: list, baseline: dict,
            threshold_pct: float = REGRESSION_THRESHOLD_PCT) -> RegressionReport:
    """Compare current results against baseline, produce report."""
    report = RegressionReport(threshold_pct=threshold_pct)

    for r in results:
        if r.error:
            entry = ComparisonEntry(
                operation=r.operation, width=r.width,
                timing_verdict=f"ERROR: {r.error}",
            )
            report.entries.append(entry)
            report.total_benchmarks += 1
            continue

        key = f"{r.operation}:{r.width}"
        base = baseline.get(key)

        entry = ComparisonEntry(
            operation=r.operation,
            width=r.width,
            current_gates=r.total_gates,
            current_ms=r.elapsed_ms,
        )

        if base is None:
            entry.timing_verdict = "NEW"
            report.entries.append(entry)
            report.total_benchmarks += 1
            continue

        entry.baseline_gates = base.get("total_gates", 0)
        entry.baseline_ms = base.get("elapsed_ms", 0.0)

        # Gate count check (deterministic -- must match exactly)
        entry.gates_match = (entry.current_gates == entry.baseline_gates)
        if not entry.gates_match:
            report.gate_mismatches += 1

        # Timing comparison (use elapsed_ms from C timer, not wall time)
        if entry.baseline_ms > 0:
            change = ((entry.current_ms - entry.baseline_ms)
                      / entry.baseline_ms * 100.0)
            entry.timing_change_pct = round(change, 1)

            if change > threshold_pct:
                entry.timing_verdict = "REGRESSION"
                report.regressions += 1
            elif change < -threshold_pct:
                entry.timing_verdict = "IMPROVED"
                report.improvements += 1
            else:
                entry.timing_verdict = "PASS"
        else:
            entry.timing_verdict = "SKIP"

        report.entries.append(entry)
        report.total_benchmarks += 1

    if report.regressions > 0 or report.gate_mismatches > 0:
        report.overall_verdict = "FAIL"
    else:
        report.overall_verdict = "PASS"

    return report


# ---------------------------------------------------------------------------
# Report formatting
# ---------------------------------------------------------------------------

def format_report(report: RegressionReport) -> str:
    """Format the regression report as a human-readable string."""
    lines = []
    lines.append("=" * 78)
    lines.append("BENCHMARK REGRESSION REPORT")
    lines.append("=" * 78)
    lines.append(f"Overall verdict:   {report.overall_verdict}")
    lines.append(f"Threshold:         {report.threshold_pct}%")
    lines.append(f"Total benchmarks:  {report.total_benchmarks}")
    lines.append(f"Regressions:       {report.regressions}")
    lines.append(f"Improvements:      {report.improvements}")
    lines.append(f"Gate mismatches:   {report.gate_mismatches}")
    lines.append("-" * 78)

    # Table header
    hdr = (f"{'Operation':<16} {'Width':>5}  "
           f"{'Base Gates':>10} {'Curr Gates':>10} {'Gates':>5}  "
           f"{'Base ms':>9} {'Curr ms':>9} {'Change':>8} {'Verdict':<12}")
    lines.append(hdr)
    lines.append("-" * 78)

    for e in report.entries:
        gates_ok = "OK" if e.gates_match else "FAIL"
        change_str = (f"{e.timing_change_pct:+.1f}%"
                      if e.timing_verdict not in ("NEW", "SKIP", "ERROR")
                      else "---")
        base_g = str(e.baseline_gates) if e.baseline_gates else "---"
        base_m = f"{e.baseline_ms:.3f}" if e.baseline_ms else "---"
        curr_m = f"{e.current_ms:.3f}" if e.current_ms else "---"

        line = (f"{e.operation:<16} {e.width:>5}  "
                f"{base_g:>10} {e.current_gates:>10} {gates_ok:>5}  "
                f"{base_m:>9} {curr_m:>9} {change_str:>8} "
                f"{e.timing_verdict:<12}")
        lines.append(line)

    lines.append("=" * 78)
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Pytest integration
# ---------------------------------------------------------------------------

def test_no_gate_count_regressions():
    """Pytest: verify gate counts match baseline exactly."""
    build_dir = Path(os.environ.get("BUILD_DIR", DEFAULT_BUILD_DIR))
    baseline_path = Path(os.environ.get("BASELINE_PATH", DEFAULT_BASELINE))

    baseline = load_baseline(baseline_path)
    if not baseline:
        import pytest
        pytest.skip("No baseline file found; run with --update-baseline first")

    results = run_all_benchmarks(build_dir)
    report = compare(results, baseline)

    if report.gate_mismatches > 0:
        mismatches = [e for e in report.entries if not e.gates_match]
        details = "\n".join(
            f"  {e.operation} w={e.width}: "
            f"expected {e.baseline_gates}, got {e.current_gates}"
            for e in mismatches
        )
        raise AssertionError(
            f"Gate count mismatches ({report.gate_mismatches}):\n{details}"
        )


def test_no_timing_regressions():
    """Pytest: verify no timing regressions exceed threshold."""
    build_dir = Path(os.environ.get("BUILD_DIR", DEFAULT_BUILD_DIR))
    baseline_path = Path(os.environ.get("BASELINE_PATH", DEFAULT_BASELINE))

    baseline = load_baseline(baseline_path)
    if not baseline:
        import pytest
        pytest.skip("No baseline file found; run with --update-baseline first")

    results = run_all_benchmarks(build_dir)
    report = compare(results, baseline)

    if report.regressions > 0:
        regressions = [e for e in report.entries
                       if e.timing_verdict == "REGRESSION"]
        details = "\n".join(
            f"  {e.operation} w={e.width}: "
            f"{e.baseline_ms:.3f}ms -> {e.current_ms:.3f}ms "
            f"({e.timing_change_pct:+.1f}%)"
            for e in regressions
        )
        raise AssertionError(
            f"Timing regressions > {report.threshold_pct}% "
            f"({report.regressions}):\n{details}"
        )


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Benchmark regression suite for circuit-c-backend"
    )
    parser.add_argument(
        "--baseline", "-b", default=str(DEFAULT_BASELINE),
        help="Path to baseline JSON file",
    )
    parser.add_argument(
        "--build-dir", default=str(DEFAULT_BUILD_DIR),
        help="CMake build directory",
    )
    parser.add_argument(
        "--threshold", "-t", type=float, default=REGRESSION_THRESHOLD_PCT,
        help=f"Regression threshold percentage (default: {REGRESSION_THRESHOLD_PCT})",
    )
    parser.add_argument(
        "--update-baseline", action="store_true",
        help="Save current results as the new baseline (no comparison)",
    )
    parser.add_argument(
        "--json", action="store_true",
        help="Output report as JSON instead of text",
    )
    parser.add_argument(
        "--output", "-o", default=None,
        help="Write report to file (default: stdout)",
    )
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    baseline_path = Path(args.baseline)

    print("Running regression benchmarks...")
    results = run_all_benchmarks(build_dir)

    errors = [r for r in results if r.error]
    if errors:
        print(f"\nWARNING: {len(errors)} benchmark(s) had errors:",
              file=sys.stderr)
        for r in errors:
            print(f"  {r.operation} w={r.width}: {r.error}", file=sys.stderr)

    if args.update_baseline:
        save_baseline(results, baseline_path)
        return 0

    baseline = load_baseline(baseline_path)
    if not baseline:
        print(f"\nNo baseline found at {baseline_path}")
        print("Creating initial baseline from current results...")
        save_baseline(results, baseline_path)
        print("Run again to compare against this baseline.")
        return 0

    report = compare(results, baseline, threshold_pct=args.threshold)

    if args.json:
        output = json.dumps(report.to_dict(), indent=2)
    else:
        output = format_report(report)

    if args.output:
        out_path = Path(args.output)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with open(out_path, "w") as f:
            f.write(output)
        print(f"\nReport written to {args.output}")
    else:
        print()
        print(output)

    if report.overall_verdict == "FAIL":
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
