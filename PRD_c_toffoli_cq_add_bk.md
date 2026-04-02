# PRD: Controlled Toffoli CQ BK CLA Add Sequence Builder

**Issue**: ORCHESTRATOR_HANDOFF_TOFFOLI_AND_REMAINING.md Issue 6
**Status**: Confirmed -- work needed
**Packages**: circuit-c-backend, quantum-cython-types, quantum-core

## Problem

When `a += 3` executes inside `with qbool:` in `@compile` mode with
`fault_tolerant=True`, the arithmetic path records `op="toffoli_cq_add_bk"`.
The `_record_ir` function detects the controlled context and prepends `c_`,
producing `"c_toffoli_cq_add_bk"`. This op name is not in the dispatch table,
so `get_sequence` logs a warning and returns 0 (no-op sequence pointer),
silently dropping the addition.

## Root Cause Analysis

Three missing pieces across three layers:

1. **C layer**: `qc_toffoli_ccq_add_bk_seq` does not exist in `toffoli_cla_seq.c`.
   The uncontrolled `qc_toffoli_cq_add_bk_seq` and the controlled-QQ
   `qc_toffoli_cqq_add_bk_seq` both exist, but the controlled-CQ variant
   was never created. The underlying runtime function `qc_toffoli_ccq_add`
   (in `toffoli_cdkm.c` line 557) is available to build the sequence from.

2. **Cython layer**: No declaration of `qc_toffoli_ccq_add_bk_seq` in
   `_c_backend.pxd` and no Python wrapper in `_c_backend.pyx`.

3. **Dispatch table**: `sequences.py` has no entry mapping
   `"c_toffoli_cq_add_bk"` to `"toffoli_ccq_add_bk_seq"`.

## Success Criteria

1. `qc_toffoli_ccq_add_bk_seq(bits, value)` exists in C and produces a
   valid `qc_sequence_t*` with correct `total_qubits`.
2. Cython wrapper `toffoli_ccq_add_bk_seq(bits, value)` is callable from Python.
3. Dispatch entry `"c_toffoli_cq_add_bk": ("toffoli_ccq_add_bk_seq", True)`
   exists in `_SEQ_DISPATCH`.
4. The `_CQ_OPS` set in `_ir_helpers.py` already contains `"toffoli_ccq_add_bk"`,
   so `_extract_bits_value` will correctly parse parameters (no change needed).

## Existing Pattern

The controlled-CQ KS variant (`qc_toffoli_ccq_add_ks_seq`) already exists in
`toffoli_cla.c` and follows the exact same capture pattern:
- Create capture context
- Allocate `n` target qubits + 1 control qubit
- Call `qc_toffoli_ccq_add(ctx, target, n, value, control)`
- Capture to sequence, set `total_qubits`, destroy context

The new BK CLA function follows this identical pattern but lives in
`toffoli_cla_seq.c` alongside the other BK CLA builders.

## Scope

- 1 new C function (~35 lines, matching existing pattern)
- 1 new header declaration (1 line)
- 1 new Cython `.pxd` declaration (1 line)
- 1 new Cython `.pyx` wrapper (~5 lines)
- 1 new dispatch entry (1 line)
- No new tests required beyond verifying the function links correctly;
  the existing `qc_toffoli_ccq_add` has full test coverage in
  `test_toffoli_cdkm.c`.
