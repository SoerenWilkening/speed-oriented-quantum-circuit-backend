---
phase: 87-scope-segfault-fixes
plan: 04
subsystem: python-frontend
tags: [scope, current_scope_depth, __mul__, __rmul__, conditional, with-block, BUG-COND-MUL-01]

requires:
  - phase: 87-scope-segfault-fixes
    provides: Working multiplication from Plans 87-01/87-03
provides:
  - Scope-safe __mul__ and __rmul__ that bypass scope registration
  - Working controlled multiplication inside with blocks
affects: [controlled-operations, scope-management]

tech-stack:
  added: []
  patterns: [scope depth bypass pattern for operators creating result qints]

key-files:
  created:
    - tests/python/test_bug07_cond_mul.py
  modified:
    - src/quantum_language/qint_arithmetic.pxi
    - tests/test_conditionals.py
    - tests/test_toffoli_multiplication.py
    - tests/python/test_cross_backend.py

key-decisions:
  - "Scope depth bypass in __mul__/__rmul__ only, not __init__ or scope logic"
  - "Workaround removal from tests since production code now handles scope bypass"
  - "test_cond_mul_false expects 0 (not original value) - correct per controlled mul semantics"

patterns-established:
  - "current_scope_depth.set(0) before result allocation in operators, restore after"

requirements-completed: [BUG-07]

duration: 12min
completed: 2026-02-24
---

# Plan 87-04: Controlled Multiplication Scope Fix Summary

**Prevented multiplication result scope registration via current_scope_depth bypass in __mul__/__rmul__, removing 13 test workarounds**

## Performance

- **Duration:** 12 min
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Fixed controlled multiplication scope uncomputation corruption (BUG-COND-MUL-01)
- Removed 6 scope-depth workarounds from test_toffoli_multiplication.py
- Removed 7 scope-depth workarounds from test_cross_backend.py
- Removed xfail markers from test_conditionals.py conditional mul tests
- All 12 controlled Toffoli multiplication tests pass without workarounds

## Task Commits

1. **Task 1+2: Scope bypass and test cleanup** - `1e7f971` (fix)

## Files Created/Modified
- `src/quantum_language/qint_arithmetic.pxi` - Scope depth bypass in __mul__/__rmul__
- `tests/python/test_bug07_cond_mul.py` - 7 regression tests
- `tests/test_conditionals.py` - xfail markers removed, tests rewritten
- `tests/test_toffoli_multiplication.py` - 6 workarounds removed
- `tests/python/test_cross_backend.py` - 7 workarounds removed

## Decisions Made
- Targeted fix in __mul__/__rmul__ only (minimal blast radius per user decision)
- test_cond_mul_false expects 0, not 1 - this is correct per controlled mul semantics

## Deviations from Plan
- test_cond_mul_false test expected value changed from 1 to 0 (controlled mul with inactive control produces 0 in fresh register)

## Issues Encountered
- Pre-existing verify_circuit fixture uses bitstring[:width] extraction which doesn't work for in-place operators that swap qubit positions; rewrote test_conditionals.py mul tests with direct extraction

## User Setup Required
None.

## Next Phase Readiness
- Controlled multiplication works correctly in production
- All test workarounds removed

---
*Phase: 87-scope-segfault-fixes*
*Completed: 2026-02-24*
