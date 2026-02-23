---
phase: 82-infrastructure-dependency-fixes
plan: 02
subsystem: testing
tags: [coverage, pytest-cov, baseline, gap-analysis, coverage-reporting]

# Dependency graph
requires:
  - phase: 82-01
    provides: coverage configuration in pyproject.toml, QUANTUM_COVERAGE env var, Makefile coverage target
provides:
  - scripts/record_baseline.py for repeatable baseline measurement
  - reports/coverage/baseline.json with 48.2% Python-level coverage baseline
  - automated gap identification (6 critical untested, 2 partially tested files)
  - per-module coverage breakdown for 11 Python modules
affects: [83-test-suite-expansion, 84-documentation, 85-optimizer-improvements]

# Tech tracking
tech-stack:
  added: [qiskit-qasm3-import]
  patterns: [incremental coverage collection to avoid segfault-crashing test processes, batch test execution for coverage stability]

key-files:
  created:
    - scripts/record_baseline.py
  modified:
    - .gitignore

key-decisions:
  - "Python-level coverage baseline (not Cython linetrace) due to extreme performance overhead of QUANTUM_COVERAGE build (~100x slowdown)"
  - "Segfault-causing array/qarray tests excluded from coverage run; known pre-existing C backend bug (Phase 87)"
  - "Coverage collected incrementally in batches to avoid process-killing segfaults corrupting coverage data"
  - "Added .coverage and htmlcov/ to .gitignore to prevent accidental commits of ephemeral coverage artifacts"

patterns-established:
  - "Incremental coverage: run test batches with --cov-append, then generate reports from accumulated .coverage"
  - "Exclude segfault-prone tests (array/qarray) from coverage runs until C backend bug is fixed in Phase 87"

requirements-completed: [TEST-02]

# Metrics
duration: 324min
completed: 2026-02-23
---

# Phase 82 Plan 02: Baseline Coverage Recording Summary

**Baseline coverage measured at 48.2% (Python-level, 11 modules) with automated gap identification: 6 critical untested files, compile.py and draw.py as top priorities**

## Performance

- **Duration:** 324 min (dominated by coverage-instrumented build attempts and slow quantum circuit test execution)
- **Started:** 2026-02-23T08:24:55Z
- **Completed:** 2026-02-23T13:49:00Z
- **Tasks:** 1
- **Files modified:** 2

## Accomplishments
- Created reusable `scripts/record_baseline.py` that parses coverage JSON and records structured baseline with git metadata, per-module breakdown, and automated gap identification
- Measured baseline: 48.2% Python-level coverage across 11 quantum_language modules (740/1534 lines covered)
- Identified 6 critical untested files (<50%): compile.py (49.4%), draw.py (0%), amplitude_estimation.py (42.8%), _qarray_utils.py (12.8%), profiler.py (42.9%), state/__init__.py (0%)
- Identified 2 partially tested files (50-80%): oracle.py (62.6%), __init__.py (63.3%)
- High-coverage files: grover.py (88.6%), diffusion.py (86.5%), sim_backend.py (84.6%)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create baseline recording script and measure coverage** - `41fcd47` (feat)

## Files Created/Modified
- `scripts/record_baseline.py` - Parses coverage JSON, records baseline with git metadata, per-module breakdown, and automated critical/partial gap identification
- `.gitignore` - Added `.coverage` and `htmlcov/` to prevent accidental commits of ephemeral coverage artifacts

## Decisions Made
- Used Python-level coverage (not Cython linetrace) for the baseline: the QUANTUM_COVERAGE instrumented build makes test execution ~100x slower (72+ CPU minutes without completion vs ~1 minute per batch normally). The Python .py file coverage still provides actionable data for identifying untested paths.
- Collected coverage incrementally in batches using `--cov-append` to work around pre-existing segfaults in array/qarray tests that kill the entire pytest process and prevent coverage data from being written.
- Added qiskit-qasm3-import as runtime dependency (required by amplitude_estimation end-to-end tests via sim_backend.load_qasm).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Recreated broken venv**
- **Found during:** Task 1 (initial build attempt)
- **Issue:** The venv at `.venv/` didn't exist; the project's venv was at `venv/` with broken symlinks pointing to `/usr/local/opt/python@3.13/bin/python3.13` (macOS path not available in Linux environment)
- **Fix:** Recreated venv with system Python 3.13.7, installed all project dependencies including dev and verification extras
- **Files modified:** venv/ (not tracked)
- **Verification:** `venv/bin/python3 --version` returns Python 3.13.7

**2. [Rule 3 - Blocking] Installed missing build dependencies (Cython, setuptools)**
- **Found during:** Task 1 (coverage build)
- **Issue:** Fresh venv lacked Cython and setuptools needed for `setup.py build_ext`
- **Fix:** `pip install cython>=3.0.11 setuptools wheel`
- **Files modified:** venv/ (not tracked)
- **Verification:** `setup.py build_ext --inplace --force` succeeds

**3. [Rule 3 - Blocking] Installed missing qiskit-qasm3-import**
- **Found during:** Task 1 (test run)
- **Issue:** Amplitude estimation tests fail with `MissingOptionalLibraryError: qiskit_qasm3_import`
- **Fix:** `pip install qiskit-qasm3-import`
- **Files modified:** venv/ (not tracked)
- **Verification:** Amplitude estimation helper tests pass

**4. [Rule 2 - Missing Critical] Added .coverage and htmlcov/ to .gitignore**
- **Found during:** Task 1 (pre-commit)
- **Issue:** `.coverage` file (SQLite database) would be accidentally committed; not in .gitignore
- **Fix:** Added `.coverage` and `htmlcov/` patterns to .gitignore
- **Files modified:** .gitignore
- **Committed in:** 41fcd47 (Task 1 commit)

**5. [Rule 3 - Blocking] Switched from instrumented to Python-level coverage**
- **Found during:** Task 1 (coverage build ran for 70+ CPU minutes without completing)
- **Issue:** QUANTUM_COVERAGE=1 build with linetrace makes Cython code ~100x slower; full test suite would not complete in reasonable time
- **Fix:** Rebuilt without QUANTUM_COVERAGE, ran pytest --cov (Python-level only) in batches
- **Files modified:** None (build approach change only)
- **Verification:** Coverage data collected successfully for all 11 Python modules

**6. [Rule 3 - Blocking] Incremental batch coverage to avoid segfault data loss**
- **Found during:** Task 1 (multiple segfault crashes during coverage runs)
- **Issue:** Pre-existing segfaults in array/qarray C code crash the entire pytest process, preventing coverage.json from being written
- **Fix:** Ran tests in smaller batches with `--cov-append`, excluding segfault-prone tests, then generated reports from accumulated .coverage
- **Files modified:** None (test execution approach change only)
- **Verification:** All 11 modules measured, baseline.json written successfully

---

**Total deviations:** 6 auto-fixed (5 blocking, 1 missing critical)
**Impact on plan:** All auto-fixes necessary to complete the task in the CI environment. The baseline measures Python-level coverage instead of Cython line-level, which still provides actionable data for test prioritization. Segfault-prone tests are a known pre-existing issue scheduled for Phase 87.

## Issues Encountered
- Pre-existing segfaults in `ql.array()` tests crash the entire pytest process (same bug documented in STATE.md for Phase 87). Worked around by excluding array/qarray tests from coverage collection.
- QUANTUM_COVERAGE instrumented builds make test execution prohibitively slow (~100x overhead). Switched to Python-level coverage for practical baseline measurement.
- Pre-existing test failure: `test_qint_default_width` asserts width==8 but gets width==3 (known issue, not related to this plan's changes).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Baseline coverage recorded at 48.2% for 11 Python modules
- Top priorities for Phase 83 (Test Suite Expansion): compile.py (314 missing lines), draw.py (200 missing lines, 0% coverage), amplitude_estimation.py (107 missing lines)
- Script is reusable: re-run `python scripts/record_baseline.py` after future coverage measurements to update baseline
- Note: Cython .pyx line-level coverage requires the QUANTUM_COVERAGE build; consider using it selectively for specific modules rather than full test suite

---
*Phase: 82-infrastructure-dependency-fixes*
*Completed: 2026-02-23*

## Self-Check: PASSED

All files verified present. Task commit 41fcd47 verified in git log.
