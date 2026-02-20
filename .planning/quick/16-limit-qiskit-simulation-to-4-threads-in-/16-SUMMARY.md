---
phase: 16-limit-qiskit-threads
plan: 01
subsystem: testing
tags: [qiskit, aer-simulator, thread-limiting, performance]

# Dependency graph
requires: []
provides:
  - "Thread-limited AerSimulator across all tests and scripts (max_parallel_threads=4)"
affects: [all-test-files, scripts]

# Tech tracking
tech-stack:
  added: []
  patterns: ["AerSimulator instantiation always includes max_parallel_threads=4"]

key-files:
  created: []
  modified:
    - tests/conftest.py
    - tests/test_bitwise_mixed.py
    - tests/test_modular.py
    - tests/test_cla_addition.py
    - tests/test_copy.py
    - tests/test_toffoli_division.py
    - tests/test_mul_addsub.py
    - tests/test_hardcoded_sequences.py
    - tests/test_copy_binops.py
    - tests/test_compare_preservation.py
    - tests/test_toffoli_cq_reduction.py
    - tests/test_toffoli_addition.py
    - tests/test_toffoli_multiplication.py
    - tests/test_div.py
    - tests/test_array_verify.py
    - tests/test_uncomputation.py
    - tests/test_toffoli_hardcoded.py
    - tests/test_mod.py
    - tests/python/test_cross_backend.py
    - tests/python/test_oracle.py
    - tests/python/test_cla_verification.py
    - tests/python/test_branch_superposition.py
    - tests/python/test_cla_bk_algorithm.py
    - tests/python/test_diffusion.py
    - scripts/verify_circuit.py

key-decisions:
  - "Used max_parallel_threads=4 kwarg (Qiskit Aer native) rather than environment variable approach"

patterns-established:
  - "AerSimulator instantiation pattern: always include max_parallel_threads=4 to prevent CPU monopolization"

requirements-completed: []

# Metrics
duration: 22min
completed: 2026-02-20
---

# Quick Task 16: Limit Qiskit Simulation to 4 Threads Summary

**Added max_parallel_threads=4 to all 35 AerSimulator instantiations across 25 files to prevent CPU thread exhaustion during test runs and CI**

## Performance

- **Duration:** 22 min
- **Started:** 2026-02-20T21:30:08Z
- **Completed:** 2026-02-20T21:52:14Z
- **Tasks:** 1
- **Files modified:** 25

## Accomplishments
- Added `max_parallel_threads=4` to all 35 AerSimulator() calls across tests/ and scripts/
- Zero ungated AerSimulator instances remain (verified via grep)
- Three patterns handled: no-args `AerSimulator()`, with-method `AerSimulator(method="statevector")`, and string-template in `scripts/verify_circuit.py`
- Tests pass with thread limit applied (88 tests verified in oracle, branch_superposition, diffusion suites)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add max_parallel_threads=4 to all AerSimulator instantiations** - `8aca1bc` (chore)

## Files Created/Modified
- `tests/conftest.py` - Thread-limited AerSimulator in verify_circuit fixture
- `tests/test_bitwise_mixed.py` - statevector simulator thread limit
- `tests/test_modular.py` - statevector simulator thread limit
- `tests/test_cla_addition.py` - statevector simulator thread limit
- `tests/test_copy.py` - statevector simulator thread limit
- `tests/test_toffoli_division.py` - matrix_product_state simulator thread limit
- `tests/test_mul_addsub.py` - statevector simulator thread limit
- `tests/test_hardcoded_sequences.py` - statevector simulator thread limit
- `tests/test_copy_binops.py` - statevector simulator thread limit (2 instances)
- `tests/test_compare_preservation.py` - statevector simulator thread limit
- `tests/test_toffoli_cq_reduction.py` - statevector simulator thread limit
- `tests/test_toffoli_addition.py` - statevector simulator thread limit
- `tests/test_toffoli_multiplication.py` - statevector simulator thread limit
- `tests/test_div.py` - matrix_product_state simulator thread limit
- `tests/test_array_verify.py` - statevector simulator thread limit
- `tests/test_uncomputation.py` - statevector simulator thread limit
- `tests/test_toffoli_hardcoded.py` - statevector simulator thread limit
- `tests/test_mod.py` - matrix_product_state simulator thread limit
- `tests/python/test_cross_backend.py` - both MPS and statevector thread limits
- `tests/python/test_oracle.py` - default simulator thread limit (2 instances)
- `tests/python/test_cla_verification.py` - statevector simulator thread limit (4 instances)
- `tests/python/test_branch_superposition.py` - default simulator thread limit
- `tests/python/test_cla_bk_algorithm.py` - statevector and MPS thread limits (4 instances)
- `tests/python/test_diffusion.py` - statevector and default simulator thread limits
- `scripts/verify_circuit.py` - string-template statevector simulator thread limit

## Decisions Made
- Used `max_parallel_threads=4` as a keyword argument on each AerSimulator instantiation (Qiskit Aer native option) rather than an environment variable approach, providing explicit per-call control

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed pre-existing ruff F841 lint errors in test_branch_superposition.py**
- **Found during:** Task 1 (commit attempt)
- **Issue:** Pre-commit ruff hook rejected commit due to 2 pre-existing F841 (unused variable) errors: `result` on line 244 and `total` on line 413
- **Fix:** Removed assignment to `result` (call kept), removed unused `total` variable
- **Files modified:** `tests/python/test_branch_superposition.py`
- **Verification:** ruff passes, tests pass
- **Committed in:** `8aca1bc` (part of task commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Pre-existing lint fix required by pre-commit hook. No scope creep.

## Issues Encountered
- Full test suite (`pytest tests/python/ -v`) crashes with segfault in unrelated test_api_coverage.py (pre-existing C backend segfault in array test). Verified modified files pass by running targeted test suites (88 tests in oracle, branch_superposition, diffusion).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All AerSimulator calls now thread-limited; future AerSimulator additions should follow the established pattern
- No blockers

---
*Quick Task: 16-limit-qiskit-threads*
*Completed: 2026-02-20*
