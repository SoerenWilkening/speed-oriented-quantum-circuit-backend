# PRD: Controlled Division and QQ Multiplication for circuit-c-backend

## Goal

Add controlled variants of division (`cdivmod_cq`, `cdivmod_qq`) and multiplication (`cmul_qq`, `cmul_cq`) to the C backend, producing structurally equivalent circuits to the monolith (same gate types, counts, and topology modulo qubit index remapping). These are required for compile-mode sequence builders and for `with qbool:` blocks that wrap division or multiplication operations.

## Background

The refactored `circuit-c-backend` has uncontrolled variants of all four operations (`qc_toffoli_divmod_cq`, `qc_toffoli_divmod_qq`, `qc_toffoli_qq_mul`, `qc_toffoli_cq_mul`) but lacks the externally-controlled versions. The monolith provides `toffoli_cdivmod_cq`, `toffoli_cdivmod_qq`, `toffoli_cmul_qq`, and `toffoli_cmul_cq`.

The controlled variants are needed because:
1. **`with qbool:` blocks** -- any arithmetic operation inside a conditional quantum block must be controlled by an external qubit. Without C-level controlled variants, the Python layer cannot delegate to the fast path.
2. **Compile-mode sequences** -- `quantum-core/sequences.py` currently maps `c_mul_cq` and `c_mul_qq` to the *uncontrolled* sequence builders (a documented workaround). Division has no dispatch entries at all.

## Requirements

### R1: Controlled QQ Multiplication (`qc_toffoli_cmul_qq`)

- Signature: `qc_error_t qc_toffoli_cmul_qq(circuit_ctx_t *ctx, const uint32_t *result, uint32_t result_bits, const uint32_t *a, uint32_t a_bits, const uint32_t *b, uint32_t b_bits, uint32_t ext_ctrl)`
- Algorithm: For each multiplier bit `b[j]`, compute `and_anc = b[j] AND ext_ctrl` via CCX, use `and_anc` as control for `qc_dynamic_cqq_add`, then uncompute AND (CCX is self-inverse).
- For width 1: decompose MCX(3 controls) via AND-ancilla pattern (3 CCX gates).
- Must be structurally equivalent to monolith `toffoli_cmul_qq` (same gate types, counts, and topology modulo qubit index remapping). Qubit indices may differ because `qc_qubit_alloc` assigns ancillae dynamically.

### R2: Controlled CQ Multiplication (`qc_toffoli_cmul_cq`)

- Signature: `qc_error_t qc_toffoli_cmul_cq(circuit_ctx_t *ctx, const uint32_t *result, uint32_t result_bits, const uint32_t *target, uint32_t target_bits, int64_t value, uint32_t ext_ctrl)`
- Algorithm: For each set bit `j` of classical value, perform controlled addition of `target[0..width-1]` into `result[j..j+width-1]`, controlled by `ext_ctrl`.
- For width 1: emit CCX(result[n-1], target[0], ext_ctrl).
- Must be structurally equivalent to monolith `toffoli_cmul_cq` (same gate types, counts, and topology modulo qubit index remapping).

### R3: Controlled CQ Division (`qc_toffoli_cdivmod_cq`)

- Signature: `qc_error_t qc_toffoli_cdivmod_cq(circuit_ctx_t *ctx, const uint32_t *dividend, uint32_t dividend_bits, int64_t divisor, const uint32_t *quotient, const uint32_t *remainder, uint32_t ext_ctrl)`
- Algorithm: Same restoring division as uncontrolled, but:
  - Initial copy uses CCX (controlled by ext_ctrl) instead of CX.
  - Comparison subtraction/addition uses `qc_dynamic_ccq_add` (doubly controlled by ext_ctrl).
  - Conditional subtract uses AND-ancilla pattern: `and_anc = cmp_anc AND ext_ctrl`, then `qc_dynamic_ccq_add(remainder, -trial, and_anc)`.
  - Division-by-zero sentinel is controlled.
- Must be structurally equivalent to monolith `toffoli_cdivmod_cq` (same gate types, counts, and topology modulo qubit index remapping).

### R4: Controlled QQ Division (`qc_toffoli_cdivmod_qq`)

- Signature: `qc_error_t qc_toffoli_cdivmod_qq(circuit_ctx_t *ctx, const uint32_t *dividend, uint32_t dividend_bits, const uint32_t *divisor, uint32_t divisor_bits, const uint32_t *quotient, const uint32_t *remainder, uint32_t ext_ctrl)`
- Algorithm: Same repeated-subtraction as uncontrolled, but:
  - Initial copy uses CCX.
  - Widened subtraction uses doubly-controlled QQ sub (AND-decomposed: `and_anc = cmp_anc AND ext_ctrl`).
  - Conditional subtract/quotient-increment uses AND-ancilla pattern.
- Must be structurally equivalent to monolith `toffoli_cdivmod_qq` (same gate types, counts, and topology modulo qubit index remapping).

### R5: Public Header Declarations

- All four new functions declared in `quantum_circuit.h` under the Toffoli Division / Multiplication sections.

### R6: C Tests

- Gate-count tests verifying controlled variants emit more gates than uncontrolled (the CCX overhead).
- Structural tests verifying NULL/zero-width error handling.
- QASM export round-trip tests for small widths (2-bit).

## Success Criteria

1. All four functions compile and pass C tests.
2. Gate type counts match the monolith for identical inputs (verified via QASM export gate-type counting, not literal string diff, since qubit indices may differ).
3. `toffoli_multiplication.c` stays under 500 lines after adding `cmul_qq` and `cmul_cq`.
4. `toffoli_division.c` stays under 500 lines after adding `cdivmod_cq` and `cdivmod_qq`.
5. No modifications to `Quantum_Assembly/`.

## Out of Scope

- **Cython wrappers**: Adding declarations to `_c_backend.pxd`/`.pyx` is a `quantum-cython-types` task. This PRD will note the sibling dependency.
- **Sequence dispatch entries**: Adding `divmod_cq`, `divmod_qq`, `c_divmod_cq`, `c_divmod_qq`, `c_mul_cq`, `c_mul_qq` to `quantum-core/sequences.py` is a `quantum-core` task.
- **QFT-mode controlled arithmetic**: Only Toffoli-mode is in scope.
- **Controlled modular arithmetic**: `cmod_reduce`, `cmod_add`, `cmod_mul` are separate.

## Sibling Package Dependencies

After this work lands, sibling packages need updates:

| Package | Change Needed |
|---------|---------------|
| `quantum-cython-types` | Add `qc_toffoli_cmul_qq`, `qc_toffoli_cmul_cq`, `qc_toffoli_cdivmod_cq`, `qc_toffoli_cdivmod_qq` to `.pxd` declarations and `.pyx` wrapper methods |
| `quantum-core` | Add sequence dispatch entries for `divmod_cq`, `divmod_qq`, `c_divmod_cq`, `c_divmod_qq`; update `c_mul_cq`/`c_mul_qq` to use real controlled builders |
