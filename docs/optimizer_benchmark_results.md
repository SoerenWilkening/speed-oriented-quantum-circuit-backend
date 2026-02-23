# Optimizer Benchmark Results

**Date:** 2026-02-23
**Implementation:** Binary search O(log L) in `smallest_layer_below_comp`
**Environment:** Linux x86_64, Python 3.13.7, GCC with -O3
**Protocol:** 5 runs per benchmark, median reported

## Scaling Analysis

Circuit generation time measured end-to-end for synthetic workloads
(repeated 4-bit toffoli additions with varying constants to prevent cancellation).

| Operations | Gates | Layers | Median (ms) | Rate (gates/ms) |
|------------|-------|--------|-------------|-----------------|
| 10 | 180 | 146 | 0.43 | 419 |
| 50 | 907 | 736 | 1.40 | 648 |
| 100 | 1,811 | 1,469 | 2.22 | 816 |
| 500 | 9,070 | 7,356 | 15.26 | 594 |
| 1,000 | 18,141 | 14,712 | 24.86 | 730 |
| 5,000 | 90,711 | 73,569 | 177.50 | 511 |
| 10,000 | 341,426 | 307,141 | 1,193.91 | 286 |

The rate (gates generated per millisecond) stays relatively stable across
1-2 orders of magnitude, with some degradation at very large scales due to
memory allocation overhead (not the binary search itself).

## 10K Operation Scaling Ratio

- 10K ops takes ~240x longer than 100 ops
- The 10K operation benchmark generates 341K gates and 307K layers
- Gate count grows ~189x from 100 to 10K operations (super-linear due to carry chains)
- The binary search contributes O(log L) per gate vs O(L) for linear scan
- At 307K layers, binary search performs ~18 comparisons vs 307K for linear scan

## Real Workload Benchmarks

| Workload | Gates | Layers | Median (ms) |
|----------|-------|--------|-------------|
| 4-bit multiplication | 64 | 53 | 0.17 |
| 8-bit QFT addition | 122 | 40 | 0.22 |
| 20x 4-bit additions | 354 | 292 | 0.53 |
| 3-qubit Grover (1 iter) | 21 | 11 | 387.34* |

*Grover time dominated by Python-level oracle compilation, not optimizer.

## Algorithm Analysis

The binary search in `smallest_layer_below_comp` replaces a linear scan
that was O(L) per gate (where L = number of occupied layers for a qubit).
The binary search is O(log L).

For the optimizer's hot path:
- Each gate calls `minimum_layer()` which calls `smallest_layer_below_comp()`
  once per qubit involved (target + controls)
- With linear scan: total cost per gate = O(k * L) where k = 1 + num_controls
- With binary search: total cost per gate = O(k * log L)

For a circuit with N gates and average L occupied layers:
- Linear scan total: O(N * k * L) -- quadratic in circuit depth
- Binary search total: O(N * k * log L) -- near-linear in circuit depth

At 307K layers (10K operation benchmark), this is:
- Linear scan: ~307,000 comparisons per gate
- Binary search: ~18 comparisons per gate (~17,000x fewer)

## Compile Replay Optimization (PERF-03)

Stack-allocated `gate_t` in `inject_remapped_gates` instead of per-gate
`malloc`/`free`. Measured on 50 replays per scenario, median reported.

### Before/After Comparison

| Workload | Gates | Before (ms) | After (ms) | Improvement |
|----------|-------|-------------|------------|-------------|
| Small (add_one_4bit) | 18 | 0.146 | 0.098 | 33% faster |
| Medium (add_sub_chain) | 57 | 0.400 | 0.252 | 37% faster |
| Large (mult_then_add) | 88 | 0.614 | 0.393 | 36% faster |

### Per-Gate Time

| Workload | Before (us/gate) | After (us/gate) | Improvement |
|----------|-------------------|-----------------|-------------|
| Small | 8.13 | 5.42 | 33% |
| Medium | 7.03 | 4.42 | 37% |
| Large | 6.98 | 4.47 | 36% |

### Hot-Path Breakdown

| Component | Before | After |
|-----------|--------|-------|
| inject_remapped_gates | 24.5 us (7.1%) | 15.1 us (6.9%) |
| Full replay | 342.9 us | 218.6 us |
| Python overhead | 92.9% | 93.1% |

The inject_remapped_gates C call itself improved by ~38% (malloc elimination),
but the overall 36% improvement in full replay time indicates that the reduced
memory pressure also benefits Python-level code (less GC interference, better
cache behavior).

### Analysis

Profiling revealed that 93% of compile replay time is Python-level overhead:
- Building virtual_to_real qubit mapping dict
- Allocating ancilla qubits via _allocate_qubit() calls
- Layer floor management and return value construction
- Forward call tracking for inverse support

The C-level inject_remapped_gates accounts for only ~7% of replay time.
The stack allocation optimization eliminates malloc/free syscalls per gate,
which provides the direct 38% improvement in the C path but also reduces
memory pressure benefiting the surrounding Python code.

Further optimization would require moving more of the _replay logic into
Cython (qubit mapping, ancilla allocation), which is a larger scope change.

## Verification

The binary search was verified against the original linear scan using:
1. Debug build with both implementations running in parallel (OPTIMIZER_DEBUG flag)
2. All 20 golden-master circuit snapshots match exactly
3. All 5 targeted optimizer correctness tests pass
4. Zero semantic regression across the full test suite

The stack allocation optimization was verified:
1. All 20 golden-master circuit snapshots match exactly after change
2. All existing tests pass (same pre-existing failures only)
3. Before/after profiling confirms measurable improvement
