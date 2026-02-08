"""Compare two benchmark JSON files and produce a regression report.

Compares Phase 62 baseline (before) against post-Phase-63 (after) benchmark
data and generates a structured regression report with per-metric verdicts.

Usage:
    python scripts/benchmark_compare.py
    python scripts/benchmark_compare.py --before path/to/before.json --after path/to/after.json
    python scripts/benchmark_compare.py --tolerance 10.0
    python scripts/benchmark_compare.py --output-dir benchmarks/results/

Output:
    benchmarks/results/regression_report.json  (structured comparison data)
    benchmarks/results/regression_report.md    (human-readable report)
"""

import argparse
import json
import os
import sys
import time

# ---------------------------------------------------------------------------
# Project root detection
# ---------------------------------------------------------------------------
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DEFAULT_BEFORE = os.path.join(PROJECT_ROOT, "benchmarks", "results", "bench_raw.json")
DEFAULT_AFTER = os.path.join(PROJECT_ROOT, "benchmarks", "results", "bench_raw_post63.json")
DEFAULT_OUTPUT_DIR = os.path.join(PROJECT_ROOT, "benchmarks", "results")
DEFAULT_TOLERANCE = 15.0


# ---------------------------------------------------------------------------
# Utility helpers
# ---------------------------------------------------------------------------


def pct_change(before, after):
    """Compute percentage change from before to after.

    Positive means regression (slower/larger), negative means improvement.
    Returns None if before is zero or None.
    """
    if before is None or after is None or before == 0:
        return None
    return ((after - before) / before) * 100.0


def verdict(change_pct, tolerance):
    """Return PASS, IMPROVED, or FAIL based on change percentage.

    - Negative change (improvement) -> IMPROVED
    - Change within tolerance -> PASS
    - Change beyond tolerance -> FAIL
    - None -> WARNING (missing data)
    """
    if change_pct is None:
        return "WARNING"
    if change_pct < 0:
        return "IMPROVED"
    if change_pct <= tolerance:
        return "PASS"
    return "FAIL"


# ---------------------------------------------------------------------------
# BENCH-01: Import Time Comparison
# ---------------------------------------------------------------------------


def compare_bench_01(before, after, tolerance):
    """Compare import time measurements."""
    b = before.get("bench_01_import_time", {})
    a = after.get("bench_01_import_time", {})

    b_median = b.get("median_ms")
    a_median = a.get("median_ms")
    change = pct_change(b_median, a_median)
    v = verdict(change, tolerance)

    return {
        "before_median_ms": b_median,
        "after_median_ms": a_median,
        "before_mean_ms": b.get("mean_ms"),
        "after_mean_ms": a.get("mean_ms"),
        "before_stdev_ms": b.get("stdev_ms"),
        "after_stdev_ms": a.get("stdev_ms"),
        "change_pct": round(change, 1) if change is not None else None,
        "verdict": v,
    }


# ---------------------------------------------------------------------------
# BENCH-02: First-Call Generation Cost Comparison
# ---------------------------------------------------------------------------


def compare_bench_02(before, after, tolerance):
    """Compare first-call generation cost per operation and width."""
    b_fc = before.get("bench_02_first_call_us", {})
    a_fc = after.get("bench_02_first_call_us", {})

    all_ops = sorted(set(list(b_fc.keys()) + list(a_fc.keys())))
    results = {}
    regressions = []
    section_verdict = "PASS"

    for op in all_ops:
        b_op = b_fc.get(op, {})
        a_op = a_fc.get(op, {})
        all_widths = sorted(set(list(b_op.keys()) + list(a_op.keys())), key=lambda w: int(w))
        results[op] = {}

        for width in all_widths:
            b_val = b_op.get(width)
            a_val = a_op.get(width)
            change = pct_change(b_val, a_val)
            v = verdict(change, tolerance)

            results[op][width] = {
                "before_us": b_val,
                "after_us": a_val,
                "change_pct": round(change, 1) if change is not None else None,
                "verdict": v,
            }

            if v == "FAIL":
                regressions.append(f"{op} width={width}: {change:+.1f}%")
                section_verdict = "FAIL"

    return {
        "operations": results,
        "regressions": regressions,
        "verdict": section_verdict,
    }


# ---------------------------------------------------------------------------
# BENCH-03: Cached Dispatch Overhead Comparison
# ---------------------------------------------------------------------------


def compare_bench_03(before, after, tolerance):
    """Compare cached dispatch overhead measurements."""
    b_cd = before.get("bench_03_cached_dispatch_ns", {})
    a_cd = after.get("bench_03_cached_dispatch_ns", {})

    all_ops = sorted(set(list(b_cd.keys()) + list(a_cd.keys())))
    results = {}
    regressions = []
    section_verdict = "PASS"

    for op in all_ops:
        b_op = b_cd.get(op, {})
        a_op = a_cd.get(op, {})
        all_tags = sorted(set(list(b_op.keys()) + list(a_op.keys())))
        results[op] = {}

        for tag in all_tags:
            b_val = b_op.get(tag)
            a_val = a_op.get(tag)
            change = pct_change(b_val, a_val)
            v = verdict(change, tolerance)

            results[op][tag] = {
                "before_ns": b_val,
                "after_ns": a_val,
                "change_pct": round(change, 1) if change is not None else None,
                "verdict": v,
            }

            if v == "FAIL":
                regressions.append(f"{op} {tag}: {change:+.1f}%")
                section_verdict = "FAIL"

    return {
        "operations": results,
        "regressions": regressions,
        "verdict": section_verdict,
    }


# ---------------------------------------------------------------------------
# .so File Size Comparison
# ---------------------------------------------------------------------------


def compare_so_sizes(before, after, tolerance):
    """Compare .so file sizes between before and after."""
    b_sizes = before.get("bench_01_import_time", {}).get("so_file_sizes_bytes", {})
    a_sizes = after.get("bench_01_import_time", {}).get("so_file_sizes_bytes", {})

    all_files = sorted(set(list(b_sizes.keys()) + list(a_sizes.keys())))
    results = {}
    section_verdict = "PASS"
    regressions = []

    b_total = sum(b_sizes.values())
    a_total = sum(a_sizes.values())

    for f in all_files:
        b_val = b_sizes.get(f)
        a_val = a_sizes.get(f)
        change = pct_change(b_val, a_val)
        v = verdict(change, tolerance)

        results[f] = {
            "before_bytes": b_val,
            "after_bytes": a_val,
            "change_pct": round(change, 1) if change is not None else None,
            "verdict": v,
        }

        # Flag any .so size increase > 5% as notable
        if change is not None and change > 5.0:
            regressions.append(f"{f}: {change:+.1f}%")
            section_verdict = "FAIL"

    total_change = pct_change(b_total, a_total)
    total_v = verdict(total_change, 5.0)  # Stricter for total size

    return {
        "files": results,
        "total_before_bytes": b_total,
        "total_after_bytes": a_total,
        "total_change_pct": round(total_change, 1) if total_change is not None else None,
        "total_verdict": total_v,
        "regressions": regressions,
        "verdict": total_v if not regressions else section_verdict,
    }


# ---------------------------------------------------------------------------
# Overall comparison
# ---------------------------------------------------------------------------


def compare_all(before, after, tolerance):
    """Run all comparisons and compute overall verdict."""
    bench_01 = compare_bench_01(before, after, tolerance)
    bench_02 = compare_bench_02(before, after, tolerance)
    bench_03 = compare_bench_03(before, after, tolerance)
    so_sizes = compare_so_sizes(before, after, tolerance)

    section_verdicts = [
        bench_01["verdict"],
        bench_02["verdict"],
        bench_03["verdict"],
        so_sizes["verdict"],
    ]

    overall = "PASS" if all(v in ("PASS", "IMPROVED") for v in section_verdicts) else "FAIL"

    return {
        "overall_verdict": overall,
        "tolerance_pct": tolerance,
        "bench_01": bench_01,
        "bench_02": bench_02,
        "bench_03": bench_03,
        "so_sizes": so_sizes,
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
    }


# ---------------------------------------------------------------------------
# Markdown report generation
# ---------------------------------------------------------------------------


def generate_markdown(report, before_path, after_path):
    """Generate human-readable markdown report."""
    lines = []
    lines.append("# Regression Verification Report")
    lines.append("")
    lines.append(f"**Overall Verdict: {report['overall_verdict']}**")
    lines.append("")
    lines.append(f"- **Tolerance:** {report['tolerance_pct']}%")
    lines.append(f"- **Before:** `{os.path.basename(before_path)}` (Phase 62 baseline)")
    lines.append(f"- **After:** `{os.path.basename(after_path)}` (post-Phase-63)")
    lines.append(f"- **Generated:** {report['timestamp']}")
    lines.append("")

    # BENCH-01: Import Time
    b01 = report["bench_01"]
    lines.append("## BENCH-01: Import Time")
    lines.append("")
    lines.append("| Metric | Before (Phase 62) | After (Phase 63) | Change | Verdict |")
    lines.append("|--------|-------------------|-------------------|--------|---------|")
    lines.append(
        f"| Median | {b01['before_median_ms']} ms | {b01['after_median_ms']} ms "
        f"| {b01['change_pct']:+.1f}% | {b01['verdict']} |"
    )
    lines.append(
        f"| Mean | {b01['before_mean_ms']} ms | {b01['after_mean_ms']} ms "
        f"| {pct_change(b01['before_mean_ms'], b01['after_mean_ms']):+.1f}% | - |"
    )
    lines.append(f"| Stdev | {b01['before_stdev_ms']} ms | {b01['after_stdev_ms']} ms | - | - |")
    lines.append("")

    # BENCH-02: First-Call Generation Cost
    b02 = report["bench_02"]
    lines.append("## BENCH-02: First-Call Generation Cost")
    lines.append("")
    lines.append(f"**Section Verdict: {b02['verdict']}**")
    lines.append("")

    # Summary table for addition operations
    add_ops = ["QQ_add", "CQ_add", "cQQ_add", "cCQ_add"]
    other_ops = [op for op in b02["operations"] if op not in add_ops]

    lines.append("### Addition Operations (hardcoded, widths 1-16)")
    lines.append("")
    lines.append("| Operation | Width | Before (us) | After (us) | Change | Verdict |")
    lines.append("|-----------|-------|-------------|------------|--------|---------|")

    for op in add_ops:
        if op not in b02["operations"]:
            continue
        op_data = b02["operations"][op]
        for width in sorted(op_data.keys(), key=lambda w: int(w)):
            d = op_data[width]
            b_val = f"{d['before_us']:.1f}" if d["before_us"] is not None else "N/A"
            a_val = f"{d['after_us']:.1f}" if d["after_us"] is not None else "N/A"
            c_str = f"{d['change_pct']:+.1f}%" if d["change_pct"] is not None else "N/A"
            lines.append(f"| {op} | {width} | {b_val} | {a_val} | {c_str} | {d['verdict']} |")

    lines.append("")

    # Other operations summary (just key widths)
    lines.append("### Other Operations (key widths)")
    lines.append("")
    lines.append("| Operation | Width | Before (us) | After (us) | Change | Verdict |")
    lines.append("|-----------|-------|-------------|------------|--------|---------|")

    for op in other_ops:
        if op not in b02["operations"]:
            continue
        op_data = b02["operations"][op]
        for width in ["1", "4", "8", "16"]:
            if width not in op_data:
                continue
            d = op_data[width]
            b_val = f"{d['before_us']:.1f}" if d["before_us"] is not None else "N/A"
            a_val = f"{d['after_us']:.1f}" if d["after_us"] is not None else "N/A"
            c_str = f"{d['change_pct']:+.1f}%" if d["change_pct"] is not None else "N/A"
            lines.append(f"| {op} | {width} | {b_val} | {a_val} | {c_str} | {d['verdict']} |")

    lines.append("")

    if b02["regressions"]:
        lines.append("### Regressions Detected")
        lines.append("")
        for r in b02["regressions"]:
            lines.append(f"- {r}")
        lines.append("")

    # BENCH-03: Cached Dispatch Overhead
    b03 = report["bench_03"]
    lines.append("## BENCH-03: Cached Dispatch Overhead")
    lines.append("")
    lines.append(f"**Section Verdict: {b03['verdict']}**")
    lines.append("")
    lines.append("| Operation | Path | Before (ns) | After (ns) | Change | Verdict |")
    lines.append("|-----------|------|-------------|------------|--------|---------|")

    for op in sorted(b03["operations"].keys()):
        op_data = b03["operations"][op]
        for tag in sorted(op_data.keys()):
            d = op_data[tag]
            b_val = f"{d['before_ns']:.1f}" if d["before_ns"] is not None else "N/A"
            a_val = f"{d['after_ns']:.1f}" if d["after_ns"] is not None else "N/A"
            c_str = f"{d['change_pct']:+.1f}%" if d["change_pct"] is not None else "N/A"
            lines.append(f"| {op} | {tag} | {b_val} | {a_val} | {c_str} | {d['verdict']} |")

    lines.append("")

    if b03["regressions"]:
        lines.append("### Regressions Detected")
        lines.append("")
        for r in b03["regressions"]:
            lines.append(f"- {r}")
        lines.append("")

    # .so File Sizes
    so = report["so_sizes"]
    lines.append("## .so Binary Sizes")
    lines.append("")
    lines.append(f"**Section Verdict: {so['verdict']}**")
    lines.append("")
    lines.append("| File | Before (bytes) | After (bytes) | Change | Verdict |")
    lines.append("|------|----------------|---------------|--------|---------|")

    for f in sorted(so["files"].keys()):
        d = so["files"][f]
        b_val = f"{d['before_bytes']:,}" if d["before_bytes"] is not None else "N/A"
        a_val = f"{d['after_bytes']:,}" if d["after_bytes"] is not None else "N/A"
        c_str = f"{d['change_pct']:+.1f}%" if d["change_pct"] is not None else "N/A"
        lines.append(f"| {f} | {b_val} | {a_val} | {c_str} | {d['verdict']} |")

    total_b = f"{so['total_before_bytes']:,}" if so["total_before_bytes"] else "N/A"
    total_a = f"{so['total_after_bytes']:,}" if so["total_after_bytes"] else "N/A"
    total_c = f"{so['total_change_pct']:+.1f}%" if so["total_change_pct"] is not None else "N/A"
    lines.append(
        f"| **TOTAL** | **{total_b}** | **{total_a}** | **{total_c}** | **{so['total_verdict']}** |"
    )
    lines.append("")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def parse_args():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Compare benchmark results and produce regression report",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--before",
        type=str,
        default=DEFAULT_BEFORE,
        help=f"Path to baseline benchmark JSON (default: {os.path.basename(DEFAULT_BEFORE)})",
    )
    parser.add_argument(
        "--after",
        type=str,
        default=DEFAULT_AFTER,
        help=f"Path to post-change benchmark JSON (default: {os.path.basename(DEFAULT_AFTER)})",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=DEFAULT_TOLERANCE,
        help=f"Acceptable regression percentage (default: {DEFAULT_TOLERANCE}%%)",
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default=DEFAULT_OUTPUT_DIR,
        help=f"Output directory for reports (default: {os.path.basename(DEFAULT_OUTPUT_DIR)}/)",
    )
    return parser.parse_args()


def main():
    """Main entry point."""
    args = parse_args()

    print("Benchmark Comparison: Regression Verification")
    print("=" * 60)
    print(f"  Before: {args.before}")
    print(f"  After:  {args.after}")
    print(f"  Tolerance: {args.tolerance}%")
    print(f"  Output: {args.output_dir}/")
    print()

    # Load data
    if not os.path.exists(args.before):
        print(f"ERROR: Before file not found: {args.before}")
        sys.exit(1)
    if not os.path.exists(args.after):
        print(f"ERROR: After file not found: {args.after}")
        sys.exit(1)

    with open(args.before) as f:
        before = json.load(f)
    with open(args.after) as f:
        after = json.load(f)

    # Compare
    report = compare_all(before, after, args.tolerance)

    # Save JSON report
    os.makedirs(args.output_dir, exist_ok=True)
    json_path = os.path.join(args.output_dir, "regression_report.json")
    with open(json_path, "w") as f:
        json.dump(report, f, indent=2)
    print(f"  JSON report: {json_path}")

    # Save markdown report
    md_path = os.path.join(args.output_dir, "regression_report.md")
    md_content = generate_markdown(report, args.before, args.after)
    with open(md_path, "w") as f:
        f.write(md_content)
        f.write("\n")
    print(f"  Markdown report: {md_path}")

    # Print summary
    print()
    print("=" * 60)
    print(f"  Overall Verdict: {report['overall_verdict']}")
    print(f"  BENCH-01 (Import Time): {report['bench_01']['verdict']}")
    print(f"  BENCH-02 (First-Call):  {report['bench_02']['verdict']}")
    print(f"  BENCH-03 (Dispatch):    {report['bench_03']['verdict']}")
    print(f"  .so Sizes:              {report['so_sizes']['verdict']}")
    print("=" * 60)

    return 0 if report["overall_verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
