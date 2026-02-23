---
phase: 85-optimizer-fix-improvement
plan: 01
subsystem: optimizer
tags: [c-backend, optimizer, golden-master, regression-testing]

requires:
  - phase: 84-static-analysis
    provides: Clean codebase with static analysis passing
provides:
  - Fixed smallest_layer_below_comp loop direction (--i instead of ++i)
  - Golden-master snapshot infrastructure with 20 representative circuit fixtures
  - Targeted optimizer regression tests
affects: [85-02, 85-03]

tech-stack:
  added: []
  patterns: [golden-master-testing, circuit-snapshot-capture]

key-files:
  created:
    - tests/python/test_optimizer_golden_master.py
    - tests/python/test_optimizer_bug_fix.py
    - tests/golden_masters/*.json (20 files)
  modified:
    - c_backend/src/optimizer.c

key-decisions:
  - "Used ql.option('fault_tolerant', True/False) for arithmetic mode switching in golden-masters"
  - "20 circuits covering add/sub/mult/compare/bitwise/QFT/grover/compiled/controlled operations"
  - "Golden-master gen marked with @pytest.mark.golden_master_gen for manual-only execution"

patterns-established:
  - "Golden-master pattern: capture_circuit_snapshot() -> save/load JSON -> compare_snapshots()"
  - "Circuit registry: name -> builder function mapping for parametrized testing"

requirements-completed: [PERF-01]

duration: 12min
completed: 2026-02-23
---

# Plan 85-01: Fix optimizer loop direction bug + golden-master infrastructure Summary

**Fixed ++i to --i in smallest_layer_below_comp preventing out-of-bounds reads, with 20-circuit golden-master regression suite**

## Performance

- **Duration:** 12 min
- **Started:** 2026-02-23
- **Completed:** 2026-02-23
- **Tasks:** 2
- **Files modified:** 23

## Accomplishments
- Fixed latent loop direction bug: `++i` changed to `--i` in `smallest_layer_below_comp`, preventing reads past `occupied_layers_of_qubit` array bounds
- Fixed boundary condition: `if (last_index < 0)` changed to `if (last_index <= 0)` for correct unsigned-to-int handling
- Created golden-master infrastructure with `capture_circuit_snapshot()`, `save/load_golden_master()`, `compare_snapshots()` helpers
- Generated 20 representative circuit fixtures covering all major operation types
- Created 5 targeted regression tests verifying correct direction, determinism, placement, inverse cancellation, and multi-qubit ordering

## Task Commits

1. **Task 1 + Task 2: Bug fix + golden-master + regression tests** - `f3fbbde` (feat)

## Files Created/Modified
- `c_backend/src/optimizer.c` - Fixed loop direction bug in smallest_layer_below_comp
- `tests/python/test_optimizer_golden_master.py` - Golden-master snapshot infrastructure and 20-circuit registry
- `tests/python/test_optimizer_bug_fix.py` - 5 targeted optimizer correctness tests
- `tests/golden_masters/*.json` - 20 circuit snapshot fixtures (add, sub, mult, compare, bitwise, QFT, grover, compiled, controlled)

## Decisions Made
- Combined Task 1 and Task 2 into a single commit since both are part of the same logical change
- Used `ql.option('fault_tolerant', True/False)` instead of non-existent `ql.set_arithmetic_mode()`
- Verified zero regressions by comparing test results before and after fix (same 1 pre-existing failure)

## Deviations from Plan
None - plan executed as written with minor API adaptation (option() instead of set_arithmetic_mode()).

## Issues Encountered
- `ql.set_arithmetic_mode` referenced in plan does not exist; discovered correct API is `ql.option('fault_tolerant', True)`
- Full test suite OOM-kills when run as single batch; ran tests in smaller batches
- Pre-existing test failure `test_tic_tac_toe_pattern` (qbool item assignment TypeError) confirmed unrelated to fix

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Golden-master infrastructure ready for Plan 85-02 (binary search) verification
- All 20 circuit snapshots committed for regression comparison
- Optimizer correctness tests provide safety net for further optimizer changes

---
*Phase: 85-optimizer-fix-improvement*
*Completed: 2026-02-23*
