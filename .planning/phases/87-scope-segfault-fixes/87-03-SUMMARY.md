---
phase: 87-scope-segfault-fixes
plan: 03
subsystem: python-frontend
tags: [qarray, __imul__, multiplication, segfault, GC, allocated_qubits]

requires:
  - phase: 87-scope-segfault-fixes
    provides: MAXLAYERINSEQUENCE=300000 (Plan 87-01) resolving buffer overflow root cause
provides:
  - Working qarray *= scalar for widths 1-8
  - Defensive __imul__ guard preventing GC-triggered gate reversal
affects: [qarray-operations]

tech-stack:
  added: []
  patterns: [defensive _is_uncomputed guard after qubit swap in in-place operators]

key-files:
  created:
    - tests/python/test_bug02_qarray_imul.py
  modified:
    - src/quantum_language/qint_arithmetic.pxi
    - tests/test_qarray_elementwise.py
    - tests/test_qarray_mutability.py
    - tests/python/test_api_coverage.py

key-decisions:
  - "Root cause was MAXLAYERINSEQUENCE overflow, not allocated_qubits flag"
  - "Added defensive __imul__ guard anyway to prevent future GC issues"
  - "Removed 7 skip markers across 3 test files"

patterns-established:
  - "After qubit swap in in-place operators, mark swapped-out qint as _is_uncomputed=True"

requirements-completed: [BUG-02]

duration: 10min
completed: 2026-02-24
---

# Plan 87-03: qarray *= Scalar Segfault Fix Summary

**Fixed qarray *= scalar by resolving MAXLAYERINSEQUENCE overflow (87-01) and adding defensive __imul__ qubit ownership guard**

## Performance

- **Duration:** 10 min
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Confirmed root cause: MAXLAYERINSEQUENCE buffer overflow (already fixed by Plan 87-01)
- Added defensive fix: __imul__ marks swapped-out result_qint as uncomputed
- Created 25 regression tests covering widths 1-8, single/multi-element, simulation
- Removed 7 skip markers from 3 existing test files

## Task Commits

1. **Task 1+2: Fix and regression tests** - `83ba3f2` (fix)

## Files Created/Modified
- `src/quantum_language/qint_arithmetic.pxi` - Defensive __imul__ guard
- `tests/python/test_bug02_qarray_imul.py` - 25 regression tests
- `tests/test_qarray_elementwise.py` - 4 skip markers removed
- `tests/test_qarray_mutability.py` - 1 skip marker removed
- `tests/python/test_api_coverage.py` - 1 skip marker removed

## Decisions Made
- Root cause was MAXLAYERINSEQUENCE, not allocated_qubits flag as hypothesized in research
- Added defensive __imul__ guard regardless, as a correctness improvement

## Deviations from Plan

### Auto-fixed Issues

**1. Root cause different from hypothesis**
- **Found during:** Task 1 (investigation)
- **Issue:** Research hypothesized allocated_qubits flag issue, but actual root cause was MAXLAYERINSEQUENCE overflow
- **Fix:** Applied defensive guard for allocated_qubits anyway; primary fix was 87-01
- **Verification:** All 25 regression tests pass, simulation correct

## Issues Encountered
None - investigation quickly confirmed root cause.

## User Setup Required
None.

## Next Phase Readiness
- qarray *= works for all widths 1-8
- Plan 87-04 (controlled multiplication) can proceed

---
*Phase: 87-scope-segfault-fixes*
*Completed: 2026-02-24*
