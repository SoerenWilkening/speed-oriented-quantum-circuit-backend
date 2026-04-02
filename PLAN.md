# Implementation Plan: Controlled Division and QQ Multiplication

## Overview

Add four controlled arithmetic functions to the C backend. The work is split into
three steps, each independently testable. Total estimated new code: ~350 lines
across two existing source files and one new test file.

Current file sizes:
- `toffoli_multiplication.c`: 474 lines (budget: 500 max -> ~26 lines headroom)
- `toffoli_division.c`: 240 lines (budget: 500 max -> ~260 lines headroom)

---

## Step 1: Controlled QQ and CQ Multiplication

**Files modified:**
- `circuit-c-backend/src/toffoli_multiplication.c` (add ~100 lines)
- `circuit-c-backend/include/quantum_circuit.h` (add 2 declarations)
- `circuit-c-backend/src/internal.h` (no change expected; dynamic helpers already exposed)

**Estimated lines added:** ~110 total

### 1a: `qc_toffoli_cmul_qq` (~60 lines)

Port of monolith `toffoli_cmul_qq` (ToffoliMultiplication.c:405-502). Key
translation from monolith to refactored API:

| Monolith | Refactored |
|----------|------------|
| `emit_ccx_or_clifford_t(circ, tgt, c1, c2, decompose)` | `qc_emit_ccx_or_decomp(ctx, tgt, c1, c2)` |
| `allocator_alloc(circ->allocator, 1, true)` | `qc_qubit_alloc(ctx, &qubit)` |
| `allocator_free(circ->allocator, q, 1)` | `qc_qubit_free(ctx, q)` |
| `toffoli_cQQ_add` sequence via `run_instruction` | `qc_dynamic_cqq_add(ctx, a, b, width, control)` |

Algorithm (for each multiplier bit b[j]):
1. Allocate `and_anc` qubit (reuse across loop iterations -- alloc before loop).
2. Compute `and_anc = b[j] AND ext_ctrl` via `qc_emit_ccx_or_decomp(ctx, and_anc, b[j], ext_ctrl)`.
3. Use `and_anc` as control for `qc_dynamic_cqq_add(ctx, a, &result[j], add_width, and_anc)`.
4. Uncompute AND: `qc_emit_ccx_or_decomp(ctx, and_anc, b[j], ext_ctrl)` (self-inverse).

Width-1 special case: Decompose MCX(3 controls: a[0], b[j], ext_ctrl) via
AND-ancilla pattern (3 CCX gates).

**Note on file size:** toffoli_multiplication.c will grow to ~574 lines, which
exceeds the 500-line limit. To stay within budget, the controlled multiplication
functions should be placed in a **new file** `toffoli_ctrl_multiplication.c`
(~110 lines). This file includes `internal.h` and has access to all the dynamic
helpers declared there. The existing `toffoli_multiplication.c` is unchanged.

**Note on CMakeLists.txt:** No CMakeLists.txt change is needed for the new source
file. The existing `file(GLOB ...)` pattern in CMakeLists.txt auto-picks up all
`src/*.c` files, so `toffoli_ctrl_multiplication.c` will be included automatically.

### 1b: `qc_toffoli_cmul_cq` (~40 lines)

Port of monolith `toffoli_cmul_cq` (ToffoliMultiplication.c:525-609). Simpler
than QQ because classical bit selection is compile-time; only `ext_ctrl` gates
each addition.

Algorithm (for each set bit j of classical value):
1. `qc_dynamic_cqq_add(ctx, target, &result[j], add_width, ext_ctrl)` -- the
   ext_ctrl directly serves as the single control.
2. Width-1 special case: `qc_emit_ccx_or_decomp(ctx, result[n-1], target[0], ext_ctrl)`.

### 1c: Header declarations

Add to `quantum_circuit.h` after the existing `qc_toffoli_cq_mul` declaration:

```c
QC_API qc_error_t qc_toffoli_cmul_qq(circuit_ctx_t *ctx, const uint32_t *result,
                                       uint32_t result_bits, const uint32_t *a,
                                       uint32_t a_bits, const uint32_t *b,
                                       uint32_t b_bits, uint32_t ext_ctrl);

QC_API qc_error_t qc_toffoli_cmul_cq(circuit_ctx_t *ctx, const uint32_t *result,
                                       uint32_t result_bits, const uint32_t *target,
                                       uint32_t target_bits, int64_t value,
                                       uint32_t ext_ctrl);
```

### Verification

```bash
cd circuit-c-backend && cmake -B build && cmake --build build
# Verify compilation succeeds
# Run Step 3 tests (or a quick smoke test)
```

### Test criteria (verified in Step 3)

- `qc_toffoli_cmul_qq` with 2-bit operands returns QC_OK and emits more gates
  than `qc_toffoli_qq_mul` (CCX overhead per multiplier bit).
- `qc_toffoli_cmul_cq` with 2-bit operands, value=3 returns QC_OK.
- NULL ctx, NULL registers, and zero-width all return appropriate error codes.

---

## Step 2: Controlled CQ and QQ Division

**Files modified:**
- `circuit-c-backend/src/toffoli_division.c` (add ~220 lines, total ~460)
- `circuit-c-backend/include/quantum_circuit.h` (add 2 declarations)

**Estimated lines added:** ~230 total

**500-line contingency:** toffoli_division.c is estimated to reach ~460 lines.
If the controlled functions exceed their estimates and push the file past 500 lines,
split the controlled variants into a new file `toffoli_ctrl_division.c` (mirroring
the `toffoli_ctrl_multiplication.c` split in Step 1).

### 2a: `qc_toffoli_cdivmod_cq` (~110 lines)

Port of monolith `toffoli_cdivmod_cq` (ToffoliDivision.c:856-946). Key
differences from uncontrolled `qc_toffoli_divmod_cq`:

1. **Controlled copy**: Replace `qc_circuit_cx(ctx, dividend[i], remainder[i])`
   with `qc_emit_ccx_or_decomp(ctx, remainder[i], dividend[i], ext_ctrl)`.

2. **Controlled comparison subtract/add**: Replace
   `qc_dynamic_cq_add(ctx, temp_arr, wide, -trial)` with
   `qc_dynamic_ccq_add(ctx, temp_arr, wide, -trial, ext_ctrl)`.

3. **Doubly-controlled conditional subtract**: The uncontrolled version uses
   `qc_dynamic_ccq_add(ctx, remainder, n, -trial, cmp_anc)`. The controlled
   version needs TWO controls (cmp_anc AND ext_ctrl). Use AND-ancilla pattern:
   - Allocate `and_anc`
   - `qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl)` -- compute AND
   - `qc_dynamic_ccq_add(ctx, remainder, n, -trial, and_anc)` -- controlled sub
   - `qc_circuit_cx(ctx, and_anc, quotient[k])` -- set quotient bit
   - `qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl)` -- uncompute AND
   - Free `and_anc`

4. **Controlled division-by-zero sentinel**: Replace CX with
   `qc_circuit_cx(ctx, ext_ctrl, quotient[i])` and CCX for remainder copy.

5. **Sign bit copy**: The `qc_circuit_cx(ctx, temp_arr[n], cmp_anc)` remains
   unconditional (Bennett's trick: the sign bit is already conditioned on
   ext_ctrl because the comparison was controlled).

6. **cmp_anc reset**: Same pattern as uncontrolled
   (`qc_circuit_cx(ctx, quotient[k], cmp_anc); qc_circuit_x(ctx, cmp_anc)`).

### 2b: `qc_toffoli_cdivmod_qq` (~100 lines)

Port of monolith `toffoli_cdivmod_qq` (ToffoliDivision.c:948-1038). Key
differences from uncontrolled:

1. **Controlled copy**: CCX instead of CX for initial dividend-to-remainder copy.

2. **Controlled comparison**: The widened subtraction
   `qc_dynamic_qq_sub(ctx, wide_div, temp_arr, wide)` becomes controlled.
   The monolith uses `div_cqq_add(circ, temp_arr, wide_div, wide, ext_ctrl, 1)`
   (controlled QQ subtract). Map to:
   `qc_dynamic_cqq_sub(ctx, wide_div, temp_arr, wide, ext_ctrl)`.

3. **Controlled uncompute**: `qc_dynamic_cqq_add(ctx, wide_div, temp_arr, wide, ext_ctrl)` for the add-back.

4. **Controlled copy/uncopy of remainder to temp**: CCX instead of CX.

5. **Doubly-controlled conditional subtract**: AND-ancilla pattern
   (same as controlled CQ division):
   - `qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl)`
   - `qc_dynamic_cqq_sub(ctx, divisor, remainder, n, and_anc)` -- controlled sub
   - `qc_dynamic_ccq_add(ctx, quotient, n, 1, and_anc)` -- controlled increment
   - Uncompute AND

6. **cmp_anc leak**: Same as uncontrolled QQ division -- cmp_anc is not freed.

### 2c: Header declarations

Add to `quantum_circuit.h` after the existing `qc_toffoli_divmod_qq` declaration:

```c
QC_API qc_error_t qc_toffoli_cdivmod_cq(circuit_ctx_t *ctx,
                                          const uint32_t *dividend,
                                          uint32_t dividend_bits, int64_t divisor,
                                          const uint32_t *quotient,
                                          const uint32_t *remainder,
                                          uint32_t ext_ctrl);

QC_API qc_error_t qc_toffoli_cdivmod_qq(circuit_ctx_t *ctx,
                                          const uint32_t *dividend,
                                          uint32_t dividend_bits,
                                          const uint32_t *divisor,
                                          uint32_t divisor_bits,
                                          const uint32_t *quotient,
                                          const uint32_t *remainder,
                                          uint32_t ext_ctrl);
```

### Verification

```bash
cmake --build build
# Verify compilation succeeds
# Run Step 3 tests
```

### Test criteria (verified in Step 3)

- `qc_toffoli_cdivmod_cq` with 2-bit dividend, divisor=2 returns QC_OK.
- `qc_toffoli_cdivmod_cq` with divisor=0 returns QC_ERR_DIVISOR and emits
  controlled sentinel gates (more gates than uncontrolled sentinel).
- `qc_toffoli_cdivmod_qq` with 2-bit operands returns QC_OK.
- Gate counts for controlled > gate counts for uncontrolled (same inputs).
- NULL/zero-width error handling works.

---

## Step 3: C Tests and QASM Verification

**Files created:**
- `circuit-c-backend/tests/test_ctrl_mul_div.c` (~200 lines)

**Files modified:**
- `circuit-c-backend/CMakeLists.txt` (add test target)

**Estimated lines added:** ~210 total

### Test cases

#### 3a: Controlled multiplication tests

1. **`test_cmul_qq_basic`**: Create 2-bit a, b, result registers + ext_ctrl.
   Call `qc_toffoli_cmul_qq`. Verify QC_OK, gate_count > 0.

2. **`test_cmul_qq_vs_uncontrolled`**: Same inputs, compare gate counts:
   controlled must emit strictly more gates than uncontrolled (extra CCX per
   multiplier bit for AND compute/uncompute).

3. **`test_cmul_cq_basic`**: 2-bit target, result, value=3, ext_ctrl.
   Verify QC_OK, gate_count > 0.

4. **`test_cmul_cq_zero_value`**: value=0 should emit zero gates (no set bits).

5. **`test_cmul_error_handling`**: NULL ctx, NULL registers, zero widths.

#### 3b: Controlled division tests

6. **`test_cdivmod_cq_basic`**: 2-bit dividend, divisor=2, quotient, remainder,
   ext_ctrl. Verify QC_OK.

7. **`test_cdivmod_cq_div_zero`**: divisor=0. Verify QC_ERR_DIVISOR. Gate count
   should be nonzero (controlled sentinel).

8. **`test_cdivmod_cq_vs_uncontrolled`**: Same inputs, controlled gate count >
   uncontrolled gate count.

9. **`test_cdivmod_qq_basic`**: 2-bit dividend and divisor. Verify QC_OK.
   (Note: QQ division is O(2^n) iterations, so keep width small.)

10. **`test_cdivmod_qq_vs_uncontrolled`**: Controlled > uncontrolled gate count.

11. **`test_cdivmod_error_handling`**: NULL ctx, NULL registers, zero widths.

#### 3c: QASM export smoke test

12. **`test_cmul_qq_qasm_export`**: 2-bit controlled QQ mul, export to QASM
    string, verify non-NULL and contains "ccx" or "cx" gates. Compare gate type
    counts (not literal QASM strings) since qubit indices may differ from the
    monolith due to dynamic ancilla allocation.

13. **`test_cdivmod_cq_qasm_export`**: 2-bit controlled CQ divmod, export to
    QASM, verify non-NULL. Compare gate type counts, not literal strings.

### Build integration

Add to `CMakeLists.txt`:
```cmake
add_executable(test_ctrl_mul_div tests/test_ctrl_mul_div.c)
target_link_libraries(test_ctrl_mul_div PRIVATE quantum_static m)
target_include_directories(test_ctrl_mul_div PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_ctrl_mul_div COMMAND test_ctrl_mul_div)
```

### Verification

```bash
cmake -B build && cmake --build build
cd build && ctest --output-on-failure -R ctrl_mul_div
```

### Test criteria

- All 13 test cases pass.
- Total test runtime < 10 seconds (QQ division at width 2 = 4 iterations).
- No memory errors under sanitizers (if available).

---

## Step 4: Update CLAUDE.md

**Files modified:**
- `circuit-c-backend/CLAUDE.md` (add 4 function signatures)

Add the four new public API function signatures to the "Toffoli-Based Arithmetic"
and "Toffoli Division and Modular Arithmetic" sections of `CLAUDE.md` so downstream
consumers can discover them:

```c
// Under "Toffoli-Based Arithmetic":
qc_error_t qc_toffoli_cmul_qq(ctx, const uint32_t *result, uint32_t result_bits,
                               const uint32_t *a, uint32_t a_bits,
                               const uint32_t *b, uint32_t b_bits, uint32_t ext_ctrl);
qc_error_t qc_toffoli_cmul_cq(ctx, const uint32_t *result, uint32_t result_bits,
                               const uint32_t *target, uint32_t target_bits,
                               int64_t value, uint32_t ext_ctrl);

// Under "Toffoli Division and Modular Arithmetic":
qc_error_t qc_toffoli_cdivmod_cq(ctx, const uint32_t *dividend, uint32_t dividend_bits,
                                  int64_t divisor, const uint32_t *quotient,
                                  const uint32_t *remainder, uint32_t ext_ctrl);
qc_error_t qc_toffoli_cdivmod_qq(ctx, const uint32_t *dividend, uint32_t dividend_bits,
                                  const uint32_t *divisor, uint32_t divisor_bits,
                                  const uint32_t *quotient, const uint32_t *remainder,
                                  uint32_t ext_ctrl);
```

### Verification

Review the updated CLAUDE.md to confirm all four signatures are present and correctly
placed in the appropriate sections.

---

## Summary

| Step | Description | New file? | Files modified | Est. lines |
|------|-------------|-----------|----------------|------------|
| 1 | Controlled QQ/CQ multiplication | `toffoli_ctrl_multiplication.c` (new) | `quantum_circuit.h` | ~110 |
| 2 | Controlled CQ/QQ division | No | `toffoli_division.c`, `quantum_circuit.h` | ~230 |
| 3 | C tests + QASM verification | `test_ctrl_mul_div.c` (new) | `CMakeLists.txt` | ~210 |
| 4 | Update CLAUDE.md | No | `CLAUDE.md` | ~10 |

**Total:** ~560 lines of new code across 2 new files and 3 modified files.

## Dependencies

- Step 1 and Step 2 are independent (multiplication and division are in separate files).
- Step 3 depends on both Steps 1 and 2.
- Step 4 depends on Steps 1 and 2 (needs final function signatures).

## File Size Budget Check

| File | Current | After change | Limit | OK? |
|------|---------|--------------|-------|-----|
| `toffoli_multiplication.c` | 474 | 474 (unchanged) | 500 | Yes |
| `toffoli_ctrl_multiplication.c` | 0 (new) | ~110 | 500 | Yes |
| `toffoli_division.c` | 240 | ~460 | 500 | Yes |
| `test_ctrl_mul_div.c` | 0 (new) | ~210 | 500 | Yes |

## Sibling Package Follow-Up (not implemented here)

After this work lands, file issues for:

1. **quantum-cython-types**: Add `.pxd` declarations and `.pyx` wrappers for
   `qc_toffoli_cmul_qq`, `qc_toffoli_cmul_cq`, `qc_toffoli_cdivmod_cq`,
   `qc_toffoli_cdivmod_qq`.

2. **quantum-core**: Add sequence dispatch entries for `divmod_cq`, `divmod_qq`,
   `c_divmod_cq`, `c_divmod_qq`, `c_mul_cq` (real controlled), `c_mul_qq`
   (real controlled). Remove the workaround comments on lines 65-66 of
   `sequences.py`.
