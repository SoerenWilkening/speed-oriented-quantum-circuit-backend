"""Performance benchmarks for the circuit optimizer (PERF-02).

On-demand benchmarks measuring circuit generation time with the O(log L)
binary search implementation. NOT run in CI.

Usage:
    pytest tests/python/test_optimizer_benchmark.py -v -m benchmark --no-header

All benchmarks marked with @pytest.mark.benchmark so they are skipped by default.
"""

import statistics
import time

import pytest

import quantum_language as ql
from quantum_language._core import extract_gate_range, get_current_layer

# ---------------------------------------------------------------------------
# Benchmark infrastructure
# ---------------------------------------------------------------------------

NUM_RUNS = 5  # Per user decision: 5 runs, take median


def benchmark_circuit(name, builder_func, runs=NUM_RUNS):
    """Run a circuit builder multiple times and return timing statistics.

    Parameters
    ----------
    name : str
        Human-readable benchmark name.
    builder_func : callable
        Function that creates a circuit. Must call ``ql.circuit()`` internally.
    runs : int
        Number of repetitions.

    Returns
    -------
    dict
        Benchmark results with name, timings, gate_count, layer_count.
    """
    timings_ns = []
    gate_count = 0
    layer_count = 0

    for _ in range(runs):
        start = time.perf_counter_ns()
        builder_func()
        end = time.perf_counter_ns()
        timings_ns.append(end - start)
        gate_count = len(extract_gate_range(0, get_current_layer()))
        layer_count = get_current_layer()

    timings_ms = [t / 1e6 for t in timings_ns]
    return {
        "name": name,
        "runs": runs,
        "median_ms": statistics.median(timings_ms),
        "mean_ms": statistics.mean(timings_ms),
        "min_ms": min(timings_ms),
        "max_ms": max(timings_ms),
        "stdev_ms": statistics.stdev(timings_ms) if runs > 1 else 0.0,
        "gate_count": gate_count,
        "layer_count": layer_count,
    }


def print_result(result):
    """Print a benchmark result in a readable format."""
    print(
        f"  {result['name']:40s} | "
        f"gates={result['gate_count']:>6d} | "
        f"layers={result['layer_count']:>5d} | "
        f"median={result['median_ms']:>8.2f}ms | "
        f"min={result['min_ms']:>8.2f}ms | "
        f"max={result['max_ms']:>8.2f}ms"
    )


# ---------------------------------------------------------------------------
# Synthetic large circuit builders
# ---------------------------------------------------------------------------


def _build_synthetic(num_ops, width=4):
    """Build a synthetic circuit with repeated additions."""
    ql.circuit()
    ql.option("fault_tolerant", True)
    a = ql.qint(0, bits=width)
    for i in range(num_ops):
        a += (i % 7) + 1  # Vary the constant to avoid trivial cancellation


def _build_synthetic_100():
    _build_synthetic(100)


def _build_synthetic_1000():
    _build_synthetic(1000)


def _build_synthetic_10000():
    _build_synthetic(10000, width=8)


# ---------------------------------------------------------------------------
# Real workload builders
# ---------------------------------------------------------------------------


def _build_mult_4bit():
    ql.circuit()
    ql.option("fault_tolerant", True)
    a = ql.qint(3, bits=4)
    b = ql.qint(2, bits=4)
    _ = a * b


def _build_qft_add_8bit():
    ql.circuit()
    ql.option("fault_tolerant", False)
    a = ql.qint(100, bits=8)
    b = ql.qint(50, bits=8)
    _ = a + b


def _build_repeated_add_4bit():
    """20 sequential additions on a 4-bit register."""
    ql.circuit()
    ql.option("fault_tolerant", True)
    a = ql.qint(0, bits=4)
    for i in range(20):
        a += (i % 5) + 1


def _build_grover_3q():
    ql.circuit()
    ql.option("fault_tolerant", True)

    @ql.compile
    def equals_five(x):
        return x == 5

    ql.grover(equals_five, width=3, iterations=1)


# ---------------------------------------------------------------------------
# Benchmark tests
# ---------------------------------------------------------------------------


@pytest.mark.benchmark
def test_synthetic_scaling():
    """Benchmark synthetic circuits at 100, 1K, and 10K operations.

    Measures end-to-end circuit generation time including optimizer.
    The 50K scale is omitted due to memory constraints in CI-like environments.
    """
    print("\n--- Synthetic Scaling Benchmarks ---")

    results = []
    for name, builder in [
        ("synthetic_100_ops", _build_synthetic_100),
        ("synthetic_1000_ops", _build_synthetic_1000),
        ("synthetic_10000_ops", _build_synthetic_10000),
    ]:
        result = benchmark_circuit(name, builder)
        print_result(result)
        results.append(result)

    # Verify scaling: 10K should take less than 100x of 100 (sub-linear with binary search)
    r100 = results[0]
    r10k = results[2]
    ratio = r10k["median_ms"] / max(r100["median_ms"], 0.001)
    print(f"\n  Scaling ratio (10K/100): {ratio:.1f}x (ideal <100x for sub-linear)")

    # Basic sanity: circuits were actually built
    assert r100["gate_count"] > 0
    assert r10k["gate_count"] > r100["gate_count"]


@pytest.mark.benchmark
def test_real_workloads():
    """Benchmark real-world circuit patterns."""
    print("\n--- Real Workload Benchmarks ---")

    for name, builder in [
        ("mult_4bit", _build_mult_4bit),
        ("qft_add_8bit", _build_qft_add_8bit),
        ("repeated_add_4bit_x20", _build_repeated_add_4bit),
        ("grover_3q", _build_grover_3q),
    ]:
        result = benchmark_circuit(name, builder)
        print_result(result)


@pytest.mark.benchmark
def test_scaling_summary():
    """Generate scaling data for documentation."""
    print("\n--- Scaling Summary ---")
    print(f"  {'Operations':>12s} | {'Gates':>8s} | {'Layers':>7s} | {'Median (ms)':>12s}")
    print(f"  {'-' * 12} | {'-' * 8} | {'-' * 7} | {'-' * 12}")

    def _make_builder(n):
        def builder():
            _build_synthetic(n)

        return builder

    for num_ops in [10, 50, 100, 500, 1000, 5000]:
        result = benchmark_circuit(f"synthetic_{num_ops}", _make_builder(num_ops), runs=3)
        print(
            f"  {num_ops:>12d} | "
            f"{result['gate_count']:>8d} | "
            f"{result['layer_count']:>7d} | "
            f"{result['median_ms']:>12.2f}"
        )
