---
phase: 91-arithmetic-bug-fixes
plan: 02
subsystem: c-backend, python-frontend
tags: [modular-arithmetic, mod-reduce, toffoli, qint_mod, bug-fix]

requires:
  - phase: 91-01
    provides: "C-level divmod infrastructure and CDKM adder primitives"
provides:
  - "C-level modular reduction (toffoli_mod_reduce) replacing broken Python _reduce_mod"
  - "Python wiring: qint_mod operations dispatch to C-level mod_reduce"
affects:
  - tests/test_modular.py

tech-stack:
  added: []
  patterns: ["Iterative modular reduction via repeated comparison-and-subtract"]

key-files:
  created:
    - c_backend/src/ToffoliModReduce.c
  modified:
    - c_backend/include/toffoli_arithmetic_ops.h
    - src/quantum_language/qint_mod.pyx

key-decisions:
  - "Modular reduction uses iterative compare-and-subtract (same pattern as QQ division)"
  - "Persistent ancilla leak: 1 comparison ancilla per mod_reduce call cannot be uncomputed"
  - "Strictly better than old Python _reduce_mod which leaked n+1 qubits per comparison"

patterns-established:
  - "C-level modular reduction with explicit ancilla tracking"

requirements-completed: [FIX-03]

completed: 2026-02-24
---

# Phase 91 Plan 02: C-level Modular Reduction Summary

**C-level modular reduction replacing broken Python-level _reduce_mod, partially fixing BUG-MOD-REDUCE**

## Accomplishments
- Implemented `toffoli_mod_reduce` in ToffoliModReduce.c
- Wired `qint_mod` add, sub, mul to dispatch to C-level mod_reduce
- Reduced ancilla leak from n+1 qubits (old Python) to 1 qubit (new C-level) per reduction call
- Cases where no reduction is needed (raw result < N) pass correctly

## Task Commits

1. **C-level mod_reduce + Python wiring** - `461e644` (feat, combined with 91-01)

## Files Created/Modified
- `c_backend/src/ToffoliModReduce.c` - New: iterative modular reduction
- `c_backend/include/toffoli_arithmetic_ops.h` - Modified: function declarations
- `src/quantum_language/qint_mod.pyx` - Modified: dispatch to C backend

## Known Limitations
- Persistent ancilla leak: comparison ancilla entangled with computation, cannot be uncomputed
- Subtraction always triggers reduction (adds N then reduces), so all subtraction cases affected
- Addition/multiplication affected only when raw result >= N

---
*Phase: 91-arithmetic-bug-fixes*
*Completed: 2026-02-24*
