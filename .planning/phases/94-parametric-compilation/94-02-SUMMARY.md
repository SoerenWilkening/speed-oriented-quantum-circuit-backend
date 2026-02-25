---
phase: 94-parametric-compilation
plan: 02
subsystem: compile
tags: [parametric, topology, probe, replay, toffoli-cq, structural-detection, cache]

# Dependency graph
requires:
  - phase: 94-parametric-compilation
    provides: Mode-aware cache keys and parametric=True API surface (Plan 01)
provides:
  - _extract_topology() pure function for hashable gate structure signatures
  - _extract_angles() pure function for angle extraction from gate sequences
  - _apply_angles() pure function for fresh gate list construction from template
  - CompiledFunc._parametric_call() lifecycle method (probe, detect, replay, fallback)
  - Automatic Toffoli CQ structural detection and per-value fallback
  - Defensive topology verification on parametric-safe fast path
  - clear_cache() resets all parametric state
affects: [94-03-PLAN]

# Tech tracking
tech-stack:
  added: []
  patterns: [two-capture structural probe for parametric detection, defensive topology verification on every new classical value]

key-files:
  created: []
  modified:
    - src/quantum_language/compile.py

key-decisions:
  - "Parametric-safe fast path still captures per-value on cache miss (simplest correct implementation) rather than reconstructing from template"
  - "Defensive topology check on every new classical value guards against runtime divergence edge cases"
  - "Two-capture probe: first call stores topology, second call with different classical args compares; topology match marks parametric-safe, mismatch marks structural"
  - "_reset_for_circuit preserves parametric state across circuit resets (only clears block reference) while clear_cache fully resets"

patterns-established:
  - "Two-capture probe pattern: call 1 captures baseline topology, call 2 with different args probes for structural vs parametric"
  - "Topology = (gate_type, target, controls_tuple, num_controls) per gate -- angles excluded for structural comparison"

requirements-completed: [PAR-02, PAR-03]

# Metrics
duration: 4min
completed: 2026-02-25
---

# Phase 94 Plan 02: Parametric Compilation Probe and Replay Lifecycle Summary

**Two-capture structural probe detecting topology-safe vs topology-dependent functions with automatic Toffoli CQ fallback to per-value caching**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-25T23:08:07Z
- **Completed:** 2026-02-25T23:12:07Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- Topology extraction helpers (_extract_topology, _extract_angles, _apply_angles) enable structural comparison of gate sequences ignoring rotation angles
- Full parametric lifecycle in CompiledFunc._parametric_call: unknown -> probed -> parametric-safe or structural, with state machine handling all transitions
- Toffoli CQ operations automatically detected as structural (value-dependent topology) and silently fall back to per-value caching
- Defensive topology verification on the parametric-safe fast path catches runtime divergence edge cases
- clear_cache() fully resets parametric state; _reset_for_circuit preserves probe state across circuit resets
- Toffoli CQ fallback fully documented in compile() docstring
- All 16 parametric-specific tests pass (test_parametric.py)

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement structural topology extraction and comparison** - `01b3306` (feat) - pre-existing commit
2. **Task 2: Implement parametric capture, probe, and replay in CompiledFunc** - `01b3306` (feat) - pre-existing commit

**Plan metadata:** (pending)

_Note: Both tasks were already committed in a prior session as a single atomic commit (01b3306). Verification in this session confirmed all implementations are correct and all 16 parametric tests pass._

## Files Created/Modified
- `src/quantum_language/compile.py` - _extract_topology(), _extract_angles(), _apply_angles() pure helpers; CompiledFunc.__init__ parametric state fields; __call__ parametric routing; _parametric_call() lifecycle method; clear_cache() parametric reset; compile() docstring Toffoli CQ documentation

## Decisions Made
- Parametric-safe fast path still captures per-value on cache miss rather than reconstructing from template+angles (simplest correct implementation, avoids complexity around return value construction and ancilla allocation)
- Defensive topology check on every new classical value provides safety against runtime divergence without significant overhead
- _reset_for_circuit only clears _parametric_block (since cache entries are gone) but preserves probed/safe state, while clear_cache does a full reset

## Deviations from Plan

None - plan executed exactly as written. All code was already present from a prior commit (01b3306).

## Issues Encountered
- Both tasks were already implemented and committed in a prior session (commit 01b3306). Verified correctness via topology helper unit tests, QFT parametric detection, Toffoli CQ structural detection, clear_cache reset, and full parametric test suite (16/16 pass).
- QFT mode `x += val` addition with Toffoli CQ encoding also shows structural topology differences (False for parametric-safe). This is correct behavior -- the CQ adder generates value-dependent X-gate patterns regardless of arithmetic mode.
- 14 pre-existing test failures in test_compile.py (qarray, replay gate count, nesting, auto-uncompute) are unrelated to this plan's changes. Confirmed by running the same tests against pre-phase-94 code (commit d53cc5c).

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Full parametric lifecycle is operational, ready for Plan 03 (parametric compilation verification tests)
- All probe states (unknown, probed, parametric-safe, structural) are exercised and verified
- The _parametric_safe and _parametric_probed attributes are accessible for test introspection

## Self-Check: PASSED

- FOUND: 94-02-SUMMARY.md
- FOUND: 01b3306 (Task 1+2 commit)
- FOUND: src/quantum_language/compile.py

---
*Phase: 94-parametric-compilation*
*Completed: 2026-02-25*
