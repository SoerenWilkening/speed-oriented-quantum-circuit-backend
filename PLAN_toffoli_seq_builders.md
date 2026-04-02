# Implementation Plan: Toffoli Multiplication and CQ Comparison Sequence Builders

**PRD**: `circuit-c-backend/PRD_toffoli_seq_builders.md`

## Overview

Two new C source files, each containing capture-based sequence builders that follow established patterns. All functions use the same capture approach: create a temporary circuit, run the toffoli operation, extract gates into a `qc_sequence_t`.

## Pre-Implementation Findings

- `qc_dynamic_cq_add(ctx, target, width, value)` exists -- CQ subtraction can be done as `add(target, width, -value)` since two's complement arithmetic wraps correctly.
- `qc_dynamic_ccq_add(ctx, target, width, value, control)` exists -- controlled CQ subtraction is `ccq_add(target, width, -value, control)`.
- No need for new `qc_dynamic_cq_sub` or `qc_dynamic_ccq_sub` helper functions.
- `capture_helpers.h` provides `cmp_create_capture_ctx()` and `cmp_capture_circuit_to_sequence()`.
- `cmul_sequences.c` provides a separate but equivalent capture pattern with `cmul_create_capture_ctx()` (which also sets `QC_ARITH_TOFFOLI` mode).

## Step 1: Toffoli Multiplication Sequence Builders

**File**: `circuit-c-backend/src/toffoli_mul_sequences.c` (new, ~120 lines)

**Functions**:

### `qc_toffoli_cq_mul_seq(int bits, int64_t value)`

Capture-based builder for uncontrolled toffoli CQ multiplication.

```
Qubit layout: [0..n-1] result, [n..2n-1] target
```

Steps:
1. Validate `bits` (1-64)
2. Create capture context with `cmp_create_capture_ctx(reg_qubits + headroom)`
3. Set arith mode to `QC_ARITH_TOFFOLI`
4. Allocate `2n` register qubits
5. Build result[] and target[] arrays
6. Call `qc_toffoli_cq_mul(ctx, result, n, target, n, value)`
7. Capture to sequence, set `total_qubits`, destroy ctx

Pattern source: `cmul_sequences.c::qc_c_arith_cq_mul_seq` minus the ext_ctrl qubit.

### `qc_toffoli_qq_mul_seq(int bits)`

Capture-based builder for uncontrolled toffoli QQ multiplication.

```
Qubit layout: [0..n-1] result, [n..2n-1] a, [2n..3n-1] b
```

Steps:
1. Validate `bits` (1-64)
2. Create capture context with headroom
3. Set arith mode to `QC_ARITH_TOFFOLI`
4. Allocate `3n` register qubits
5. Build result[], a[], b[] arrays
6. Call `qc_toffoli_qq_mul(ctx, result, n, a, n, b, n)`
7. Capture to sequence, set `total_qubits`, destroy ctx

Pattern source: `cmul_sequences.c::qc_c_arith_qq_mul_seq` minus the ext_ctrl qubit.

**Includes**: `capture_helpers.h` (for capture functions), `internal.h` (for `qc_toffoli_*` declarations)

**Estimated size**: ~120 lines (well under 500-line limit)

**Test**: Build library, call each function for bits=4, verify non-NULL return, verify `total_qubits > 0`, verify `total_gate_count > 0`.

## Step 2: Toffoli CQ Comparison Sequence Builders

**File**: `circuit-c-backend/src/cq_comparison_toffoli.c` (new, ~200 lines)

**Functions**:

### `qc_cmp_cq_less_toffoli_seq(int bits, int64_t value)`

Capture-based toffoli CQ less-than comparison.

```
Qubit layout: [0] result, [1..n] A, [n+1] borrow_ancilla
```

Algorithm (borrow-detect via extended subtraction):
1. Validate `bits` (1-63)
2. Create capture context, allocate `n+2` register qubits
3. Build target array `a_ext[0..n]` where `a_ext[i] = i+1` for `i < n` and `a_ext[n] = n+1` (borrow)
4. Step 1: `[A, borrow] -= value` via `qc_dynamic_cq_add(ctx, a_ext, n+1, -value)`
5. Step 2: `CX(borrow -> result)` via `qc_circuit_cx(ctx, n+1, 0)`
6. Step 3: `[A, borrow] += value` via `qc_dynamic_cq_add(ctx, a_ext, n+1, value)` (restore)
7. Capture to sequence, set `total_qubits`, destroy ctx

Key insight: CQ subtraction = CQ addition with negated value. The `qc_dynamic_cq_add` function handles two's complement encoding internally, so passing `-value` produces the correct subtraction. The borrow qubit at position `n` of the extended register captures the borrow/carry from the subtraction.

### `qc_cmp_cq_greater_toffoli_seq(int bits, int64_t value)`

Delegates to `qc_cmp_cq_less_toffoli_seq(bits, value + 1)`. Returns empty sequence if `value >= max_val`.

### `qc_c_cmp_cq_less_toffoli_seq(int bits, int64_t value)`

Controlled variant of CQ less-than.

```
Qubit layout: [0] result, [1..n] A, [n+1] borrow, [n+2] control
```

Algorithm:
1. Same setup, but allocate `n+3` qubits (extra control at `n+2`)
2. Step 1: `[A, borrow] -= value` via `qc_dynamic_ccq_add(ctx, a_ext, n+1, -value, ctrl)`
3. Step 2: `CCX(ctrl, borrow, result)` via `qc_circuit_ccx(ctx, n+2, n+1, 0)`
4. Step 3: `[A, borrow] += value` via `qc_dynamic_ccq_add(ctx, a_ext, n+1, value, ctrl)` (restore)
5. Capture, set `total_qubits`, cleanup

### `qc_c_cmp_cq_greater_toffoli_seq(int bits, int64_t value)`

Delegates to `qc_c_cmp_cq_less_toffoli_seq(bits, value + 1)`. Returns empty sequence if `value >= max_val`.

**Estimated size**: ~200 lines (well under 500-line limit)

**Test**: Build library, call each function for bits=4 with value=3, verify non-NULL, verify `total_qubits > 0`, verify only Clifford+T gates (no phase rotations).

## Step 3: Header Declarations

**File**: `circuit-c-backend/include/quantum_circuit.h`

Add two new sections after the existing toffoli QQ comparison declarations:

```c
/* -- Toffoli multiplication sequence builders (capture-based) -------------- */

QC_API qc_sequence_t *qc_toffoli_cq_mul_seq(int bits, int64_t value);
QC_API qc_sequence_t *qc_toffoli_qq_mul_seq(int bits);

/* -- Toffoli CQ comparison sequence builders ------------------------------- */

QC_API qc_sequence_t *qc_cmp_cq_less_toffoli_seq(int bits, int64_t value);
QC_API qc_sequence_t *qc_cmp_cq_greater_toffoli_seq(int bits, int64_t value);
QC_API qc_sequence_t *qc_c_cmp_cq_less_toffoli_seq(int bits, int64_t value);
QC_API qc_sequence_t *qc_c_cmp_cq_greater_toffoli_seq(int bits, int64_t value);
```

**Test**: Header compiles cleanly when included from a test file.

## Step 4: CMake Integration

**File**: `circuit-c-backend/CMakeLists.txt`

Add to the source file list:
- `src/toffoli_mul_sequences.c`
- `src/cq_comparison_toffoli.c`

**Test**: `cmake -B build && cmake --build build` succeeds with no warnings.

## Step 5: C Smoke Tests

**File**: `circuit-c-backend/tests/test_toffoli_seq_builders.c` (new, ~150 lines)

Tests:
1. `test_toffoli_cq_mul_seq` -- bits=4, value=3: returns non-NULL, total_qubits >= 8, total_gate_count > 0
2. `test_toffoli_qq_mul_seq` -- bits=4: returns non-NULL, total_qubits >= 12, total_gate_count > 0
3. `test_toffoli_cq_less_seq` -- bits=4, value=5: returns non-NULL, total_qubits >= 6, total_gate_count > 0
4. `test_toffoli_cq_greater_seq` -- bits=4, value=5: non-NULL
5. `test_controlled_cq_less_seq` -- bits=4, value=5: non-NULL, total_qubits >= 7
6. `test_invalid_inputs` -- bits=0, bits=65: returns NULL for all functions
7. `test_edge_cq_greater_max` -- value=max: returns empty sequence

**Test**: `ctest --output-on-failure` passes all tests.

## Execution Order

Steps 1-4 can be implemented in a single pass (the files are independent and small). Step 5 follows immediately after to verify correctness.

## Downstream Work (quantum-cython-types, NOT planned here)

After these C functions are implemented and tested:
1. Add `.pxd` Cython declarations for all 6 new `_seq()` functions
2. Add Python wrapper functions
3. Add dispatch table entries:
   - `toffoli_mul_cq` -> `qc_toffoli_cq_mul_seq`
   - `toffoli_mul_qq` -> `qc_toffoli_qq_mul_seq`
   - `toffoli_cmp_cq_less` -> `qc_cmp_cq_less_toffoli_seq`
   - `toffoli_cmp_cq_greater` -> `qc_cmp_cq_greater_toffoli_seq`
   - `toffoli_c_cmp_cq_less` -> `qc_c_cmp_cq_less_toffoli_seq`
   - `toffoli_c_cmp_cq_greater` -> `qc_c_cmp_cq_greater_toffoli_seq`
4. Update `_qint_compare.py` to select toffoli CQ comparison ops when `fault_tolerant=True`
5. Update `func.py` to select toffoli mul ops when `fault_tolerant=True`
