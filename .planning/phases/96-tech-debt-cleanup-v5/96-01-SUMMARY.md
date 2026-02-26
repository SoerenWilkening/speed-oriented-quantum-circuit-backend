---
phase: 96-tech-debt-cleanup-v5
plan: 01
subsystem: infra
tags: [cython, dead-code, cleanup]

requires:
  - phase: 92-modular-toffoli-arithmetic
    provides: Beauregard primitives that replaced toffoli_mod_reduce
provides:
  - Clean _core.pxd without dead toffoli_mod_reduce/toffoli_cmod_reduce declarations
  - Clean qint.pyx without unused toffoli_cdivmod_cq/qq imports
affects: []

tech-stack:
  added: []
  patterns: []

key-files:
  created: []
  modified:
    - src/quantum_language/_core.pxd
    - src/quantum_language/qint.pyx

key-decisions:
  - "Clean removal, no stubs -- recover from git history if needed later"
  - "C source files (ToffoliModReduce.c, ToffoliDivision.c) left untouched -- only Cython references removed"

patterns-established: []

requirements-completed: []

duration: 5min
completed: 2026-02-26
---

# Plan 96-01: Remove Dead Declarations Summary

**Removed unreachable toffoli_mod_reduce and unused toffoli_cdivmod_cq/qq from Cython declaration and import files**

## Performance

- **Duration:** 5 min
- **Tasks:** 2
- **Files modified:** 3 (including auto-synced qint_preprocessed.pyx)

## Accomplishments
- Removed `toffoli_mod_reduce` and `toffoli_cmod_reduce` declarations from `_core.pxd`
- Removed `toffoli_cdivmod_cq` and `toffoli_cdivmod_qq` unused imports from `qint.pyx`
- Cython extensions build cleanly after removal
- Preserved all actively-used `toffoli_divmod_cq` and `toffoli_divmod_qq` references

## Task Commits

1. **Task 1+2: Remove dead declarations and verify** - `c64e755` (refactor)

## Files Created/Modified
- `src/quantum_language/_core.pxd` - Removed 5 lines of dead toffoli_mod_reduce declarations
- `src/quantum_language/qint.pyx` - Removed 1 line of unused toffoli_cdivmod imports
- `src/quantum_language/qint_preprocessed.pyx` - Auto-synced by pre-commit hook

## Decisions Made
None - followed plan as specified

## Deviations from Plan
None - plan executed exactly as written

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Dead code eliminated; _core.pxd and qint.pyx are clean

---
*Phase: 96-tech-debt-cleanup-v5*
*Completed: 2026-02-26*
