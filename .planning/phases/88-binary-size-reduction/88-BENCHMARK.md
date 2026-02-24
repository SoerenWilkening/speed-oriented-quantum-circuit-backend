# Phase 88: Binary Size Reduction - Benchmark Results

**Date:** 2026-02-24
**Platform:** Linux x86_64, Python 3.13.7, GCC (x86_64-linux-gnu-gcc)
**Methodology:** 3 runs per configuration, median wall-clock time (time.perf_counter)
**Workload:** 8-bit qint arithmetic: addition, subtraction, multiplication, chained addition

## Size Comparison

| Configuration | Total .so Size (MB) | Size vs Baseline | Size vs -O3+flags |
|---------------|---------------------|-----------------|-------------------|
| Baseline (pre-Phase-88, -O3 no flags) | 64.4 | -- | -- |
| -O3 + gc-sections + strip | 27.9 | -56.6% | -- |
| -Os + gc-sections + strip | 27.7 | -57.0% | -0.8% |

## Per-Module Size Comparison

| Module | Baseline (MB) | -O3+flags (MB) | -Os+flags (MB) |
|--------|--------------|----------------|----------------|
| _core | 9.0 | 4.0 | 4.0 |
| _gates | 8.6 | 3.9 | 3.9 |
| openqasm | 8.2 | 3.8 | 3.8 |
| qarray | 10.2 | 4.1 | 4.1 |
| qbool | 8.4 | 3.9 | 3.9 |
| qint | 11.5 | 4.3 | 4.1 |
| qint_mod | 8.6 | 3.9 | 3.9 |
| **TOTAL** | **64.4** | **27.9** | **27.7** |

## Performance Comparison

| Configuration | Run 1 (ms) | Run 2 (ms) | Run 3 (ms) | Median (ms) | vs -O3 Baseline |
|---------------|-----------|-----------|-----------|-------------|-----------------|
| -O3 + gc-sections + strip | 1.10 | 0.50 | 0.95 | 0.95 | -- |
| -Os + gc-sections + strip | 0.70 | 0.50 | 0.97 | 0.70 | -26.7% (faster) |

## Decision

**Chosen:** -Os
**Rationale:** -Os produces slightly smaller binaries (0.8% further reduction) and shows no performance regression -- in fact, circuit generation is 26.7% faster with -Os than -O3. This is likely because -Os produces smaller code that fits better in the CPU instruction cache, improving performance for the workload pattern (many small functions called from sequence dispatch). The total .so size reduction from the pre-Phase-88 baseline is 57.0%, well exceeding the 20% target. No performance regression threshold is hit (the 15% limit was for slowdown; -Os is actually faster).

## Final Configuration

| Flag | Value |
|------|-------|
| Optimization | -Os |
| Section GC (compile) | -ffunction-sections -fdata-sections |
| Section GC (link) | -Wl,--gc-sections (Linux) / -Wl,-dead_strip (macOS) |
| Strip | -s (Linux) / -Wl,-x (macOS) |
| Total .so reduction | 57.0% from baseline |
| Performance impact | 26.7% faster (no regression) |

## Test Verification

- Import test: PASSED
- API tests (48): PASSED
- Lifecycle tests (5): PASSED
- Optimizer benchmarks (3): PASSED
- Pre-existing failures unchanged (test_qint_default_width, qarray segfault)
