# PRD: Sequence Total Qubit Count Metadata

## Status: COMPLETE (2026-04-02)

All requirements met. `qc_sequence_total_qubits()` API works for all sequence types. 31 C tests pass. Downstream integration (quantum-cython-types _ir_helpers.py) tracked as refactor-uln.

## Goal

Every sequence builder must report the total number of virtual qubits used -- including internal ancillae -- so that callers can construct complete qubit mappings and avoid out-of-bounds memory access during replay.

## Motivation

When `qc_run_instruction()` replays a sequence, it maps each gate's virtual qubit indices through a caller-provided `qubit_array[]`. The caller currently only provides mapping entries for the register qubits (e.g., 3n for CQ divmod). However, capture-based sequences contain gates referencing internal ancilla qubits with indices beyond the register range. These unmapped indices pass through as raw values, causing the circuit to expand to thousands of qubits and triggering OOM.

### Concrete Example (CQ divmod, n=5)

| Qubit role       | Virtual indices | Count |
|------------------|-----------------|-------|
| Dividend         | 0-4             | 5     |
| Quotient         | 5-9             | 5     |
| Remainder        | 10-14           | 5     |
| Internal ancillae| 15-30+          | ~16   |

The caller provides a 15-entry mapping. Gates referencing indices 15+ read past the array bounds, producing garbage qubit positions.

## Requirements

### R1: Add `total_qubits` field to `qc_sequence_t`

Add a `uint32_t total_qubits;` field to `struct qc_sequence` in `internal.h`. This records the total number of distinct virtual qubit indices used in the sequence (register qubits + ancillae).

### R2: Public query function

Add a public API function:
```c
QC_API uint32_t qc_sequence_total_qubits(const qc_sequence_t *seq);
```
Returns `seq->total_qubits` or 0 if seq is NULL.

### R3: Fix allocators that use malloc

The following allocators use `malloc` for the `qc_sequence_t` struct itself, leaving `total_qubits` uninitialized (garbage):

- `qft_sequence_alloc()` in `src/qft_addition.c`
- `qc_mul_sequence_alloc()` in `src/qft_multiplication.c`
- `capture_circuit_to_sequence()` in `src/divmod_sequences.c`
- `cmul_capture_circuit_to_sequence()` in `src/cmul_sequences.c`

**Fix**: Add `seq->total_qubits = 0;` immediately after every `malloc(sizeof(qc_sequence_t))` call, before any early-return path. This ensures the field is always initialized regardless of whether the builder subsequently sets it.

Allocators that already use `calloc` (and thus zero-initialize the field) are:
- `alloc_sequence()` in `src/integer_comparison.c`
- `logic_alloc_seq()` in `src/logic_operations.c`
- `hp_alloc_seq()` in `src/hot_path_add.c`
- `qc_sequence_alloc()` in `src/toffoli_helpers.c`

### R4: Capture builders must set `total_qubits` from allocator high-water mark

All capture-based sequence builders must query the temporary circuit's allocator after the operation completes and store the total qubit count (`ctx->allocator->next_qubit`).

Affected builders (6 total):
- `qc_divmod_cq_seq()` in `src/divmod_sequences.c`
- `qc_divmod_qq_seq()` in `src/divmod_sequences.c`
- `qc_c_divmod_cq_seq()` in `src/divmod_sequences.c`
- `qc_c_divmod_qq_seq()` in `src/divmod_sequences.c`
- `qc_c_arith_cq_mul_seq()` in `src/cmul_sequences.c`
- `qc_c_arith_qq_mul_seq()` in `src/cmul_sequences.c`

### R5: Non-capture builders set `total_qubits` to their qubit count

For non-capture sequence builders, `total_qubits` must be set to the number of distinct virtual qubit indices used in the sequence. This includes any AND-ancilla qubits introduced by MCX decomposition within the builder.

Complete list of non-capture builders (28 total, grouped by file):

**`src/qft_addition.c`** (4 builders):
| Builder | `total_qubits` |
|---------|----------------|
| `qc_arith_qq_add_seq(bits)` | `2 * bits` |
| `qc_arith_cq_add_seq(bits, value)` | `bits` |
| `qc_arith_cqq_add_seq(bits)` | `2 * bits + 1` |
| `qc_arith_ccq_add_seq(bits, value)` | `bits + 1` |

**`src/qft_multiplication.c`** (2 builders):
| Builder | `total_qubits` |
|---------|----------------|
| `qc_arith_cq_mul_seq(bits, value)` | `2 * bits` |
| `qc_arith_qq_mul_seq(bits)` | `3 * bits` |

**`src/integer_comparison.c`** (8 builders, 2 are stubs):
| Builder | `total_qubits` |
|---------|----------------|
| `qc_cmp_cq_equal_seq(bits, value)` | `bits < 3 ? bits + 1 : 2 * bits - 1` (result + A + AND-ancilla) |
| `qc_cmp_cq_less_seq(bits, value)` | `bits + 2` (result + A + borrow; sub-sequences are offset-copied, no extra qubits) |
| `qc_cmp_cq_greater_seq(bits, value)` | `bits + 2` (delegates to cq_less) |
| `qc_c_cmp_cq_equal_seq(bits, value)` | `bits == 1 ? bits + 2 : 2 * bits + 1` (result + A + ctrl + AND-ancilla) |
| `qc_c_cmp_cq_less_seq(bits, value)` | `bits + 3` (result + A + borrow + ctrl) |
| `qc_c_cmp_cq_greater_seq(bits, value)` | `bits + 3` (delegates to c_cq_less) |
| `qc_cmp_qq_less_seq(bits)` | stub (returns NULL) -- skip |
| `qc_c_cmp_qq_less_seq(bits)` | stub (returns NULL) -- skip |

**`src/logic_operations.c`** (8 builders):
| Builder | `total_qubits` |
|---------|----------------|
| `qc_not_seq(bits)` | `bits` |
| `qc_xor_seq(bits)` | `2 * bits` |
| `qc_and_seq(bits)` | `3 * bits` |
| `qc_or_seq(bits)` | `3 * bits` |
| `qc_c_not_seq(bits)` | `bits + 1` |
| `qc_c_xor_seq(bits)` | `2 * bits + 1` |
| `qc_c_and_seq(bits)` | `3 * bits + 1` |
| `qc_c_or_seq(bits)` | `3 * bits + 1` |

**`src/hot_path_add.c`** (2 builders):
| Builder | `total_qubits` |
|---------|----------------|
| `qc_split_cq_add_seq(bits, value)` | `bits + 1` |
| `qc_split_cq_sub_seq(bits, value)` | `bits + 1` (delegates to add_seq) |

**`src/toffoli_cla.c`** (4 stub builders, all return NULL -- skip):
- `qc_toffoli_qq_add_ks_seq`, `qc_toffoli_cq_add_ks_seq`, `qc_toffoli_cqq_add_ks_seq`, `qc_toffoli_ccq_add_ks_seq`

Note: The BK CLA builders (`build_qq_add_bk`, `build_cq_add_bk`, `build_cqq_add_bk`) are `static` and not directly exposed as `_seq()` API functions. Their public wrappers (`qc_toffoli_qq_add_bk`) allocate ancilla on the real circuit and build the qubit mapping internally, so `total_qubits` on the cached sequence is not needed by external callers. If these are exposed in the future, they would need `total_qubits` set.

### R6: Backward compatibility

- The `total_qubits` field defaults to 0, meaning "unknown" or "legacy".
- `qc_run_instruction()` behavior is unchanged; it does not validate mapping length.
- Callers that do not use `qc_sequence_total_qubits()` are unaffected.

### R7: Test coverage

- Unit test: `qc_sequence_total_qubits(NULL)` returns 0.
- Unit test: build each capture-based sequence, verify `qc_sequence_total_qubits()` returns a value greater than the register qubit count.
- Unit test: build representative non-capture sequences, verify `qc_sequence_total_qubits()` equals the expected value from the tables above.
- Unit test: build `qc_cmp_cq_equal_seq(4, 5)` and verify `total_qubits == 7` (4 + 1 result + 2 AND-ancilla).
- Integration test: replay a divmod sequence with a correctly-sized mapping, verify no OOM and correct gate placement.

## Success Criteria

1. `qc_sequence_total_qubits()` returns the correct total for all sequence builders (capture and non-capture).
2. A caller using `qc_sequence_total_qubits()` to size the qubit mapping can replay divmod/cmul sequences without OOM.
3. All existing tests continue to pass (backward compatible).
4. No changes to `qc_run_instruction()` logic.
5. All `malloc`-based allocators explicitly initialize `total_qubits = 0`.

## Out of Scope

- Modifying sibling packages (`quantum-cython-types`, `quantum-core`) to use the new API. Those changes will be tracked as separate issues in their respective packages.
- Changing the replay mechanism in `qc_run_instruction()` to validate mapping length. The function is correct; it just needs to receive complete mappings.
- Adding ancilla reuse or compaction within sequences. The virtual indices are stable and sequential.
- Refactoring the duplicated `capture_circuit_to_sequence()` helper between `divmod_sequences.c` and `cmul_sequences.c`. That is a separate cleanup task.
- Setting `total_qubits` on static/cached BK CLA sequences (`build_qq_add_bk`, etc.) since their public wrappers manage ancilla allocation internally.

## Key Design Decisions

### Why a struct field, not a scan?

Scanning all gates in a sequence to find the max qubit index is O(gates) and error-prone (must check targets and all controls). Storing the value at build time is O(1) at query time and guaranteed correct because the allocator tracks the high-water mark.

### Why `total_qubits` and not `n_ancillae`?

The caller needs to know the total mapping size, not just the ancilla count. Reporting `total_qubits` avoids requiring the caller to also know the register count formula for each operation type.

### Why not validate inside `qc_run_instruction()`?

Adding bounds checking inside the replay loop would add overhead to every gate of every sequence replay. The fix belongs at the boundary: the caller should construct a complete mapping. The C library's job is to report the information needed.

### Why init `total_qubits = 0` instead of switching malloc to calloc?

Changing `malloc` to `calloc` in `qft_sequence_alloc` and `qc_mul_sequence_alloc` would zero-initialize the entire struct, which is unnecessary since these allocators immediately set all other fields. Adding a single `seq->total_qubits = 0;` line is minimal, explicit, and avoids changing the allocation pattern.

## Sibling Package Impact

After this change ships, the following sibling packages need updates:

- **quantum-cython-types** (`_ir_helpers.py`): Must call `qc_sequence_total_qubits()` on the sequence, allocate `total - register` fresh ancilla qubits on the real circuit, and include them in the qubit mapping passed to `record_instruction()`.
- **quantum-cython-types** (`_c_backend.pxd`): Must add the `cdef` declaration for `qc_sequence_total_qubits()`.
- **quantum-core** (`backend_bridge.py`): May need to expose the total qubit count to Python callers.

These will be filed as cross-package issues after this PRD is approved.
