---
phase: 121-chess-engine-rewrite
plan: 01
subsystem: compile
tags: [compile, dag, opt-levels, oom-prevention, call-graph]

# Dependency graph
requires:
  - phase: 107-compile-infrastructure
    provides: "compile decorator with opt levels and call graph DAG"
provides:
  - "opt=1 DAG-only replay mode (skip inject_remapped_gates on cache hit)"
  - "DAG accumulation across multiple calls for opt=1"
affects: [121-02-chess-engine, chess-engine, compile]

# Tech tracking
tech-stack:
  added: []
  patterns: ["opt=1 conditional gate injection skip in _replay()"]

key-files:
  created:
    - tests/python/test_compile_dag_only.py
  modified:
    - src/quantum_language/compile.py
    - tests/python/test_call_graph.py

key-decisions:
  - "opt=1 replay skips inject_remapped_gates() but keeps ancilla allocation for correct qubit tracking"
  - "opt=1 DAG accumulates across calls (same CallGraphDAG reused) unlike opt=0/opt=2 which reset each call"
  - "Updated test_second_call_replaces_dag to test_second_call_accumulates_dag to match new opt=1 semantics"

patterns-established:
  - "Conditional gate injection: check self._opt before inject_remapped_gates in _replay()"
  - "DAG persistence: opt=1 reuses CallGraphDAG across top-level calls for cumulative stats"

requirements-completed: [CHESS-02, CHESS-05]

# Metrics
duration: 5min
completed: 2026-03-10
---

# Phase 121 Plan 01: opt=1 DAG-only Replay Summary

**opt=1 compile replay skips flat gate injection, recording DAG-only stats to prevent OOM on repeated compiled function calls**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-10T00:13:33Z
- **Completed:** 2026-03-10T00:18:38Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- opt=1 replay now skips `inject_remapped_gates()`, preventing flat circuit OOM on repeated calls
- DAG nodes still recorded on every call (capture and replay), enabling aggregate stats via `_call_graph.aggregate()`
- opt=1 DAG accumulates across calls, so chess engine can call compiled predicates hundreds of times and get full DAG stats
- All other opt levels (0, 2, 3) completely unchanged -- 104 tests pass across 4 test suites

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): Failing tests for opt=1 DAG-only replay** - `9f1bd11` (test)
2. **Task 1 (GREEN): Implement opt=1 DAG-only replay** - `a482d7a` (feat)
3. **Task 2: Regression verification + test update** - `9e8537b` (test)

_TDD task had separate RED/GREEN commits._

## Files Created/Modified
- `tests/python/test_compile_dag_only.py` - 4 new tests: gate skip, opt=0 unchanged, DAG 3-node accumulation, return value correctness
- `src/quantum_language/compile.py` - 2-line change: conditional `inject_remapped_gates()` skip + DAG reuse for opt=1
- `tests/python/test_call_graph.py` - Updated `test_second_call_replaces_dag` to `test_second_call_accumulates_dag`

## Decisions Made
- opt=1 replay skips inject_remapped_gates() but keeps ancilla allocation for correct qubit tracking in DAG nodes and return value building
- opt=1 DAG accumulates across calls (same CallGraphDAG object reused) -- this differs from opt=0/opt=2 which create fresh DAGs each call, because opt=1's purpose is aggregate stats across many calls
- Updated existing test expectation rather than adding skip, since the old "replaces DAG" behavior was wrong for opt=1's intended use case

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] DAG accumulation across calls for opt=1**
- **Found during:** Task 1 (GREEN phase, test 3 failing)
- **Issue:** Plan said to only modify `_replay()`, but each top-level call created a fresh CallGraphDAG, so DAG only ever had 1 node. The plan's test expectation of 3 nodes could not work.
- **Fix:** Modified `__call__()` to reuse existing `self._call_graph` for opt=1 instead of creating fresh DAG each time (2 lines changed in DAG building block)
- **Files modified:** src/quantum_language/compile.py
- **Verification:** Test 3 passes with 3 DAG nodes after 3 calls
- **Committed in:** a482d7a (Task 1 GREEN commit)

**2. [Rule 1 - Bug] Numpy array comparison in test**
- **Found during:** Task 1 (GREEN phase, test 4)
- **Issue:** `r2.qubits != r1.qubits` raises ValueError for numpy arrays with >1 element
- **Fix:** Changed to `not (r2.qubits == r1.qubits).all()`
- **Files modified:** tests/python/test_compile_dag_only.py
- **Verification:** Test 4 passes
- **Committed in:** a482d7a (Task 1 GREEN commit)

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 bug)
**Impact on plan:** Both fixes necessary for correctness. The DAG accumulation fix is essential for the chess engine use case. No scope creep.

## Issues Encountered
- `test_compile.py` referenced in plan does not exist -- used `test_compile_performance.py` and `test_compile_nested_with.py` instead
- `test_compile_nested_with.py::test_replay_both_true` fails but confirmed pre-existing (fails on prior commit without changes)
- `pytest --timeout` flag not available (no pytest-timeout plugin) -- ran without timeout

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- opt=1 compile mode is ready for the chess engine (Plan 02) to use without OOM
- DAG aggregate stats available via `compiled_func._call_graph.aggregate()` for circuit analysis
- Pre-existing test_compile_nested_with failure is a known carry-forward issue (documented in STATE.md)

## Self-Check: PASSED

- All 3 source/test files verified present on disk
- All 3 task commits verified in git log (9f1bd11, a482d7a, 9e8537b)
- SUMMARY.md created at expected path

---
*Phase: 121-chess-engine-rewrite*
*Completed: 2026-03-10*
