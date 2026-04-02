# PRD: Toffoli Multiplication and CQ Comparison Sequence Builders

## Problem

Two gaps in the circuit-c-backend sequence builder coverage prevent the compile-mode pipeline from selecting toffoli-based circuits when `fault_tolerant=True`:

1. **Issue 4 -- Toffoli Multiplication**: No sequence builders exist for toffoli-based CQ and QQ multiplication. When `@compile` records `mul_cq` or `mul_qq`, the dispatch table can only find QFT-based `qc_arith_cq_mul_seq` / `qc_arith_qq_mul_seq`. There are no `qc_toffoli_cq_mul_seq` / `qc_toffoli_qq_mul_seq` counterparts.

2. **Issue 5 -- Toffoli CQ Comparisons**: CQ comparisons (`a < 5`, `a > 5`) always use QFT-based split subtraction (`qc_split_cq_sub_seq`), even with `fault_tolerant=True`. Only QQ comparisons have toffoli variants (`qc_cmp_qq_less_toffoli_seq`). No toffoli CQ comparison sequence builders exist.

## Scope (circuit-c-backend only)

This PRD covers the C sequence builder functions. The Cython wrappers and dispatch table entries in quantum-cython-types are out of scope and will be planned separately.

## Requirements

### R1: Toffoli CQ Multiplication Sequence Builder

- **Function**: `qc_toffoli_cq_mul_seq(int bits, int64_t value) -> qc_sequence_t*`
- **Qubit layout**: `[0..n-1]` result, `[n..2n-1]` target (source quantum register)
- **Behavior**: Captures gates from `qc_toffoli_cq_mul()` on a temporary circuit
- **Pattern**: Same capture pattern as `qc_c_arith_cq_mul_seq()` in `cmul_sequences.c`, but calls the uncontrolled `qc_toffoli_cq_mul` instead of the controlled variant, and does NOT include an ext_ctrl qubit
- **Must set**: `seq->total_qubits = ctx->allocator->next_qubit`
- **Error handling**: Return NULL on invalid bits (<=0 or >64) or allocation failure

### R2: Toffoli QQ Multiplication Sequence Builder

- **Function**: `qc_toffoli_qq_mul_seq(int bits) -> qc_sequence_t*`
- **Qubit layout**: `[0..n-1]` result, `[n..2n-1]` a, `[2n..3n-1]` b
- **Behavior**: Captures gates from `qc_toffoli_qq_mul()` on a temporary circuit
- **Pattern**: Same capture pattern as `qc_c_arith_qq_mul_seq()` in `cmul_sequences.c`, but calls uncontrolled `qc_toffoli_qq_mul` and does NOT include an ext_ctrl qubit
- **Must set**: `seq->total_qubits = ctx->allocator->next_qubit`

### R3: Toffoli CQ Less-Than Sequence Builder

- **Function**: `qc_cmp_cq_less_toffoli_seq(int bits, int64_t value) -> qc_sequence_t*`
- **Qubit layout**: `[0]` result, `[1..bits]` A, `[bits+1]` borrow_ancilla
- **Algorithm**: Same borrow-detect as `qc_cmp_cq_less_seq()`, but uses `qc_dynamic_cq_add`/`qc_dynamic_cq_sub` (CDKM-based) on the capture context instead of QFT-based `qc_split_cq_sub_seq`/`qc_split_cq_add_seq`
  - Step 1: `[A, borrow] -= value` via dynamic CQ subtraction on n+1 bits
  - Step 2: `CX(borrow -> result)`
  - Step 3: `[A, borrow] += value` via dynamic CQ addition on n+1 bits (restore)
- **Pattern**: Uses `capture_helpers.h` (same as `integer_comparison_toffoli.c`)
- **Must set**: `seq->total_qubits = ctx->allocator->next_qubit`

### R4: Toffoli CQ Greater-Than Sequence Builder

- **Function**: `qc_cmp_cq_greater_toffoli_seq(int bits, int64_t value) -> qc_sequence_t*`
- **Behavior**: Delegates to `qc_cmp_cq_less_toffoli_seq(bits, value + 1)` -- same pattern as existing `qc_cmp_cq_greater_seq`
- **Edge case**: If `value >= max_val`, return empty sequence (always false)

### R5: Controlled Variants

- **Function**: `qc_c_cmp_cq_less_toffoli_seq(int bits, int64_t value) -> qc_sequence_t*`
- **Qubit layout**: `[0]` result, `[1..bits]` A, `[bits+1]` borrow, `[bits+2]` control
- **Algorithm**: Same as R3, but uses `qc_dynamic_ccq_add`/`qc_dynamic_ccq_sub` (controlled CDKM), and CCX instead of CX for the borrow-to-result copy
- **Function**: `qc_c_cmp_cq_greater_toffoli_seq(int bits, int64_t value) -> qc_sequence_t*`
- **Behavior**: Delegates to `qc_c_cmp_cq_less_toffoli_seq(bits, value + 1)`

### R6: Header Declarations

All new functions must be declared in `include/quantum_circuit.h` with `QC_API` linkage, in appropriately commented sections.

### R7: CMake Integration

New `.c` source files must be added to the `CMakeLists.txt` source list.

## Non-Requirements

- No changes to existing working functions
- No Cython wrappers (quantum-cython-types responsibility)
- No dispatch table changes (quantum-cython-types responsibility)
- No changes to the QFT-based sequence builders

## Success Criteria

1. All new functions compile without warnings under `-Wall -Wextra`
2. Each function returns a valid `qc_sequence_t*` with correct `total_qubits`
3. Each function returns NULL on invalid input
4. The library builds successfully with the new source files
5. Gate counts from toffoli mul sequences are non-zero and use only Clifford+T gates (no phase rotations)
6. Gate counts from toffoli CQ comparison sequences use only Clifford+T gates

## Dependencies

- `qc_toffoli_cq_mul` and `qc_toffoli_qq_mul` (already exist in `toffoli_multiplication.c`)
- `qc_dynamic_cq_add`, `qc_dynamic_cq_sub` (declared as internal helpers, need to verify they handle the n+1 borrow extension)
- `qc_dynamic_ccq_add`, `qc_dynamic_ccq_sub` (controlled variants, need to verify existence -- `qc_dynamic_ccq_add` exists but `qc_dynamic_ccq_sub` may not)
- `capture_helpers.h` (shared capture infrastructure)

## Downstream: quantum-cython-types (not planned here)

After these C functions are implemented, quantum-cython-types needs:
1. Cython `.pxd` declarations for all new `_seq()` functions
2. Python wrappers that call the C functions
3. Dispatch table entries mapping `toffoli_mul_cq` -> `qc_toffoli_cq_mul_seq`, etc.
4. Dispatch table entries mapping `toffoli_cmp_cq_less` -> `qc_cmp_cq_less_toffoli_seq`, etc.
