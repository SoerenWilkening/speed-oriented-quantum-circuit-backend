# PRD: QQ Less-Than Comparison Sequence Builders

**Issue:** refactor-eap
**Package:** circuit-c-backend
**Status:** Draft

## Problem

The QQ comparison sequence builders in `src/integer_comparison.c` are stubs that return NULL:

- `qc_cmp_qq_less_seq(int bits)` -- returns NULL
- `qc_c_cmp_qq_less_seq(int bits)` -- returns NULL

This causes two failures:

1. **Controlled QQ comparisons are completely broken.** When the C backend returns NULL, the Python layer falls back to direct gate emission that bypasses the control stack, so `with ctrl: result = (a < b)` silently ignores the control qubit.

2. **Uncontrolled QQ comparisons produce wrong results for large unsigned values.** The Python fallback uses signed arithmetic (zero-extends both operands by 1 bit, then checks the MSB after addition), which gives incorrect results when either operand is >= 2^(w-1).

## Goal

Implement correct unsigned QQ less-than comparison as pre-built `qc_sequence_t` sequences, matching the algorithm used in the monolith (`QQ_less_than` / `cQQ_less_than` in `Quantum_Assembly/c_backend/src/IntegerComparison.c`).

## Success Criteria

1. `qc_cmp_qq_less_seq(bits)` returns a valid sequence (not NULL) for bits in [1, 63].
2. `qc_c_cmp_qq_less_seq(bits)` returns a valid sequence (not NULL) for bits in [1, 63].
3. Both sequences produce correct unsigned comparison results: result = (A < B) for all unsigned A, B in [0, 2^bits - 1].
4. The controlled variant respects the control qubit: result is set only when control = |1>.
5. `total_qubits` is set correctly on both sequences.
6. Both sequences restore input registers A and B to their original values (uncomputation is correct).
7. No source file exceeds 500 lines.

## Non-Goals

- QQ greater-than (can be built from QQ less-than by swapping operands at the caller level).
- QQ equality (already implemented via XOR+MCX pattern).
- Toffoli-based QQ comparison (the monolith uses QFT-based `QQ_add`; we use the same).
- Changes to sibling packages (quantum-cython-types, quantum-core).

## Algorithm

The monolith's proven algorithm (borrow-ancilla via extended QFT addition):

### Uncontrolled: result = (A < B) unsigned

Qubit layout: `[0]=result, [1..bits]=A, [bits+1..2*bits]=B, [2*bits+1]=borrow, [2*bits+2]=zero_ext`
Total qubits: `2*bits + 3`

Steps:
1. **A -= B** (apply `QQ_add(bits)` in inverse on A,B registers)
2. **[A,borrow] += [B,zero_ext]** (apply `QQ_add(bits+1)` forward, with A extended by borrow and B extended by zero_ext)
3. **CX(borrow -> result)** (copy borrow bit to result)
4. **Undo step 2** (apply `QQ_add(bits+1)` inverse)
5. **A += B** (apply `QQ_add(bits)` forward, restoring A)

The borrow bit is set iff A < B unsigned, because:
- After step 1, A holds (A - B) mod 2^n
- Step 2 adds back B on the extended register; if A < B, the original subtraction wrapped, and the re-addition of B on the extended register detects this wrap via the (n+1)-th bit (borrow).

### Controlled: result = control ? (A < B) : 0

Qubit layout: `[0]=result, [1..bits]=A, [bits+1..2*bits]=B, [2*bits+1]=borrow, [2*bits+2]=zero_ext, [2*bits+3]=control`
Total qubits: `2*bits + 4`

Same algorithm but using controlled QQ_add sequences (`cQQ_add`), and step 3 becomes CCX(result, borrow, control).

## Approach: Capture-Based Sequence Construction

Rather than reimplementing the monolith's `copy_remap_layers` / `copy_remap_layers_inverse` infrastructure, use the **capture-based pattern** already established in `divmod_sequences.c` and `cmul_sequences.c`:

1. Create a temporary `circuit_ctx_t` with virtual qubit indices.
2. Run the comparison operation using existing `qc_arith_qq_add` / `qc_arith_cqq_add` and `qc_circuit_reverse_range` (for inverse steps).
3. Capture the emitted gates into a `qc_sequence_t`.
4. Destroy the temporary circuit.

This approach:
- Reuses existing, tested arithmetic functions directly
- Avoids duplicating sequence-remapping logic
- Follows the established pattern in the codebase
- Produces correct `total_qubits` via the allocator's `next_qubit`

## Qubit Layout Contract

### Uncontrolled (`qc_cmp_qq_less_seq`)
| Index | Role |
|-------|------|
| 0 | result (qbool) |
| 1..bits | A register |
| bits+1..2*bits | B register |
| 2*bits+1 | borrow ancilla |
| 2*bits+2 | zero extension |

### Controlled (`qc_c_cmp_qq_less_seq`)
| Index | Role |
|-------|------|
| 0 | result (qbool) |
| 1..bits | A register |
| bits+1..2*bits | B register |
| 2*bits+1 | borrow ancilla |
| 2*bits+2 | zero extension |
| 2*bits+3 | control qubit |

## Files Changed

| File | Change |
|------|--------|
| `src/integer_comparison.c` | Replace `qc_cmp_qq_less_seq` stub with capture-based implementation |
| `src/integer_comparison_ctrl.c` | Replace `qc_c_cmp_qq_less_seq` stub with capture-based implementation (moved here if line limit requires) |
| `src/comparison_internal.h` | Add shared capture helper declarations if needed |
| `tests/test_qq_comparison.c` | New test file for QQ comparison correctness |

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| `integer_comparison.c` exceeds 500 lines | Move controlled variant to `integer_comparison_ctrl.c` (it already hosts controlled CQ variants) |
| Capture-based approach produces more ancilla qubits than direct construction | Acceptable -- `total_qubits` is reported and callers handle mapping. Divmod already uses this pattern successfully. |
| QFT arithmetic sequences not available for width 1 | Add edge-case test; may need special handling for bits=1 |
| `qc_circuit_reverse_range` may not perfectly invert QFT rotations | Verify via test that A and B are restored after the full sequence |
