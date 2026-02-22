---
phase: 82-infrastructure-dependency-fixes
plan: 01
subsystem: infra
tags: [coverage, pytest-cov, cython, qiskit-aer, pyproject-toml, makefile, import-guards]

# Dependency graph
requires:
  - phase: none
    provides: first plan in v4.1 milestone
provides:
  - pyproject.toml as single source of truth for dependencies
  - qiskit-aer declared in verification extras
  - sim_backend.py wrapper module with lazy import guards
  - coverage configuration in pyproject.toml
  - QUANTUM_COVERAGE env var for instrumented builds
  - Makefile coverage target for unified HTML report pipeline
affects: [82-02, 83-test-suite-expansion, 86-qft-bug-fixes, 87-advanced-bug-fixes]

# Tech tracking
tech-stack:
  added: [pytest-cov, Cython.Coverage plugin, gcov/lcov (optional)]
  patterns: [lazy import guard with cached availability, thin backend wrapper, QUANTUM_COVERAGE env var]

key-files:
  created:
    - src/quantum_language/sim_backend.py
  modified:
    - pyproject.toml
    - setup.py
    - .gitignore
    - Makefile
    - src/quantum_language/grover.py
    - src/quantum_language/amplitude_estimation.py

key-decisions:
  - "pyproject.toml is single source of truth for all dependency metadata; removed install_requires and extras_require from setup.py"
  - "pytest-cov placed in dev extras (not a separate test group)"
  - "sim_backend.py uses cached availability checks (_QISKIT_AVAILABLE, _AER_AVAILABLE) with lazy guard pattern"
  - "QUANTUM_COVERAGE env var is separate from QUANTUM_PROFILE (different performance characteristics)"
  - "Added noqa: E402 to pre-existing late imports in setup.py (sys.path manipulation required before build_preprocessor import)"

patterns-established:
  - "Lazy import guard: use _check_qiskit()/_check_aer() cached checks, _require_backend()/_require_simulator() guard functions"
  - "Backend wrapper: all qiskit imports go through sim_backend.py, never imported directly in src/quantum_language/"
  - "Generic error messages: no mention of qiskit in user-facing errors, reference pip install quantum_language[verification]"

requirements-completed: [BUG-03, TEST-01]

# Metrics
duration: 21min
completed: 2026-02-22
---

# Phase 82 Plan 01: Infrastructure & Dependency Fixes Summary

**Dependency metadata consolidated in pyproject.toml, sim_backend.py wrapper with lazy import guards for qiskit/qiskit-aer, and Makefile coverage pipeline with optional lcov integration**

## Performance

- **Duration:** 21 min
- **Started:** 2026-02-22T21:58:27Z
- **Completed:** 2026-02-22T22:19:35Z
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments
- Consolidated all dependency metadata in pyproject.toml (removed duplicates from setup.py)
- Created sim_backend.py thin wrapper with lazy import guards and generic error messages
- Migrated grover.py and amplitude_estimation.py to use sim_backend instead of direct qiskit imports
- Added coverage configuration (pyproject.toml + QUANTUM_COVERAGE env var + Makefile target)
- All 45 grover + amplitude_estimation tests pass unchanged after migration

## Task Commits

Each task was committed atomically:

1. **Task 1: Update dependency metadata and coverage configuration** - `5ec5a39` (chore)
2. **Task 2: Create sim_backend wrapper and migrate qiskit imports** - `b540b3b` (feat)
3. **Task 3: Add Makefile coverage target** - `820c73b` (chore)

## Files Created/Modified
- `src/quantum_language/sim_backend.py` - Thin simulation backend wrapper with lazy import guards, load_qasm(), and simulate() functions
- `pyproject.toml` - Added qiskit-aer to verification extras, Pillow to core deps, pytest-cov to dev extras, coverage configuration sections
- `setup.py` - Removed install_requires/extras_require, added QUANTUM_COVERAGE env var for linetrace+gcov builds, added noqa: E402 to pre-existing late imports
- `.gitignore` - Added reports/ directory exclusion
- `Makefile` - Added coverage and coverage-clean targets, HAS_LCOV tool check, updated clean and help targets
- `src/quantum_language/grover.py` - Replaced direct qiskit imports with sim_backend.load_qasm and sim_backend.simulate
- `src/quantum_language/amplitude_estimation.py` - Replaced direct qiskit imports with sim_backend.load_qasm and sim_backend.simulate

## Decisions Made
- Used `dev` extras group for pytest-cov (not a separate `test` group) to keep extras manageable
- Added `noqa: E402` comments to 3 pre-existing late imports in setup.py that ruff flagged (these imports must follow sys.path manipulation for build_preprocessor)
- sim_backend.py guards qiskit and qiskit-aer separately via `_require_backend()` and `_require_simulator()` for future flexibility
- QUANTUM_COVERAGE adds both `-DCYTHON_TRACE=1` and `--coverage` (gcc gcov) flags for comprehensive instrumentation

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added noqa: E402 to pre-existing late imports in setup.py**
- **Found during:** Task 1 (dependency metadata update)
- **Issue:** Pre-commit ruff hook failed on 3 pre-existing E402 errors (imports after sys.path manipulation in setup.py lines 18-21)
- **Fix:** Added `# noqa: E402` comments to the 3 import lines that must follow the sys.path.insert() call
- **Files modified:** setup.py
- **Verification:** Pre-commit hooks pass
- **Committed in:** 5ec5a39 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Minimal -- suppressed pre-existing linting warnings that are intentional (sys.path manipulation pattern). No scope creep.

## Issues Encountered
- Pre-existing segfault in some test files (documented in STATE.md as known issue for Phase 87). Does not affect sim_backend migration -- all 45 grover/amplitude_estimation tests pass.
- Pre-existing test failure in `test_api_coverage.py::TestQintAPI::test_qint_default_width` (assert 3 == 8) -- unrelated to this plan's changes.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- sim_backend.py wrapper is in place for all future simulation work
- Coverage infrastructure ready for Plan 02 (baseline recording)
- pyproject.toml is now the authoritative source for all dependency metadata
- Makefile `coverage` target ready for use (Python/Cython coverage works; C coverage requires lcov installation)

## Self-Check: PASSED

All created files verified present. All 3 task commits verified in git log.

---
*Phase: 82-infrastructure-dependency-fixes*
*Completed: 2026-02-22*
