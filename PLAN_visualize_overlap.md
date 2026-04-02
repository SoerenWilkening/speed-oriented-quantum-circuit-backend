# Implementation Plan: Fix ASCII Circuit Visualization for Overlapping Gates

**Ref**: `PRD_visualize_overlap.md`
**Package**: circuit-c-backend
**Files affected**: `src/circuit_output.c`, `tests/test_visualize_overlap.c`

## Overview

Three implementation steps, each independently testable, totaling approximately 150-200 lines of new/changed code.

---

## Step 1: Sub-column grouping helper function

**Goal**: Implement a static helper that computes sub-column assignments for all gates in a single layer.

**File**: `src/circuit_output.c`

**What to implement**:

```
static uint32_t compute_visual_subcols(
    const circuit_ctx_t *ctx,
    uint32_t layer,
    uint32_t *subcol_of_gate,   // out: subcol_of_gate[gi] = sub-column index
    uint32_t max_gates          // capacity of subcol_of_gate array
);
// Returns: number of sub-columns (>= 1)
```

**Algorithm**:
1. For each gate `gi` in the layer, compute `[min_q, max_q]` using `qc_min_qubit()` / `qc_max_qubit()`.
2. Sort gates by `min_q` (use a small local index array + insertion sort -- layer gate counts are small).
3. Greedy assignment: maintain an array `subcol_max_qubit[sc]` tracking the highest qubit used by each sub-column so far. For each gate (in sorted order), assign it to the first sub-column where `subcol_max_qubit[sc] < gate.min_q`. If none fits, create a new sub-column.
4. Write results to `subcol_of_gate[]`.

**Estimated lines**: ~60 lines of new code.

**Testability**: Can be unit-tested by building a circuit with known overlapping gates, calling the helper, and verifying the sub-column assignments. However, since it is `static`, testing will be done indirectly via the visualization output in Step 3.

**Dependencies**: None (pure addition).

---

## Step 2: Refactor `qc_circuit_visualize()` rendering loop

**Goal**: Replace the current single-column-per-layer rendering with a sub-column-aware rendering loop.

**File**: `src/circuit_output.c` (modify `qc_circuit_visualize`, lines 341-428)

**What changes**:

### 2a: Pre-compute sub-column data for all displayed layers

Before the rendering loops, allocate temporary arrays:
- `uint32_t subcol_of_gate[max_display][MAX_GATES_PER_LAYER]` -- use VLA or small heap alloc
- `uint32_t subcol_count[max_display]` -- number of sub-columns per layer

For each displayed layer, call `compute_visual_subcols()` to populate these.

Practically, since gate counts per layer are small (typically < 32), use stack arrays with `QC_GATES_PER_LAYER_BLOCK` as max. If a layer exceeds this, fall back to treating all gates as one sub-column (current behavior).

### 2b: Update layer header

Change the header loop to account for layers that expand into multiple sub-columns. Each layer occupies `3 * subcol_count[layer]` characters wide instead of the fixed 3.

### 2c: Update qubit row rendering

The inner loop changes from:
```c
for (layer = 0; layer < max_display; layer++) {
    // render one column
}
```
to:
```c
for (layer = 0; layer < max_display; layer++) {
    for (sc = 0; sc < subcol_count[layer]; sc++) {
        // render one sub-column:
        // - find gate_idx for this qubit in this layer
        // - if gate exists AND subcol_of_gate[layer][gate_idx] == sc: render gate symbol
        // - else if no gate but qubit is "between" any gate in THIS sub-column: render |
        // - else: render ---
    }
}
```

The key change in the "between" wire check: only consider gates assigned to the current sub-column `sc`, not all gates in the layer.

### 2d: Cleanup temporary allocations

Free any heap-allocated temporaries after the rendering is complete.

**Estimated lines**: ~80 lines changed (net ~40 added, ~40 replaced).

**Testability**: Visual inspection of output for known circuits. Formal tests in Step 3.

**Dependencies**: Step 1 (uses `compute_visual_subcols`).

---

## Step 3: Add test cases

**Goal**: Add a C test file that constructs circuits with overlapping gates and captures/validates the visualization output.

**File**: `tests/test_visualize_overlap.c` (new file)

**Test cases**:

### T1: Overlapping CX + X
- Build: `CX(0, 2)` and `X(1)` in the same layer.
- Verify: output contains both `+` (or `X`) and `@` symbols, and the `X` on qubit 1 is not replaced by `|`.
- Method: redirect stdout to a buffer (use `open_memstream` or `tmpfile` + `freopen`), call `qc_circuit_visualize()`, check buffer contents.

### T2: Non-overlapping gates share column
- Build: `X(0)` and `X(2)` in the same layer (no qubit 1 involvement).
- Verify: only one visual column per layer (output width matches single-column expectation).

### T3: Three-way overlap
- Build: `CCX(0, 2, 4)`, `H(1)`, `Z(3)` in the same layer.
- Verify: H and Z share one sub-column; CCX is in another. All three gate symbols appear.

### T4: No overlap (regression)
- Build: a simple 3-qubit circuit with no overlapping gates.
- Verify: output matches current format exactly.

**Estimated lines**: ~120 lines.

**Build integration**: Add `test_visualize_overlap` to `CMakeLists.txt` test targets.

**Dependencies**: Steps 1 and 2 (tests exercise the modified visualization).

---

## Summary

| Step | Description | File(s) | Lines | Depends on |
|------|-------------|---------|-------|------------|
| 1 | Sub-column grouping helper | `src/circuit_output.c` | ~60 | -- |
| 2 | Refactor rendering loop | `src/circuit_output.c` | ~80 | Step 1 |
| 3 | Test cases | `tests/test_visualize_overlap.c`, `CMakeLists.txt` | ~120 | Steps 1, 2 |

**Total**: ~260 lines of new/changed code across 2-3 files. All within the 500-line-per-module limit.
