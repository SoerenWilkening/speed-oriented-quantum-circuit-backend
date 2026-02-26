---
phase: 93-depth-ancilla-tradeoff
status: passed
verified: 2026-02-26
---

# Phase 93: Depth/Ancilla Tradeoff - Verification

## Goal
Users can control whether the framework optimizes for circuit depth or qubit count when selecting adder implementations.

## Success Criteria Verification

### 1. User can set ql.option('tradeoff', 'auto'), ql.option('tradeoff', 'min_depth'), or ql.option('tradeoff', 'min_qubits') to control adder selection
**Status: PASSED**
- `ql.option('tradeoff', ...)` handler implemented in `src/quantum_language/_core.pyx` supporting three string values: 'auto', 'min_depth', 'min_qubits' (93-01-SUMMARY)
- Invalid values raise `ValueError` with descriptive message (93-01-SUMMARY: validation)
- Post-arithmetic changes raise `RuntimeError` via `_arithmetic_ops_performed` module-level flag, frozen after any `addition_inplace` call (93-01-SUMMARY: set-once enforcement)
- Tradeoff state resets on new circuit creation via `_reset_for_circuit` (93-01-SUMMARY: reset logic)
- Tests: 21 tests in `tests/python/test_tradeoff.py` covering option API, frozen state, dispatch modes, correctness via Qiskit simulation, and modular RCA forcing (93-01-SUMMARY)
- Code verification: `src/quantum_language/_core.pyx` contains tradeoff option handler with validation and set-once logic
- Code verification: `src/quantum_language/qint_arithmetic.pxi` contains `_mark_arithmetic_performed()` call

### 2. In auto mode, CLA is selected for width >= threshold and CDKM otherwise, verified by inspecting circuit depth for small vs large widths
**Status: PASSED**
- `tradeoff_auto_threshold` field added to `circuit_t` C struct in `c_backend/include/circuit.h`, initialized to 4 in `c_backend/src/circuit_allocations.c` (93-01-SUMMARY)
- Runtime dispatch at 8 locations in `c_backend/src/hot_path_add_toffoli.c` replaces compile-time `CLA_THRESHOLD` with `circ->tradeoff_auto_threshold` (93-01-SUMMARY)
- Auto mode: CLA selected for width >= 4, CDKM (RCA) for width < 4 -- threshold based on Phase 71 empirical data showing CLA depth benefit minimal below width 4 (93-01-SUMMARY)
- min_depth mode: `tradeoff_auto_threshold` set to 2, enabling CLA for all widths >= 2 (93-01-SUMMARY)
- min_qubits mode: `cla_override` set to 1, forcing CDKM/RCA for all widths (93-01-SUMMARY)
- Tests verify dispatch behavior for different widths and modes via Qiskit simulation (93-01-SUMMARY)
- Code verification: `c_backend/include/circuit.h` contains `tradeoff_auto_threshold` and `tradeoff_min_depth` fields in circuit_t struct

### 3. Modular arithmetic primitives always use RCA regardless of tradeoff policy setting
**Status: PASSED**
- `c_backend/src/ToffoliModReduce.c` calls `toffoli_CQ_add`/`toffoli_QQ_add` directly (CDKM RCA primitives), NOT the `hot_path_add_toffoli` dispatch functions (93-01-SUMMARY)
- Verified: identical QASM output and peak qubit counts across all three tradeoff modes for modular operations (93-01-SUMMARY)
- Independently confirmed by Phase 92 VERIFICATION.md SC#5: "All C functions call toffoli_CQ_add/toffoli_QQ_add directly (which are CDKM RCA), NOT hot_path dispatch" (92-VERIFICATION.md)
- v5.0 milestone audit integration checker confirmed 14 dispatch locations with correct RCA bypass for modular paths (95-RESEARCH.md)
- Code verification: `c_backend/src/ToffoliModReduce.c` exists with direct calls to toffoli_CQ_add, toffoli_cCQ_add, toffoli_QQ_add, toffoli_cQQ_add

### 4. BK CLA subtraction limitation is documented clearly in both code docstrings and user-facing documentation
**Status: PASSED**
- Two's complement CLA subtraction implemented in min_depth mode for all 4 code paths (93-02-SUMMARY):
  - QQ uncontrolled: X(b) + CLA_add(a += ~b) + CQ_add(a += 1) + X(b)
  - QQ controlled: CX bit flips + controlled CLA add + controlled CQ add + CX restore
  - CQ uncontrolled: classical negation (2^width - value) + forward CLA add
  - CQ controlled: same classical negation approach with controlled CLA
- Documentation in `c_backend/src/hot_path_add_toffoli.c` file header explaining CLA subtraction limitation and two's complement approach (93-02-SUMMARY)
- Documentation in `src/quantum_language/_core.pyx` `option()` docstring with tradeoff mode explanations including CLA subtraction details (93-02-SUMMARY)
- 27 total tests in `tests/python/test_tradeoff.py`: 21 original (93-01) + 6 new CLA subtraction tests (93-02) covering correctness, cross-mode equivalence, and add/sub regression (93-02-SUMMARY)
- Code verification: `c_backend/src/hot_path_add_toffoli.c` exists with file header documentation and CLA subtraction implementations
- Code verification: `tests/python/test_tradeoff.py` exists with TestCLASubtraction and TestTradeoffRegression test classes

## Requirements Traceability

| Requirement | Status | Evidence |
|-------------|--------|----------|
| TRD-01 | Complete | `ql.option('tradeoff', ...)` with auto/min_depth/min_qubits modes; validation (ValueError for invalid); set-once enforcement (RuntimeError post-arithmetic); reset on new circuit; 21 tests (93-01-SUMMARY) |
| TRD-02 | Complete | Auto threshold=4 in circuit_t; 8 runtime dispatch locations in hot_path_add_toffoli.c; min_depth threshold=2; min_qubits cla_override=1; tests verify dispatch behavior per mode (93-01-SUMMARY) |
| TRD-03 | Complete | ToffoliModReduce.c calls toffoli_CQ_add/toffoli_QQ_add directly (RCA); identical results across all tradeoff modes; independently confirmed by 92-VERIFICATION.md SC#5 (93-01-SUMMARY, 92-VERIFICATION.md) |
| TRD-04 | Complete | Two's complement CLA subtraction in 4 code paths; file header documentation in hot_path_add_toffoli.c; extended docstring in _core.pyx option(); 6 new subtraction tests; 27 total tests (93-02-SUMMARY) |

## Artifacts

| File | Purpose |
|------|---------|
| c_backend/include/circuit.h | tradeoff_auto_threshold and tradeoff_min_depth fields in circuit_t struct |
| c_backend/src/circuit_allocations.c | Field initialization (threshold=4, min_depth=0) |
| c_backend/src/hot_path_add_toffoli.c | Runtime CLA/CDKM dispatch at 8 locations + CLA subtraction via two's complement + file header documentation |
| src/quantum_language/_core.pyx | Tradeoff option handler with validation, set-once enforcement, and extended docstring documentation |
| src/quantum_language/_core.pxd | Cython declarations for new circuit_t fields |
| src/quantum_language/qint_arithmetic.pxi | _mark_arithmetic_performed() call in addition_inplace |
| src/quantum_language/qint.pyx | Import _mark_arithmetic_performed from _core |
| src/quantum_language/qint_preprocessed.pyx | Synced from qint.pyx + .pxi includes |
| tests/python/test_tradeoff.py | 27 tests for TRD-01 through TRD-04 |

## Self-Check: PASSED

All 4 ROADMAP success criteria verified with specific evidence from SUMMARYs and code inspection. TRD-01, TRD-02, TRD-03, TRD-04 requirements satisfied. No deviations from Phase 93 plans -- 93-02-SUMMARY confirms zero deviations.
