---
phase: 91-arithmetic-bug-fixes
plan: 01
subsystem: c-backend, python-frontend
tags: [division, modulo, restoring-divmod, CDKM, toffoli, bug-fix]

requires:
  - phase: 90
    provides: "Phase 90 complete (no direct dependency, but sequential ordering)"
provides:
  - "C-level restoring CQ divmod (toffoli_divmod_cq) with proper ancilla management"
  - "C-level repeated-subtraction QQ divmod (toffoli_divmod_qq)"
  - "Python wiring: qint.__floordiv__ and qint.__mod__ dispatch to C backend"
affects:
  - tests/test_div.py
  - tests/test_mod.py
  - tests/test_toffoli_division.py

tech-stack:
  added: []
  patterns: ["Restoring division with Bennett's trick for reversible comparison"]

key-files:
  created:
    - c_backend/src/ToffoliDivision.c
  modified:
    - c_backend/include/toffoli_arithmetic_ops.h
    - src/quantum_language/qint_division.pxi

key-decisions:
  - "CQ division uses restoring algorithm (MSB-to-LSB bit-serial) with CDKM adders"
  - "QQ division uses repeated-subtraction (2^n iterations) -- has persistent ancilla leak"
  - "Bennett's trick: compute comparison -> copy to quotient bit -> uncompute comparison"
  - "Old QFT division code removed entirely (was broken, unrepairable)"

patterns-established:
  - "C-level arithmetic with LSB-first qubit convention"
  - "Widened comparison (n+1 bits) for proper MSB handling"

requirements-completed: [FIX-01, FIX-02]

completed: 2026-02-24
---

# Phase 91 Plan 01: C-level Restoring Divmod Summary

**C-level restoring division replacing broken Python-level QFT division, fixing BUG-DIV-02 and BUG-QFT-DIV**

## Accomplishments
- Implemented `toffoli_divmod_cq` in ToffoliDivision.c: restoring division with n iterations, proper ancilla uncomputation
- Implemented `toffoli_divmod_qq` for quantum-quantum division (repeated subtraction, known persistent ancilla limitation)
- Wired Python `__floordiv__` and `__mod__` operators to dispatch to C-level divmod
- CQ division: 0 persistent ancillae, all temporaries freed per iteration
- QQ division: 1 persistent ancilla per iteration (fundamental limitation of repeated-subtraction approach)

## Task Commits

1. **C-level divmod implementation + Python wiring** - `461e644` (feat)

## Files Created/Modified
- `c_backend/src/ToffoliDivision.c` - New: CQ and QQ restoring divmod
- `c_backend/include/toffoli_arithmetic_ops.h` - Modified: function declarations
- `src/quantum_language/qint_division.pxi` - Modified: dispatch to C backend

## Known Limitations
- QQ division has persistent ancilla leak (comparison ancilla cannot be uncomputed in repeated-subtraction path)
- `_divmod_c` does not check `_get_controlled()` -- ignores control context

---
*Phase: 91-arithmetic-bug-fixes*
*Completed: 2026-02-24*
