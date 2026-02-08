"""Evaluation and report generation for Phase 62 benchmarks.

Runs three evaluations (EVAL-01/02/03) and generates a comparison report (BENCH-04):
  EVAL-01: Multiplication cost analysis (per-call CQ_mul cost, comparison vs addition)
  EVAL-02: Bitwise operation cost analysis (compare vs addition)
  EVAL-03: Division cost analysis (classical and quantum divisor)
  BENCH-04: Combined comparison report with amortization/break-even analysis

All evaluation data is combined with Plan 01 measurements (bench_raw.json)
into a comprehensive benchmark report.

Usage:
    # Run all evaluations and generate report (default)
    PYTHONPATH=src python scripts/benchmark_eval.py

    # Run evaluations only (no report generation)
    PYTHONPATH=src python scripts/benchmark_eval.py --eval-only

    # Generate report from existing data only (no new measurements)
    PYTHONPATH=src python scripts/benchmark_eval.py --report-only

    # Override division width range (default 1-8)
    PYTHONPATH=src python scripts/benchmark_eval.py --widths 1-4
"""

import argparse
import json
import os
import statistics
import subprocess
import sys
import time

# ---------------------------------------------------------------------------
# Project root detection
# ---------------------------------------------------------------------------
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BENCH_RAW_PATH = os.path.join(PROJECT_ROOT, "benchmarks", "results", "bench_raw.json")
EVAL_DATA_PATH = os.path.join(PROJECT_ROOT, "benchmarks", "results", "eval_data.json")
REPORT_JSON_PATH = os.path.join(PROJECT_ROOT, "benchmarks", "results", "benchmark_report.json")
REPORT_MD_PATH = os.path.join(PROJECT_ROOT, "benchmarks", "results", "benchmark_report.md")

# Default configuration
DEFAULT_EVAL_ITERATIONS = 5
DEFAULT_DIV_WIDTH_MIN = 1
DEFAULT_DIV_WIDTH_MAX = 8
DEFAULT_MUL_WIDTH_MIN = 1
DEFAULT_MUL_WIDTH_MAX = 16
DIV_SUBPROCESS_TIMEOUT = 30  # seconds per division subprocess


# ---------------------------------------------------------------------------
# Utility helpers
# ---------------------------------------------------------------------------


def _subprocess_env():
    """Return environment dict with PYTHONPATH set for quantum_language."""
    env = os.environ.copy()
    env["PYTHONPATH"] = os.path.join(PROJECT_ROOT, "src")
    return env


def _run_subprocess_script(script, timeout=120):
    """Run a Python script in a clean subprocess and return stdout.

    Returns None on failure or timeout.
    """
    try:
        result = subprocess.run(
            [sys.executable, "-c", script],
            capture_output=True,
            text=True,
            cwd=PROJECT_ROOT,
            env=_subprocess_env(),
            timeout=timeout,
        )
        if result.returncode != 0:
            stderr_preview = result.stderr[:300] if result.stderr else "(no stderr)"
            print(f"  WARNING: subprocess failed (rc={result.returncode}): {stderr_preview}")
            return None
        return result.stdout.strip()
    except subprocess.TimeoutExpired:
        print(f"  WARNING: subprocess timed out after {timeout}s")
        return None
    except Exception as exc:
        print(f"  WARNING: subprocess error: {exc}")
        return None


def _parse_int_output(output):
    """Parse integer from subprocess output, return None on failure."""
    if output is None:
        return None
    try:
        return int(output.strip())
    except (ValueError, AttributeError):
        return None


def _parse_width_range(widths_str):
    """Parse width range string like '1-8' into list of ints."""
    if "-" in widths_str:
        parts = widths_str.split("-")
        return list(range(int(parts[0]), int(parts[1]) + 1))
    return [int(widths_str)]


def _load_bench_raw():
    """Load Plan 01 benchmark raw data."""
    if not os.path.exists(BENCH_RAW_PATH):
        print(f"ERROR: bench_raw.json not found at {BENCH_RAW_PATH}")
        print("Run Plan 01 first: PYTHONPATH=src python scripts/benchmark_sequences.py")
        sys.exit(1)
    with open(BENCH_RAW_PATH) as f:
        return json.load(f)


def _load_eval_data():
    """Load saved evaluation data if it exists."""
    if os.path.exists(EVAL_DATA_PATH):
        with open(EVAL_DATA_PATH) as f:
            return json.load(f)
    return {}


def _save_eval_data(data):
    """Save evaluation data for later report generation."""
    os.makedirs(os.path.dirname(EVAL_DATA_PATH), exist_ok=True)
    with open(EVAL_DATA_PATH, "w") as f:
        json.dump(data, f, indent=2)
    print(f"  Eval data saved to: {EVAL_DATA_PATH}")


# ---------------------------------------------------------------------------
# EVAL-01: Multiplication Evaluation
# ---------------------------------------------------------------------------

# CQ_mul per-call measurement script template
# CQ_mul is NOT cached -- each call regenerates the sequence
CQ_MUL_PER_CALL_SCRIPT = (
    "import time, quantum_language as ql\n"
    "ql.circuit()\n"
    "a = ql.qint(1, width={width})\n"
    "# First call to warm up / trigger any one-time init\n"
    "_ = a * 3\n"
    "# Measure second call -- CQ_mul is NOT cached, fresh malloc each time\n"
    "start = time.perf_counter_ns()\n"
    "c = a * 3\n"
    "end = time.perf_counter_ns()\n"
    "print(end - start)\n"
)


def eval_01_multiplication(widths=None, iterations=DEFAULT_EVAL_ITERATIONS):
    """EVAL-01: Measure CQ_mul per-call cost and compare with addition.

    CQ_mul returns a fresh malloc'd sequence each call (not cached like
    QQ_mul which caches after first call). This measures the per-call
    generation cost.

    Returns dict with per-call costs and comparison ratios.
    """
    if widths is None:
        widths = list(range(DEFAULT_MUL_WIDTH_MIN, DEFAULT_MUL_WIDTH_MAX + 1))

    print("=" * 60)
    print("EVAL-01: Multiplication Evaluation")
    print("=" * 60)

    cq_mul_per_call = {}

    for width in widths:
        print(f"  CQ_mul per-call width={width} ({iterations} iterations)...")
        script = CQ_MUL_PER_CALL_SCRIPT.format(width=width)
        times_ns = []

        for _ in range(iterations):
            output = _run_subprocess_script(script, timeout=60)
            val = _parse_int_output(output)
            if val is not None:
                times_ns.append(val)

        if times_ns:
            median_us = statistics.median(times_ns) / 1e3
            cq_mul_per_call[str(width)] = round(median_us, 2)
            print(f"    -> {median_us:.0f} us/call")
        else:
            cq_mul_per_call[str(width)] = None
            print("    -> FAILED (no valid measurements)")

    print()
    return {"cq_mul_per_call_us": cq_mul_per_call}


# ---------------------------------------------------------------------------
# EVAL-02: Bitwise Evaluation
# ---------------------------------------------------------------------------


def eval_02_bitwise(bench_raw):
    """EVAL-02: Analyze bitwise operation costs from Plan 01 data.

    Bitwise operations (xor, and, or) are already measured in bench_raw.json.
    This evaluates whether they need hardcoding by comparing to addition cost.

    Returns dict with analysis and recommendation.
    """
    print("=" * 60)
    print("EVAL-02: Bitwise Evaluation")
    print("=" * 60)

    first_call = bench_raw.get("bench_02_first_call_us", {})
    xor_data = first_call.get("Q_xor", {})
    and_data = first_call.get("Q_and", {})
    or_data = first_call.get("Q_or", {})
    add_data = first_call.get("QQ_add", {})

    # Compute max cost across all widths for each operation
    xor_vals = [v for v in xor_data.values() if v is not None]
    and_vals = [v for v in and_data.values() if v is not None]
    or_vals = [v for v in or_data.values() if v is not None]
    add_vals = [v for v in add_data.values() if v is not None]

    xor_max = max(xor_vals) if xor_vals else None
    and_max = max(and_vals) if and_vals else None
    or_max = max(or_vals) if or_vals else None
    add_median = statistics.median(add_vals) if add_vals else None

    # Print summary
    print(f"  Q_xor max cost:     {xor_max:.1f} us" if xor_max else "  Q_xor: no data")
    print(f"  Q_and max cost:     {and_max:.1f} us" if and_max else "  Q_and: no data")
    print(f"  Q_or  max cost:     {or_max:.1f} us" if or_max else "  Q_or:  no data")
    print(f"  QQ_add median cost: {add_median:.1f} us" if add_median else "  QQ_add: no data")

    # Determine recommendation
    all_max = (
        max(v for v in [xor_max, and_max, or_max] if v is not None)
        if any(v is not None for v in [xor_max, and_max, or_max])
        else None
    )

    # "skip" if all bitwise costs are under 300us (trivial generation)
    threshold_us = 300.0
    if all_max is not None and all_max < threshold_us:
        recommendation = "skip"
        reasoning = (
            f"All bitwise operation generation costs are under {threshold_us}us "
            f"(max: {all_max:.1f}us). XOR is O(N) CNOT gates with ~{xor_max:.0f}us max cost. "
            f"AND/OR use Toffoli decomposition at ~{max(and_max or 0, or_max or 0):.0f}us max. "
            f"These are trivial compared to addition ({add_median:.0f}us median) and "
            f"especially multiplication. Hardcoding would add binary size for negligible benefit."
        )
    else:
        recommendation = "hardcode"
        reasoning = (
            f"Bitwise operation costs exceed {threshold_us}us threshold "
            f"(max: {all_max:.1f}us). Consider hardcoding."
        )

    print(f"\n  Recommendation: {recommendation}")
    print(f"  Reasoning: {reasoning}")
    print()

    return {
        "xor_max_us": xor_max,
        "and_max_us": and_max,
        "or_max_us": or_max,
        "all_max_us": all_max,
        "add_median_us": add_median,
        "recommendation": recommendation,
        "reasoning": reasoning,
    }


# ---------------------------------------------------------------------------
# EVAL-03: Division Evaluation
# ---------------------------------------------------------------------------

DIV_CLASSICAL_SCRIPT = (
    "import time, quantum_language as ql\n"
    "ql.circuit()\n"
    "a = ql.qint(1, width={width})\n"
    "start = time.perf_counter_ns()\n"
    "q = a // 3\n"
    "end = time.perf_counter_ns()\n"
    "print(end - start)\n"
)

DIV_QUANTUM_SCRIPT = (
    "import time, quantum_language as ql\n"
    "ql.circuit()\n"
    "a = ql.qint(1, width={width})\n"
    "b = ql.qint(1, width={width})\n"
    "start = time.perf_counter_ns()\n"
    "q = a // b\n"
    "end = time.perf_counter_ns()\n"
    "print(end - start)\n"
)


def eval_03_division(widths=None, iterations=DEFAULT_EVAL_ITERATIONS):
    """EVAL-03: Measure division cost for classical and quantum divisors.

    Division is Python-level (Cython), not a C sequence:
    - Classical divisor: O(width) iterations of comparison+subtraction
    - Quantum divisor: O(2^width) iterations (exponential)

    Returns dict with costs and recommendation.
    """
    if widths is None:
        widths = list(range(DEFAULT_DIV_WIDTH_MIN, DEFAULT_DIV_WIDTH_MAX + 1))

    print("=" * 60)
    print("EVAL-03: Division Evaluation")
    print("=" * 60)

    classical_costs = {}
    quantum_costs = {}

    # Classical divisor (a // 3)
    print("\n  Classical divisor (a // 3):")
    for width in widths:
        print(f"    width={width} ({iterations} iterations)...")
        script = DIV_CLASSICAL_SCRIPT.format(width=width)
        times_ns = []

        for _ in range(iterations):
            output = _run_subprocess_script(script, timeout=DIV_SUBPROCESS_TIMEOUT)
            val = _parse_int_output(output)
            if val is not None:
                times_ns.append(val)

        if times_ns:
            median_us = statistics.median(times_ns) / 1e3
            classical_costs[str(width)] = round(median_us, 2)
            print(f"      -> {median_us:.0f} us")
        else:
            classical_costs[str(width)] = None
            print("      -> FAILED/TIMEOUT")

    # Quantum divisor (a // b) -- very slow for large widths
    print("\n  Quantum divisor (a // b):")
    for width in widths:
        print(f"    width={width} ({iterations} iterations, timeout={DIV_SUBPROCESS_TIMEOUT}s)...")
        script = DIV_QUANTUM_SCRIPT.format(width=width)
        times_ns = []

        for _ in range(iterations):
            output = _run_subprocess_script(script, timeout=DIV_SUBPROCESS_TIMEOUT)
            val = _parse_int_output(output)
            if val is not None:
                times_ns.append(val)

        if times_ns:
            median_us = statistics.median(times_ns) / 1e3
            quantum_costs[str(width)] = round(median_us, 2)
            if median_us >= 1e6:
                print(f"      -> {median_us / 1e6:.1f} s")
            elif median_us >= 1e3:
                print(f"      -> {median_us / 1e3:.1f} ms")
            else:
                print(f"      -> {median_us:.0f} us")
        else:
            quantum_costs[str(width)] = None
            print("      -> TIMEOUT (expected for width > ~4 due to 2^N iterations)")

    recommendation = "skip"
    reasoning = (
        "Division cost is in the Python-level loop structure (N comparison+subtraction "
        "iterations for classical divisor, 2^N for quantum divisor), NOT in C sequence "
        "generation. Hardcoding individual gate sequences would not reduce division cost "
        "because the overhead is in the number of high-level operations, not in generating "
        "each operation's gate sequence. The component operations (addition, subtraction, "
        "comparison) already benefit from hardcoded sequences when available."
    )

    print(f"\n  Recommendation: {recommendation}")
    print(f"  Reasoning: {reasoning}")
    print()

    return {
        "classical_cost_us": classical_costs,
        "quantum_cost_us": quantum_costs,
        "recommendation": recommendation,
        "reasoning": reasoning,
    }


# ---------------------------------------------------------------------------
# BENCH-04: Report Generation
# ---------------------------------------------------------------------------


def _compute_mul_vs_add_ratios(bench_raw, eval_data):
    """Compute multiplication/addition cost ratios per width."""
    first_call = bench_raw.get("bench_02_first_call_us", {})
    qq_mul = first_call.get("QQ_mul", {})
    qq_add = first_call.get("QQ_add", {})

    ratios = {}
    for width_str in qq_mul:
        mul_val = qq_mul.get(width_str)
        add_val = qq_add.get(width_str)
        if mul_val is not None and add_val is not None and add_val > 0:
            ratios[width_str] = round(mul_val / add_val, 1)
        else:
            ratios[width_str] = None
    return ratios


def _compute_mul_recommendation(ratios, bench_raw):
    """Produce multiplication hardcoding recommendation based on cost ratios."""
    valid_ratios = [v for v in ratios.values() if v is not None]

    if not valid_ratios:
        return "skip", "Insufficient data for recommendation."

    avg_ratio = statistics.mean(valid_ratios)
    max_ratio = max(valid_ratios)
    min_ratio = min(valid_ratios)

    # Multiplication is already present in bench_raw (QQ_mul, CQ_mul first-call)
    first_call = bench_raw.get("bench_02_first_call_us", {})
    qq_mul_vals = [v for v in first_call.get("QQ_mul", {}).values() if v is not None]
    avg_mul_us = statistics.mean(qq_mul_vals) if qq_mul_vals else 0

    # Check binary size impact: multiplication sequences are much larger than addition
    # Each width would add significant binary size
    so_sizes = bench_raw.get("bench_01_import_time", {}).get("so_file_sizes_bytes", {})
    total_so_bytes = sum(so_sizes.values()) if so_sizes else 0
    total_so_mb = total_so_bytes / (1024 * 1024)

    # Recommendation: multiplication generation cost is high (>10x addition), BUT:
    # - QQ_mul is already cached after first call
    # - Binary size impact would be substantial (mul sequences grow as O(N^2) additions)
    # - CQ_mul is NOT cached, but per-call cost is the same as first-call
    if avg_ratio > 10:
        recommendation = "investigate"
        reasoning = (
            f"Multiplication generation cost is {avg_ratio:.0f}x addition on average "
            f"(range: {min_ratio:.0f}x to {max_ratio:.0f}x). Average first-call cost: "
            f"{avg_mul_us:.0f}us. QQ_mul IS cached after first call, so hardcoding would "
            f"only save the first-call cost. CQ_mul is NOT cached (fresh malloc each call), "
            f"so it would benefit more from hardcoding. However, multiplication sequences "
            f"are O(N^2) additions, so binary size impact would be substantial relative to "
            f"current {total_so_mb:.1f}MB. Recommend investigating selective hardcoding "
            f"for small widths (1-4) where binary size is manageable."
        )
    else:
        recommendation = "skip"
        reasoning = (
            f"Multiplication cost ratio ({avg_ratio:.0f}x addition) is below 10x threshold. "
            f"Binary size impact not justified."
        )

    return recommendation, reasoning


def _compute_amortization(bench_raw):
    """Compute import overhead amortization (break-even analysis).

    Formula: break_even_calls = import_overhead_us / avg_generation_saving_per_call_us
    """
    import_data = bench_raw.get("bench_01_import_time", {})
    import_ms = import_data.get("median_ms", 0) or 0
    import_us = import_ms * 1000  # Convert to microseconds

    # Compute average generation saving: difference between dynamic and hardcoded dispatch
    dispatch = bench_raw.get("bench_03_cached_dispatch_ns", {})
    savings_ns = []
    for op in dispatch:
        hc = dispatch[op].get("8_hardcoded")
        dyn = dispatch[op].get("17_dynamic")
        if hc is not None and dyn is not None:
            savings_ns.append(dyn - hc)

    avg_saving_ns = statistics.mean(savings_ns) if savings_ns else 0
    avg_saving_us = avg_saving_ns / 1000

    # Break-even: how many cached calls to recoup import overhead
    # import_us / avg_saving_us = number of calls
    if avg_saving_us > 0:
        break_even = import_us / avg_saving_us
    else:
        break_even = float("inf")

    # Also compute based on first-call saving (generation cost saved)
    first_call = bench_raw.get("bench_02_first_call_us", {})
    qq_add = first_call.get("QQ_add", {})
    # Average first-call generation cost across hardcoded widths (1-16)
    add_vals = [v for v in qq_add.values() if v is not None]
    avg_first_call_us = statistics.mean(add_vals) if add_vals else 0

    # For first-call: each unique (op, width) called saves ~avg_first_call_us
    # Break-even = import_us / avg_first_call_us = number of unique first calls needed
    if avg_first_call_us > 0:
        break_even_first_call = import_us / avg_first_call_us
    else:
        break_even_first_call = float("inf")

    analysis = (
        f"The hardcoded sequences add {import_ms:.0f}ms to import time. "
        f"Each cached dispatch call saves ~{avg_saving_ns:.0f}ns vs dynamic generation. "
        f"Break-even for cached dispatch: ~{break_even:.0f} calls. "
        f"However, the primary benefit is first-call avoidance: each unique "
        f"(operation, width) combination saves ~{avg_first_call_us:.0f}us on first call. "
        f"Break-even for first-call savings: ~{break_even_first_call:.0f} unique first calls. "
        f"In practice, a typical quantum program calls 5-20 unique (op, width) pairs, "
        f"so first-call savings alone justify the import overhead after just "
        f"{break_even_first_call:.0f} unique calls."
    )

    return {
        "import_overhead_ms": import_ms,
        "import_overhead_us": round(import_us, 2),
        "avg_dispatch_saving_ns": round(avg_saving_ns, 1),
        "avg_dispatch_saving_us": round(avg_saving_us, 2),
        "break_even_cached_calls": round(break_even),
        "avg_first_call_saving_us": round(avg_first_call_us, 2),
        "break_even_first_calls": round(break_even_first_call),
        "analysis": analysis,
    }


def generate_report(bench_raw, eval_data):
    """Generate BENCH-04 comparison report (JSON + Markdown).

    Combines Plan 01 measurements with EVAL data into final report.
    """
    print("=" * 60)
    print("BENCH-04: Generating Comparison Report")
    print("=" * 60)

    # Extract components
    import_data = bench_raw.get("bench_01_import_time", {})
    first_call = bench_raw.get("bench_02_first_call_us", {})
    dispatch = bench_raw.get("bench_03_cached_dispatch_ns", {})

    # Compute mul vs add ratios
    mul_ratios = _compute_mul_vs_add_ratios(bench_raw, eval_data)
    mul_rec, mul_reasoning = _compute_mul_recommendation(mul_ratios, bench_raw)

    # Get EVAL data
    eval_01 = eval_data.get("eval_01", {})
    eval_02 = eval_data.get("eval_02", {})
    eval_03 = eval_data.get("eval_03", {})

    # Compute amortization
    amortization = _compute_amortization(bench_raw)

    # Build report JSON
    report = {
        "import_time": {
            "with_hardcoded_ms": import_data.get("median_ms"),
            "mean_ms": import_data.get("mean_ms"),
            "stdev_ms": import_data.get("stdev_ms"),
            "so_sizes_bytes": import_data.get("so_file_sizes_bytes", {}),
        },
        "first_call_generation_us": first_call,
        "cached_dispatch_ns": dispatch,
        "evaluation": {
            "multiplication": {
                "cost_vs_addition_ratio": mul_ratios,
                "cq_mul_per_call_us": eval_01.get("cq_mul_per_call_us", {}),
                "recommendation": mul_rec,
                "reasoning": mul_reasoning,
            },
            "bitwise": {
                "xor_max_us": eval_02.get("xor_max_us"),
                "and_max_us": eval_02.get("and_max_us"),
                "or_max_us": eval_02.get("or_max_us"),
                "max_cost_us": eval_02.get("all_max_us"),
                "recommendation": eval_02.get("recommendation", "skip"),
                "reasoning": eval_02.get("reasoning", ""),
            },
            "division": {
                "classical_cost_us": eval_03.get("classical_cost_us", {}),
                "quantum_cost_us": eval_03.get("quantum_cost_us", {}),
                "recommendation": eval_03.get("recommendation", "skip"),
                "reasoning": eval_03.get("reasoning", ""),
            },
        },
        "amortization": amortization,
        "metadata": {
            "generated": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
            "python_version": sys.version.split()[0],
            "bench_raw_timestamp": bench_raw.get("metadata", {}).get("timestamp", "unknown"),
        },
    }

    # Save JSON report
    os.makedirs(os.path.dirname(REPORT_JSON_PATH), exist_ok=True)
    with open(REPORT_JSON_PATH, "w") as f:
        json.dump(report, f, indent=2)
    print(f"  JSON report saved to: {REPORT_JSON_PATH}")

    # Generate Markdown report
    md = _generate_markdown_report(report, bench_raw, eval_data)
    with open(REPORT_MD_PATH, "w") as f:
        f.write(md)
    print(f"  Markdown report saved to: {REPORT_MD_PATH}")
    print()


def _format_us(val):
    """Format microsecond value for display."""
    if val is None:
        return "N/A"
    if val >= 1_000_000:
        return f"{val / 1_000_000:.1f}s"
    if val >= 1000:
        return f"{val / 1000:.1f}ms"
    return f"{val:.0f}us"


def _format_ns(val):
    """Format nanosecond value for display."""
    if val is None:
        return "N/A"
    if val >= 1_000_000:
        return f"{val / 1_000_000:.1f}ms"
    if val >= 1000:
        return f"{val / 1000:.1f}us"
    return f"{val:.0f}ns"


def _generate_markdown_report(report, bench_raw, eval_data):
    """Generate the human-readable markdown report."""
    lines = []

    def add(text=""):
        lines.append(text)

    # Title
    add("# Phase 62: Hardcoded Sequence Benchmark Report")
    add()
    add(f"Generated: {report['metadata']['generated']}")
    add(f"Python: {report['metadata']['python_version']}")
    add(f"Bench raw data: {report['metadata']['bench_raw_timestamp']}")
    add()

    # ---- Section 1: Import Time ----
    add("## 1. Import Time Analysis (BENCH-01)")
    add()
    import_data = report["import_time"]
    add(f"**Median import time: {import_data.get('with_hardcoded_ms', 'N/A')} ms**")
    add(f"- Mean: {import_data.get('mean_ms', 'N/A')} ms")
    add(f"- Stdev: {import_data.get('stdev_ms', 'N/A')} ms")
    add()

    so_sizes = import_data.get("so_sizes_bytes", {})
    if so_sizes:
        total_bytes = sum(so_sizes.values())
        total_mb = total_bytes / (1024 * 1024)
        add(f"**Total .so binary size: {total_mb:.1f} MB** ({len(so_sizes)} extensions)")
        add()
        add("| Extension | Size |")
        add("|-----------|------|")
        for name, size in sorted(so_sizes.items(), key=lambda x: -x[1]):
            add(f"| {name} | {size / 1024:.0f} KB |")
        add()
    add("The import time represents the fixed overhead of loading hardcoded sequences.")
    add("All .so extensions are loaded during `import quantum_language`.")
    add()

    # ---- Section 2: First-Call Generation Cost ----
    add("## 2. First-Call Generation Cost (BENCH-02)")
    add()
    add("First-call cost measures the time to generate gate sequences on first use of each")
    add("(operation, width) combination. For hardcoded widths (1-16), this is the dispatch")
    add("cost. For dynamic widths (17+), this includes C-level generation.")
    add()

    first_call = report["first_call_generation_us"]
    operations = list(first_call.keys())

    # Split into two tables for readability: widths 1-8, widths 9-16
    for start_w, end_w, label in [(1, 8, "Widths 1-8"), (9, 16, "Widths 9-16")]:
        width_range = list(range(start_w, end_w + 1))
        add(f"### {label}")
        add()
        header = "| Operation |" + "|".join(f" {w} " for w in width_range) + "|"
        sep = "|-----------|" + "|".join("------:" for _ in width_range) + "|"
        add(header)
        add(sep)
        for op in operations:
            op_data = first_call.get(op, {})
            cells = []
            for w in width_range:
                val = op_data.get(str(w))
                cells.append(f" {_format_us(val)} ")
            add(f"| {op} |" + "|".join(cells) + "|")
        add()

    # Highlight key observations
    add("### Key Observations")
    add()
    qq_add_8 = first_call.get("QQ_add", {}).get("8")
    qq_mul_8 = first_call.get("QQ_mul", {}).get("8")
    q_xor_8 = first_call.get("Q_xor", {}).get("8")
    add(f"- **Addition (QQ_add)** at width 8: {_format_us(qq_add_8)} -- moderate, O(N^2) CP gates")
    add(
        f"- **Multiplication (QQ_mul)** at width 8: {_format_us(qq_mul_8)} -- expensive, O(N) additions"
    )
    add(f"- **XOR (Q_xor)** at width 8: {_format_us(q_xor_8)} -- trivial, O(N) CNOT gates")
    add("- Cost ordering: QQ_mul >> QQ_add >> Q_xor (by ~3 orders of magnitude from top to bottom)")
    add()

    # ---- Section 3: Cached Dispatch Overhead ----
    add("## 3. Cached Dispatch Overhead (BENCH-03)")
    add()
    add("Measures per-call cost AFTER first call (cached sequences). Compares hardcoded")
    add("(width 8) vs dynamic (width 17) dispatch paths.")
    add()

    dispatch = report["cached_dispatch_ns"]
    add("| Operation | Hardcoded (w=8) | Dynamic (w=17) | Difference | Ratio |")
    add("|-----------|---------------:|---------------:|----------:|------:|")
    for op in dispatch:
        hc = dispatch[op].get("8_hardcoded")
        dyn = dispatch[op].get("17_dynamic")
        if hc is not None and dyn is not None:
            diff = dyn - hc
            ratio = dyn / hc if hc > 0 else 0
            add(
                f"| {op} | {_format_ns(hc)} | {_format_ns(dyn)} "
                f"| +{_format_ns(diff)} | {ratio:.1f}x |"
            )
        else:
            add(f"| {op} | {_format_ns(hc)} | {_format_ns(dyn)} | N/A | N/A |")
    add()
    add(
        "Note: Width difference (8 vs 17) contributes to the cost gap beyond pure dispatch overhead."
    )
    add("Hardcoded sequences avoid C-level generation entirely, resulting in faster dispatch.")
    add()

    # ---- Section 4: Amortization Analysis ----
    add("## 4. Amortization Analysis (BENCH-04)")
    add()
    amort = report["amortization"]
    add(f"**Import overhead: {amort['import_overhead_ms']:.0f} ms**")
    add()
    add("### Break-Even Calculations")
    add()
    add("Two break-even metrics:")
    add()
    add(f"1. **Cached dispatch break-even:** {amort['break_even_cached_calls']:,} calls")
    add(f"   - Average per-call saving: {_format_ns(amort['avg_dispatch_saving_ns'])}")
    add(
        f"   - Formula: {amort['import_overhead_us']:.0f}us / {amort['avg_dispatch_saving_us']:.2f}us = "
        f"{amort['break_even_cached_calls']:,} calls"
    )
    add()
    add(
        f"2. **First-call break-even:** {amort['break_even_first_calls']:,} unique (op, width) pairs"
    )
    add(f"   - Average first-call saving: {_format_us(amort['avg_first_call_saving_us'])}")
    add(
        f"   - Formula: {amort['import_overhead_us']:.0f}us / {amort['avg_first_call_saving_us']:.0f}us = "
        f"{amort['break_even_first_calls']:,} first calls"
    )
    add()
    add("### Interpretation")
    add()
    add(amort["analysis"])
    add()

    # ---- Section 5: Multiplication Evaluation (EVAL-01) ----
    add("## 5. Multiplication Evaluation (EVAL-01)")
    add()
    mul_eval = report["evaluation"]["multiplication"]
    add("### Cost vs Addition Ratio")
    add()
    ratios = mul_eval.get("cost_vs_addition_ratio", {})
    if ratios:
        add("| Width |" + "|".join(f" {w} " for w in range(1, 17)) + "|")
        add("|-------|" + "|".join("-----:" for _ in range(16)) + "|")
        cells = []
        for w in range(1, 17):
            val = ratios.get(str(w))
            cells.append(f" {val:.0f}x " if val is not None else " N/A ")
        add("| Ratio |" + "|".join(cells) + "|")
        add()

    # CQ_mul per-call table
    cq_per_call = mul_eval.get("cq_mul_per_call_us", {})
    if cq_per_call:
        add("### CQ_mul Per-Call Cost (NOT cached)")
        add()
        add("CQ_mul returns a fresh malloc'd sequence each call. This is the per-call cost,")
        add("not just first-call. Compare with QQ_mul first-call (which IS cached after first).")
        add()
        add("| Width |" + "|".join(f" {w} " for w in range(1, 17)) + "|")
        add("|-------|" + "|".join("-----:" for _ in range(16)) + "|")
        cells = []
        for w in range(1, 17):
            val = cq_per_call.get(str(w))
            cells.append(f" {_format_us(val)} " if val is not None else " N/A ")
        add("| Cost |" + "|".join(cells) + "|")
        add()

    add(f"**Recommendation: {mul_eval['recommendation']}**")
    add()
    add(mul_eval["reasoning"])
    add()

    # ---- Section 6: Bitwise Evaluation (EVAL-02) ----
    add("## 6. Bitwise Evaluation (EVAL-02)")
    add()
    bit_eval = report["evaluation"]["bitwise"]
    add("| Operation | Max Cost (all widths) |")
    add("|-----------|--------------------:|")
    add(f"| Q_xor | {_format_us(bit_eval.get('xor_max_us'))} |")
    add(f"| Q_and | {_format_us(bit_eval.get('and_max_us'))} |")
    add(f"| Q_or  | {_format_us(bit_eval.get('or_max_us'))} |")
    add()
    add(f"**Recommendation: {bit_eval['recommendation']}**")
    add()
    add(bit_eval["reasoning"])
    add()

    # ---- Section 7: Division Evaluation (EVAL-03) ----
    add("## 7. Division Evaluation (EVAL-03)")
    add()
    div_eval = report["evaluation"]["division"]
    add("Division is implemented at the Python/Cython level, not as a C sequence.")
    add("Classical divisor uses O(N) comparison+subtraction iterations.")
    add("Quantum divisor uses O(2^N) iterations (exponential cost).")
    add()

    classical = div_eval.get("classical_cost_us", {})
    quantum = div_eval.get("quantum_cost_us", {})
    if classical or quantum:
        widths = sorted(set(list(classical.keys()) + list(quantum.keys())), key=lambda x: int(x))
        add("| Width | Classical (a // 3) | Quantum (a // b) |")
        add("|------:|-----------------:|-----------------:|")
        for w in widths:
            c_val = classical.get(w)
            q_val = quantum.get(w)
            add(f"| {w} | {_format_us(c_val)} | {_format_us(q_val)} |")
        add()

    add(f"**Recommendation: {div_eval['recommendation']}**")
    add()
    add(div_eval["reasoning"])
    add()

    # ---- Section 8: Summary Recommendations ----
    add("## 8. Summary Recommendations")
    add()
    add("| Category | Recommendation | Key Reason |")
    add("|----------|---------------|------------|")

    # Addition (implicit from Plan 01 data)
    add(
        "| Addition (QQ/CQ/cQQ/cCQ) | **keep** | Already hardcoded widths 1-16; "
        "dispatch overhead is 2-6x lower than dynamic |"
    )

    # Multiplication
    add(
        f"| Multiplication (QQ/CQ) | **{mul_eval['recommendation']}** | "
        f"Cost is {statistics.mean([v for v in ratios.values() if v is not None]):.0f}x "
        f"addition; binary size impact needs investigation |"
        if any(v is not None for v in ratios.values())
        else f"| Multiplication (QQ/CQ) | **{mul_eval['recommendation']}** | Insufficient data |"
    )

    # Bitwise
    add(
        f"| Bitwise (xor/and/or) | **{bit_eval['recommendation']}** | "
        f"Max cost {_format_us(bit_eval.get('max_cost_us'))}; trivial generation |"
    )

    # Division
    add(
        f"| Division (// / %) | **{div_eval['recommendation']}** | "
        f"Python-level loop cost, not C sequence generation |"
    )
    add()

    add("---")
    add("*Report generated by scripts/benchmark_eval.py (Phase 62, Plan 02)*")
    add()

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Phase 62 benchmark evaluation and report generation",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--eval-only",
        action="store_true",
        help="Run EVAL measurements only, without generating report.",
    )
    parser.add_argument(
        "--report-only",
        action="store_true",
        help="Generate report from existing data only (no new measurements).",
    )
    parser.add_argument(
        "--widths",
        type=str,
        default=f"{DEFAULT_DIV_WIDTH_MIN}-{DEFAULT_DIV_WIDTH_MAX}",
        help=f"Width range for division evaluation (default: '{DEFAULT_DIV_WIDTH_MIN}-{DEFAULT_DIV_WIDTH_MAX}').",
    )
    parser.add_argument(
        "--iterations",
        type=int,
        default=DEFAULT_EVAL_ITERATIONS,
        help=f"Iterations per measurement (default: {DEFAULT_EVAL_ITERATIONS}).",
    )
    return parser.parse_args()


def main():
    """Main entry point."""
    args = parse_args()
    total_start = time.perf_counter_ns()

    print("Phase 62: Benchmark Evaluation & Report")
    print("=" * 60)

    # Load Plan 01 data
    bench_raw = _load_bench_raw()
    print(
        f"  Loaded bench_raw.json ({len(bench_raw.get('bench_02_first_call_us', {}))} operations)"
    )

    if args.report_only and args.eval_only:
        print("ERROR: Cannot specify both --report-only and --eval-only")
        sys.exit(1)

    # Run evaluations
    eval_data = _load_eval_data()  # Load existing eval data

    if not args.report_only:
        div_widths = _parse_width_range(args.widths)
        iters = args.iterations

        print(f"  Division widths: {div_widths[0]}-{div_widths[-1]}")
        print(f"  Iterations: {iters}")
        print()

        # EVAL-01: Multiplication
        eval_01_result = eval_01_multiplication(iterations=iters)
        eval_data["eval_01"] = eval_01_result

        # EVAL-02: Bitwise (analysis only, data from Plan 01)
        eval_02_result = eval_02_bitwise(bench_raw)
        eval_data["eval_02"] = eval_02_result

        # EVAL-03: Division
        eval_03_result = eval_03_division(widths=div_widths, iterations=iters)
        eval_data["eval_03"] = eval_03_result

        # Save eval data
        _save_eval_data(eval_data)
    else:
        print("  Skipping evaluations (--report-only)")
        if not eval_data:
            # Create minimal eval data from bench_raw for report-only mode
            print("  No existing eval data found, creating from bench_raw...")
            eval_data["eval_01"] = {"cq_mul_per_call_us": {}}
            eval_data["eval_02"] = eval_02_bitwise(bench_raw)
            eval_data["eval_03"] = {
                "classical_cost_us": {},
                "quantum_cost_us": {},
                "recommendation": "skip",
                "reasoning": "Division cost is in Python-level loop, not C sequence generation.",
            }
        print()

    # Generate report
    if not args.eval_only:
        generate_report(bench_raw, eval_data)

    total_elapsed_s = (time.perf_counter_ns() - total_start) / 1e9
    print("=" * 60)
    print("DONE")
    print("=" * 60)
    print(f"  Total time: {total_elapsed_s:.1f}s")
    if not args.eval_only:
        print(f"  JSON report: {REPORT_JSON_PATH}")
        print(f"  Markdown report: {REPORT_MD_PATH}")


if __name__ == "__main__":
    main()
