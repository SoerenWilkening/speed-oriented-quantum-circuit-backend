# Implementation Plan: Sequence Total Qubit Count Metadata

## Status: DONE (2026-04-02)

All 4 steps complete. `total_qubits` field added to `qc_sequence_t`, set in all ~30 sequence builders, 31 C tests pass. Follow-up: refactor-uln (use this in quantum-cython-types _ir_helpers.py), refactor-4xn (split integer_comparison.c).

PRD: `PRD_sequence_qubit_count.md`

## Overview

4 steps, each independently testable. Total estimated change: ~80 lines of C + ~200 lines of test code.

---

## Step 1: Add `total_qubits` field, public query function, and fix malloc allocators

**Goal**: Extend `qc_sequence_t` with a `total_qubits` field, expose it via a public API function, and ensure all allocators initialize the field to 0.

**Files modified**:
- `src/internal.h` -- Add `uint32_t total_qubits;` to `struct qc_sequence`
- `include/quantum_circuit.h` -- Add `QC_API uint32_t qc_sequence_total_qubits(const qc_sequence_t *seq);` declaration
- `src/execution.c` -- Add implementation of `qc_sequence_total_qubits()`
- `src/qft_addition.c` -- Add `seq->total_qubits = 0;` in `qft_sequence_alloc()` after `malloc`
- `src/qft_multiplication.c` -- Add `seq->total_qubits = 0;` in `qc_mul_sequence_alloc()` after `malloc`
- `src/divmod_sequences.c` -- Add `seq->total_qubits = 0;` in `capture_circuit_to_sequence()` after `malloc`
- `src/cmul_sequences.c` -- Add `seq->total_qubits = 0;` in `cmul_capture_circuit_to_sequence()` after `malloc`

**Changes** (~30 lines):

In `src/internal.h`, add to `struct qc_sequence`:
```c
uint32_t total_qubits;       /**< Total virtual qubits (register + ancillae), 0 = unknown */
```

In `include/quantum_circuit.h`, add after `qc_sequence_gate_count`:
```c
/** @brief Get total virtual qubit count of a sequence (0 if NULL or unknown). */
QC_API uint32_t qc_sequence_total_qubits(const qc_sequence_t *seq);
```

In `src/execution.c`, add:
```c
QC_API uint32_t qc_sequence_total_qubits(const qc_sequence_t *seq) {
    if (seq == NULL) return 0;
    return seq->total_qubits;
}
```

In each malloc-based allocator, add immediately after `malloc(sizeof(qc_sequence_t))` and the NULL check:
```c
seq->total_qubits = 0;
```

**Why fix allocators here, not in Step 2?** The field must be initialized to 0 before any builder can set it. Builders that do not explicitly set `total_qubits` (e.g., legacy or stub paths) rely on the allocator having zeroed it. The malloc-based allocators (`qft_sequence_alloc`, `qc_mul_sequence_alloc`, `capture_circuit_to_sequence`, `cmul_capture_circuit_to_sequence`) leave the field as garbage without this fix. The calloc-based allocators (`alloc_sequence`, `logic_alloc_seq`, `hp_alloc_seq`, `qc_sequence_alloc`) already zero-initialize.

**Backward compatibility**: The field initializes to 0 meaning "unknown/legacy". The query function returns 0 for legacy sequences.

**Test**: Build any existing sequence, call `qc_sequence_total_qubits()` -- should return 0. Verify it compiles and links.

**Estimated size**: ~30 lines changed

---

## Step 2: Set `total_qubits` in non-capture sequence builders

**Goal**: Every non-capture builder sets `total_qubits` to the number of distinct virtual qubit indices it uses, including any AND-ancilla from MCX decomposition.

**Files modified** (5 files, ~30 lines total):
- `src/qft_addition.c` (4 builders)
- `src/qft_multiplication.c` (2 builders)
- `src/integer_comparison.c` (6 active builders, 2 stubs skipped)
- `src/logic_operations.c` (8 builders)
- `src/hot_path_add.c` (2 builders)

**Pattern**: After sequence construction (after `qc_sequence_compute_total_gate_count(seq)`), add:
```c
seq->total_qubits = <value>;
```

### Complete builder list with values

**`src/qft_addition.c`** -- `qft_sequence_alloc` uses malloc:

| Builder | Expression | Example (bits=4) |
|---------|-----------|-------------------|
| `qc_arith_qq_add_seq(bits)` | `2 * bits` | 8 |
| `qc_arith_cq_add_seq(bits, value)` | `bits` | 4 |
| `qc_arith_cqq_add_seq(bits)` | `2 * bits + 1` | 9 |
| `qc_arith_ccq_add_seq(bits, value)` | `bits + 1` | 5 |

**`src/qft_multiplication.c`** -- `qc_mul_sequence_alloc` uses malloc:

| Builder | Expression | Example (bits=4) |
|---------|-----------|-------------------|
| `qc_arith_cq_mul_seq(bits, value)` | `2 * bits` | 8 |
| `qc_arith_qq_mul_seq(bits)` | `3 * bits` | 12 |

**`src/integer_comparison.c`** -- `alloc_sequence` uses calloc:

| Builder | Expression | Notes |
|---------|-----------|-------|
| `qc_cmp_cq_equal_seq(bits, value)` | `bits < 3 ? bits + 1 : 2 * bits - 1` | [0]=result, [1..bits]=A; for bits>=3: MCX decomp adds bits-2 AND-ancilla at [bits+1..2*bits-2] |
| `qc_cmp_cq_less_seq(bits, value)` | `bits + 2` | [0]=result, [1..bits]=A (offset from sub-seq), [bits+1]=borrow. Sub-sequences are copied with +1 qubit offset; they use indices 1..bits+1, which maps within the bits+2 range. |
| `qc_cmp_cq_greater_seq(bits, value)` | `bits + 2` | Delegates to `qc_cmp_cq_less_seq(bits, value+1)` |
| `qc_c_cmp_cq_equal_seq(bits, value)` | `bits == 1 ? bits + 2 : 2 * bits + 1` | [0]=result, [1..bits]=A, [bits+1]=ctrl; for bits>=2: MCX(bits+1) decomp adds bits-1 AND-ancilla at [bits+2..2*bits] |
| `qc_c_cmp_cq_less_seq(bits, value)` | `bits + 3` | [0]=result, [1..bits]=A, [bits+1]=borrow, [bits+2]=ctrl |
| `qc_c_cmp_cq_greater_seq(bits, value)` | `bits + 3` | Delegates to `qc_c_cmp_cq_less_seq(bits, value+1)` |
| `qc_cmp_qq_less_seq(bits)` | skip | Stub, returns NULL |
| `qc_c_cmp_qq_less_seq(bits)` | skip | Stub, returns NULL |

**Audit of `qc_cmp_cq_less_seq` composition** (Issue 3): This builder constructs `qc_split_cq_sub_seq(bits, value)` and `qc_split_cq_add_seq(bits, value)`, which operate on `bits+1` qubits (indices 0..bits). The gates are copied with a +1 offset on all qubit indices, so they reference indices 1..bits+1. The result qubit at index 0 and the borrow qubit at index bits+1 are explicitly used by the CX gate in between. Total distinct indices: 0 through bits+1, giving `bits + 2`. No qubits beyond this range are used.

**`src/logic_operations.c`** -- `logic_alloc_seq` uses calloc:

| Builder | Expression | Example (bits=4) |
|---------|-----------|-------------------|
| `qc_not_seq(bits)` | `bits` | 4 |
| `qc_xor_seq(bits)` | `2 * bits` | 8 |
| `qc_and_seq(bits)` | `3 * bits` | 12 |
| `qc_or_seq(bits)` | `3 * bits` | 12 |
| `qc_c_not_seq(bits)` | `bits + 1` | 5 |
| `qc_c_xor_seq(bits)` | `2 * bits + 1` | 9 |
| `qc_c_and_seq(bits)` | `3 * bits + 1` | 13 |
| `qc_c_or_seq(bits)` | `3 * bits + 1` | 13 |

**`src/hot_path_add.c`** -- `hp_alloc_seq` uses calloc:

| Builder | Expression | Example (bits=4) |
|---------|-----------|-------------------|
| `qc_split_cq_add_seq(bits, value)` | `bits + 1` | 5 |
| `qc_split_cq_sub_seq(bits, value)` | `bits + 1` | 5 (delegates to add_seq, inherits its total_qubits) |

**Skipped** (stubs returning NULL, no sequence produced):
- `src/toffoli_cla.c`: `qc_toffoli_qq_add_ks_seq`, `qc_toffoli_cq_add_ks_seq`, `qc_toffoli_cqq_add_ks_seq`, `qc_toffoli_ccq_add_ks_seq`
- `src/integer_comparison.c`: `qc_cmp_qq_less_seq`, `qc_c_cmp_qq_less_seq`

**Skipped** (static/cached, internal-only):
- `src/toffoli_cla.c`: `build_qq_add_bk`, `build_cq_add_bk`, `build_cqq_add_bk` -- public wrapper `qc_toffoli_qq_add_bk` manages ancilla and mapping internally

**Test**: Build `qc_arith_qq_add_seq(4)`, verify `qc_sequence_total_qubits()` returns 8. Build `qc_cmp_cq_equal_seq(4, 5)`, verify returns 7.

**Estimated size**: ~30 lines changed (one line per active builder)

**Dependencies**: Step 1

---

## Step 3: Set `total_qubits` in capture-based builders

**Goal**: Capture-based builders query the temporary circuit's allocator high-water mark and store it in the sequence.

**Files modified**:
- `src/divmod_sequences.c` -- Set `total_qubits` from temp ctx before destroying it (4 builders)
- `src/cmul_sequences.c` -- Set `total_qubits` from temp ctx before destroying it (2 builders)

**Changes** (~20 lines):

In `capture_circuit_to_sequence()` in `divmod_sequences.c`, add before `return seq;`:
```c
seq->total_qubits = ctx->allocator->next_qubit;  /* <-- caller passes ctx */
```

However, `capture_circuit_to_sequence` currently does not receive the total qubit count. Two options:
1. Add a `uint32_t total_qubits` parameter to `capture_circuit_to_sequence()`.
2. Set `seq->total_qubits` in each builder after `capture_circuit_to_sequence()` returns.

Option 2 is simpler and avoids changing the helper's signature:

In each of the 4 builders in `divmod_sequences.c`, after `capture_circuit_to_sequence(ctx)`:
```c
qc_sequence_t *seq = capture_circuit_to_sequence(ctx);
if (seq) seq->total_qubits = ctx->allocator->next_qubit;
qc_circuit_destroy(ctx);
return seq;
```

Same pattern for each of the 2 builders in `cmul_sequences.c`:
```c
qc_sequence_t *seq = cmul_capture_circuit_to_sequence(ctx);
if (seq) seq->total_qubits = ctx->allocator->next_qubit;
qc_circuit_destroy(ctx);
return seq;
```

The allocator's `next_qubit` field is the high-water mark: it equals the total number of qubit indices that have ever been allocated (register + ancilla). This is exactly the total virtual qubit count needed for a complete mapping.

**Affected builders (6 total)**:
| Builder | File | Register qubits |
|---------|------|-----------------|
| `qc_divmod_cq_seq(bits, value)` | `divmod_sequences.c` | 3n |
| `qc_divmod_qq_seq(bits)` | `divmod_sequences.c` | 4n |
| `qc_c_divmod_cq_seq(bits, value)` | `divmod_sequences.c` | 3n+1 |
| `qc_c_divmod_qq_seq(bits)` | `divmod_sequences.c` | 4n+1 |
| `qc_c_arith_cq_mul_seq(bits, value)` | `cmul_sequences.c` | 2n+1 |
| `qc_c_arith_qq_mul_seq(bits)` | `cmul_sequences.c` | 3n+1 |

**Test**: Build `qc_divmod_cq_seq(5, 3)`, verify `qc_sequence_total_qubits()` returns a value > 15 (the 15 register qubits). Build `qc_c_arith_cq_mul_seq(5, 3)`, verify `qc_sequence_total_qubits()` returns a value > 11 (the 11 register qubits).

**Estimated size**: ~20 lines changed

**Dependencies**: Step 1

---

## Step 4: Tests

**Goal**: Comprehensive test coverage for the new metadata.

**Files created**:
- `tests/test_sequence_qubit_count.c` -- C unit tests

**Test cases**:

1. **NULL safety**: `qc_sequence_total_qubits(NULL)` returns 0.

2. **Non-capture QFT addition**:
   - `qc_arith_qq_add_seq(4)` returns `total_qubits == 8`.
   - `qc_arith_cq_add_seq(4, 5)` returns `total_qubits == 4`.
   - `qc_arith_cqq_add_seq(4)` returns `total_qubits == 9`.
   - `qc_arith_ccq_add_seq(4, 5)` returns `total_qubits == 5`.

3. **Non-capture QFT multiplication**:
   - `qc_arith_cq_mul_seq(4, 3)` returns `total_qubits == 8`.
   - `qc_arith_qq_mul_seq(4)` returns `total_qubits == 12`.

4. **Non-capture comparison**:
   - `qc_cmp_cq_equal_seq(4, 5)` returns `total_qubits == 7` (result + 4 A + 2 AND-ancilla).
   - `qc_cmp_cq_equal_seq(2, 1)` returns `total_qubits == 3` (result + 2 A, no ancilla).
   - `qc_cmp_cq_less_seq(4, 5)` returns `total_qubits == 6` (result + 4 A + borrow).
   - `qc_cmp_cq_greater_seq(4, 3)` returns `total_qubits == 6`.
   - `qc_c_cmp_cq_equal_seq(4, 5)` returns `total_qubits == 9` (result + 4 A + ctrl + 3 AND-ancilla).
   - `qc_c_cmp_cq_less_seq(4, 5)` returns `total_qubits == 7` (result + 4 A + borrow + ctrl).
   - `qc_c_cmp_cq_greater_seq(4, 3)` returns `total_qubits == 7`.

5. **Non-capture bitwise**:
   - `qc_not_seq(4)` returns `total_qubits == 4`.
   - `qc_xor_seq(4)` returns `total_qubits == 8`.
   - `qc_and_seq(4)` returns `total_qubits == 12`.
   - `qc_or_seq(4)` returns `total_qubits == 12`.
   - `qc_c_not_seq(4)` returns `total_qubits == 5`.
   - `qc_c_xor_seq(4)` returns `total_qubits == 9`.
   - `qc_c_and_seq(4)` returns `total_qubits == 13`.
   - `qc_c_or_seq(4)` returns `total_qubits == 13`.

6. **Non-capture hot-path add**:
   - `qc_split_cq_add_seq(4, 3)` returns `total_qubits == 5`.
   - `qc_split_cq_sub_seq(4, 3)` returns `total_qubits == 5`.

7. **Capture divmod**:
   - `qc_divmod_cq_seq(5, 3)` returns `total_qubits > 15`.
   - `qc_divmod_qq_seq(5)` returns `total_qubits > 20`.
   - `qc_c_divmod_cq_seq(5, 3)` returns `total_qubits > 16`.
   - `qc_c_divmod_qq_seq(5)` returns `total_qubits > 21`.

8. **Capture controlled mul**:
   - `qc_c_arith_cq_mul_seq(5, 3)` returns `total_qubits > 11`.
   - `qc_c_arith_qq_mul_seq(5)` returns `total_qubits > 16`.

9. **Replay with correct mapping**: Build `qc_divmod_cq_seq(3, 2)`, get `total_qubits`, create a real circuit, allocate `total_qubits` qubits, build a 1:1 identity mapping, replay with `qc_run_instruction()`, verify `qc_circuit_width()` equals `total_qubits` (no huge qubit indices from unmapped ancilla).

**Estimated size**: ~200 lines

**Dependencies**: Steps 1, 2, 3

---

## Dependency Graph

```
Step 1 (struct + API + malloc fixes)
  ├── Step 2 (non-capture builders)
  ├── Step 3 (capture builders)
  └── Step 4 (tests) -- depends on 1, 2, 3
```

Steps 2 and 3 are independent of each other and can be done in either order or in parallel. Step 4 depends on all prior steps.

## Files Summary

| File | Action | Steps |
|------|--------|-------|
| `src/internal.h` | Modify | 1 |
| `include/quantum_circuit.h` | Modify | 1 |
| `src/execution.c` | Modify | 1 |
| `src/qft_addition.c` | Modify | 1, 2 |
| `src/qft_multiplication.c` | Modify | 1, 2 |
| `src/integer_comparison.c` | Modify | 2 |
| `src/logic_operations.c` | Modify | 2 |
| `src/hot_path_add.c` | Modify | 2 |
| `src/divmod_sequences.c` | Modify | 1, 3 |
| `src/cmul_sequences.c` | Modify | 1, 3 |
| `tests/test_sequence_qubit_count.c` | Create | 4 |

## Sibling Package Issues (to file after approval)

1. **quantum-cython-types**: Add `qc_sequence_total_qubits` to `_c_backend.pxd`; update `_ir_helpers.py` to allocate ancilla qubits and build complete mappings using the new metadata.
2. **quantum-core**: Expose `qc_sequence_total_qubits()` through `BackendBridge` for Python-level callers.
