---
phase: 118-nested-with-block-rewrite
plan: 01
subsystem: quantum-control
tags: [with-block, nesting, toffoli, AND-composition, context-manager, control-stack]

# Dependency graph
requires:
  - phase: 117-control-stack-infrastructure
    provides: "_push_control, _pop_control, _get_control_bool, _get_control_stack, _toffoli_and, _uncompute_toffoli_and"
provides:
  - "AND-composition in __enter__ for nested with-blocks (depth >= 1)"
  - "AND-ancilla uncomputation in __exit__ before popping control stack"
  - "Width=1 validation in __enter__ (TypeError for multi-bit qints)"
  - "Passing 2-level nested tests with correct doubly-controlled results"
  - "Controlled ~qbool works inside single and nested with-blocks"
affects: [118-02, nested-controls, chess-engine]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "AND-composition pattern: _toffoli_and() in __enter__ when _get_controlled() is True"
    - "AND-ancilla lifecycle: allocate in __enter__, uncompute in __exit__ after scope qbools"

key-files:
  created: []
  modified:
    - src/quantum_language/qint.pyx
    - src/quantum_language/qint_preprocessed.pyx
    - tests/python/test_nested_with_blocks.py

key-decisions:
  - "AND-ancilla uncomputed after scope qbool cleanup but before scope depth decrement and control pop"
  - "Width validation uses self.bits != 1 check, raising TypeError with descriptive message"
  - "Tests rewritten to use qbool(True/False) directly instead of comparisons on 3-bit qints (5-6 qubits vs 38)"

patterns-established:
  - "AND-composition: __enter__ checks _get_controlled(), calls _toffoli_and(current_ctrl, self), pushes (self, and_ancilla)"
  - "AND-uncomputation: __exit__ peeks at stack[-1], if and_ancilla not None, reads stack[-2] for outer ctrl, calls _uncompute_toffoli_and"

requirements-completed: [CTRL-01, CTRL-04, CTRL-05]

# Metrics
duration: 7min
completed: 2026-03-09
---

# Phase 118 Plan 01: Nested With-Block AND-Composition Summary

**AND-composition in __enter__/__exit__ enabling doubly-controlled operations in nested with-blocks, with width validation and rewritten test suite**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-09T19:43:29Z
- **Completed:** 2026-03-09T19:50:47Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Implemented AND-composition in __enter__: at nesting depth >= 1, _toffoli_and() composes outer and inner control qubits into AND-ancilla
- Implemented AND-ancilla uncomputation in __exit__: uncomputes AND-ancilla after scope qbool cleanup, before popping control stack
- Added width=1 validation raising TypeError for multi-bit qints used as with-block conditions
- Rewrote all 6 xfail tests to use qbool(True/False) conditions (5-6 qubits vs 38), removed all xfail markers
- Added controlled ~qbool tests at single-level and nested depth (CTRL-04 coverage)
- All 12 tests pass with zero xfail markers

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement AND-composition in __enter__/__exit__ with width validation** - `4161f91` (feat)
2. **Task 2: Rewrite xfail tests to use qbool(True/False) and remove xfail markers** - `bce45f1` (test)

## Files Created/Modified
- `src/quantum_language/qint.pyx` - Added _toffoli_and/_uncompute_toffoli_and imports, AND-composition in __enter__, AND-ancilla uncomputation in __exit__, width validation
- `src/quantum_language/qint_preprocessed.pyx` - Identical changes (sync copy)
- `tests/python/test_nested_with_blocks.py` - Rewrote 6 xfail tests, added 3 new tests (invert single, invert nested, width validation), 346 lines total

## Decisions Made
- AND-ancilla uncomputed after scope qbool cleanup but before scope depth decrement -- ensures scope qbools uncompute under correct control context
- Width validation placed at top of __enter__ after _check_not_uncomputed() -- fails fast before any stack manipulation
- Tests rewritten to use qbool(True/False) instead of comparisons -- reduces qubit count from ~38 to ~5-6, well under 17-qubit simulation limit

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed ruff B018 lint error for ~target expressions**
- **Found during:** Task 2 (test rewrite commit)
- **Issue:** ruff flagged `~target` as useless expression (B018) because result was not assigned
- **Fix:** Changed to `_ = ~target` to capture return value while preserving gate emission side effect
- **Files modified:** tests/python/test_nested_with_blocks.py
- **Verification:** ruff passes, tests still pass
- **Committed in:** bce45f1 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Lint fix required for pre-commit hook to pass. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- 2-level nested with-blocks fully working with AND-composition
- Single-level with-blocks verified as regression-free (CTRL-05)
- Controlled ~qbool verified at both depths (CTRL-04)
- Ready for Phase 118 Plan 02: 3+ level nesting tests and edge cases

## Self-Check: PASSED

All files exist, all commit hashes verified.

---
*Phase: 118-nested-with-block-rewrite*
*Completed: 2026-03-09*
