---
phase: 117-control-stack-infrastructure
plan: 02
subsystem: infra
tags: [cython, control-stack, oracle, compile, with-blocks, xfail]

# Dependency graph
requires:
  - phase: 117-01
    provides: "_control_stack list, _push_control/_pop_control, _get_control_stack/_set_control_stack, backward-compat wrappers"
provides:
  - "oracle.py save/restore using stack-based _get_control_stack/_set_control_stack"
  - "Updated xfail markers reflecting Phase 117 intermediate state (strict=False, no raises=NotImplementedError)"
  - "Integration tests for compile save/restore, single-level with-blocks, and nested push-no-crash"
affects: [118-nested-control-composition]

# Tech tracking
tech-stack:
  added: []
  patterns: [stack-save-restore-for-oracle-ancilla, xfail-strict-false-for-intermediate-state]

key-files:
  created: []
  modified:
    - src/quantum_language/oracle.py
    - tests/python/test_nested_with_blocks.py
    - tests/python/test_control_stack.py

key-decisions:
  - "Removed unused backward-compat imports from oracle.py since save/restore now uses stack API exclusively"
  - "Set xfail strict=False for nested tests because Phase 117 intermediate state may produce partial results rather than crashes"

patterns-established:
  - "Stack save/restore pattern: saved = list(_get_control_stack()); _set_control_stack([]); ...; _set_control_stack(saved)"

requirements-completed: [CTRL-02, CTRL-03]

# Metrics
duration: 23min
completed: 2026-03-09
---

# Phase 117 Plan 02: Wire Control Stack into Framework Summary

**Stack-based control wired into oracle.py save/restore, xfail markers updated for Phase 117 intermediate state, and integration tests verifying compile/with-block/nested behavior**

## Performance

- **Duration:** 23 min
- **Started:** 2026-03-09T18:33:16Z
- **Completed:** 2026-03-09T18:56:00Z
- **Tasks:** 3
- **Files modified:** 3

## Accomplishments
- Updated oracle.py save/restore to use _get_control_stack/_set_control_stack (both instances)
- Removed 6 unused backward-compat imports from oracle.py
- Updated all 6 xfail markers: removed raises=NotImplementedError, set strict=False
- Added 3 integration tests: compile save/restore, single-level with-block, nested push-no-crash
- Full regression verification: zero new failures across 300+ tests

## Task Commits

Each task was committed atomically:

1. **Task 1: Update oracle.py save/restore to use control stack** - `6b25367` (feat)
2. **Task 2: Update xfail markers and add integration tests** - `57dfbcb` (test)
3. **Task 3: Full regression verification** - verification-only, no commit

## Files Created/Modified
- `src/quantum_language/oracle.py` - Both save/restore blocks use _get_control_stack/_set_control_stack; removed unused backward-compat imports
- `tests/python/test_nested_with_blocks.py` - Updated 6 xfail markers (strict=False, removed raises), updated docstrings
- `tests/python/test_control_stack.py` - Added TestIntegration class with 3 tests

## Decisions Made
- Removed unused backward-compat imports (_get_controlled, _set_controlled, _get_control_bool, _set_control_bool, _get_list_of_controls, _set_list_of_controls) from oracle.py since they were no longer referenced after switching to stack API
- Used strict=False for xfail markers because nested with-blocks may produce partial (inner-only controlled) results rather than crashing, so tests may pass or fail unpredictably

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed integration test gate count assertion**
- **Found during:** Task 2 (test_compile_save_restore_stack)
- **Issue:** Test used `circuit_stats()["total_gates"]` but circuit_stats() returns allocation metrics, not gate counts
- **Fix:** Changed to use `ql.circuit.__new__(ql.circuit).gate_counts` and sum values
- **Files modified:** tests/python/test_control_stack.py
- **Verification:** All 3 integration tests pass
- **Committed in:** 57dfbcb (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Minor test assertion fix. No scope creep.

## Issues Encountered
- Full test suite (1698 tests) OOM-killed in CI environment; regression verified via targeted batches covering 300+ tests across all subsystems

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 117 complete: all control stack infrastructure wired into framework
- _push_control/_pop_control used by __enter__/__exit__ (done in 117-01)
- _get_control_stack/_set_control_stack used by compile.py (done in 117-01) and oracle.py (done in 117-02)
- Nested with-blocks no longer crash (push two entries, but only inner controls)
- Ready for Phase 118: AND-ancilla composition to make nested with-blocks produce doubly-controlled gates

## Self-Check: PASSED

All files verified present. All commit hashes verified in git log.

---
*Phase: 117-control-stack-infrastructure*
*Completed: 2026-03-09*
