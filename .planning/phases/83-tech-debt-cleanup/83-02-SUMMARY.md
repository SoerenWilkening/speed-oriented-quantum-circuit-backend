---
phase: 83-tech-debt-cleanup
plan: 02
subsystem: infra
tags: [vulture, dead-code, makefile, sequence-generation, deprecation]

# Dependency graph
requires:
  - phase: 82-infra-dependency-fixes
    provides: stable build/test infrastructure
provides:
  - vulture scan confirming zero dead code at >=80% confidence in src/quantum_language/
  - make generate-sequences target for one-command sequence regeneration
  - deprecated script runtime warnings for generate_seq_1_4.py and generate_seq_5_8.py
affects: [84-test-improvement, 85-optimizer]

# Tech tracking
tech-stack:
  added: [vulture (one-time scan, not persisted)]
  patterns: [deprecation-warnings-on-import, makefile-code-generation-targets]

key-files:
  created: []
  modified:
    - Makefile
    - scripts/generate_seq_1_4.py
    - scripts/generate_seq_5_8.py

key-decisions:
  - "Vulture found zero dead code at >=80% confidence -- all 60%-confidence findings confirmed as false positives (used from .pyx files, tests, or public API)"
  - "No code removed since all findings were false positives at below-threshold confidence"
  - "Active generator scripts already had comprehensive docstrings and argparse -- no changes needed"

patterns-established:
  - "Deprecation pattern: docstring notice + warnings.warn() after imports for deprecated scripts"
  - "Code generation: make generate-sequences as single entry point for all sequence C file regeneration"

requirements-completed: [DEBT-03, DEBT-04]

# Metrics
duration: 39min
completed: 2026-02-23
---

# Phase 83 Plan 02: Dead Code Removal and Sequence Generation Documentation Summary

**Vulture scan confirmed zero dead Python code at >=80% confidence; added make generate-sequences target and deprecated script warnings**

## Performance

- **Duration:** 39 min
- **Started:** 2026-02-23T15:33:49Z
- **Completed:** 2026-02-23T16:13:01Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Ran comprehensive vulture dead code scan on src/quantum_language/ -- zero findings at >=80% confidence threshold
- Verified all 22 findings at 60% confidence are false positives (used from .pyx/.pxd files, tests, or are public API)
- Added `make generate-sequences` Makefile target that runs all 5 active sequence generators
- Added deprecation docstrings and runtime `warnings.warn()` to both deprecated scripts
- Confirmed all 5 active generator scripts already have comprehensive --help with argparse

## Task Commits

Each task was committed atomically:

1. **Task 1: Run vulture scan and remove confirmed dead Python code** - No commit (zero dead code found at >=80% confidence -- scan was clean, no files modified)
2. **Task 2: Document sequence generation and add Makefile target** - `e830e1e` (chore)

**Plan metadata:** (pending)

## Files Created/Modified
- `Makefile` - Added generate-sequences target and Code generation section in help
- `scripts/generate_seq_1_4.py` - Added deprecation docstring and runtime warning
- `scripts/generate_seq_5_8.py` - Added deprecation docstring and runtime warning

## Decisions Made
- **No dead code removed:** Vulture found 0 findings at >=80% confidence. All 22 findings at 60% were verified as false positives:
  - `_qarray_utils.py` functions (_infer_width, _flatten, _reduce_tree, _reduce_linear) -- all imported and used from qarray.pyx
  - `compile.py` attributes (allocated_start, _start_layer, _end_layer, operation_type, etc.) -- all used extensively from qint_preprocessed.pyx and qbool.pyx
  - `compile.py` properties (reduction_percent, adjoint) -- used in tests (test_compile.py)
  - `draw.py` constants (CELL_W, CELL_H) -- imported in test_draw_render.py
  - `oracle.py` attribute (phase) -- used via qint.phase property defined in .pyx
  - `profiler.py` methods (report, top_functions) -- public API exported in __all__
  - `compile.py` variables (_verify, _inverse_eager, ret_qubit_count) -- stored for future use or tuple unpacking convention
- **Active scripts unchanged:** All 5 generator scripts already had comprehensive module docstrings, usage examples, and argparse with --help
- **Deprecation via runtime warning:** Used `warnings.warn(msg, DeprecationWarning, stacklevel=2)` placed after imports to satisfy E402 linting

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed E402 linting error in deprecated scripts**
- **Found during:** Task 2 commit (pre-commit hook failure)
- **Issue:** Initial placement of `warnings.warn()` before other imports caused E402 (module level import not at top of file)
- **Fix:** Moved `import warnings` and all imports above the `warnings.warn()` call
- **Files modified:** scripts/generate_seq_1_4.py, scripts/generate_seq_5_8.py
- **Verification:** Pre-commit ruff check passes
- **Committed in:** e830e1e (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Minor import ordering fix for linting compliance. No scope creep.

## Issues Encountered
- Pre-existing test failure in test_api_coverage.py::test_qint_default_width (expects width=8, gets width=3) -- not caused by this plan, documented as pre-existing
- Pre-existing segfault in test_array_creates_list_of_qint -- known Phase 87 bug documented in STATE.md
- Generated sequence C files differ from committed versions when regenerated (expected since generators have evolved) -- restored to committed state since regeneration was only for verification

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Dead code scan complete, codebase is clean at >=80% confidence threshold
- Sequence generation is now documented and discoverable via `make generate-sequences`
- Ready for remaining Phase 83 plans

## Self-Check: PASSED

- FOUND: 83-02-SUMMARY.md
- FOUND: Makefile
- FOUND: scripts/generate_seq_1_4.py
- FOUND: scripts/generate_seq_5_8.py
- FOUND: commit e830e1e (Task 2)

---
*Phase: 83-tech-debt-cleanup*
*Completed: 2026-02-23*
