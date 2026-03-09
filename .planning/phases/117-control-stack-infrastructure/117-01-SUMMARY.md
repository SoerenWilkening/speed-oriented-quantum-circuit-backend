---
phase: 117-control-stack-infrastructure
plan: 01
subsystem: infra
tags: [cython, control-stack, toffoli, ccx, backward-compat]

# Dependency graph
requires: []
provides:
  - "_control_stack list in _core.pyx with push/pop/get/set accessors"
  - "emit_ccx raw Toffoli gate emission in _gates.pyx"
  - "_toffoli_and and _uncompute_toffoli_and AND-ancilla helpers"
  - "Backward-compat wrappers for _get_controlled, _set_controlled, etc."
affects: [117-02, 118-nested-control-composition]

# Tech tracking
tech-stack:
  added: []
  patterns: [stack-based-control-context, toffoli-and-ancilla-lifecycle]

key-files:
  created:
    - tests/python/test_control_stack.py
  modified:
    - src/quantum_language/_core.pyx
    - src/quantum_language/_gates.pyx
    - src/quantum_language/_gates.pxd
    - src/quantum_language/qint.pyx
    - src/quantum_language/qint_preprocessed.pyx
    - src/quantum_language/compile.py

key-decisions:
  - "Replaced _controlled/_control_bool/_list_of_controls flat globals with single _control_stack list"
  - "Backward-compat wrappers (_set_controlled, _set_control_bool) are no-ops -- controlled state derived from stack depth"
  - "Updated __enter__/__exit__ to use _push_control/_pop_control for immediate single-level equivalence"
  - "Updated compile.py save/restore to use _get_control_stack/_set_control_stack shallow copy"

patterns-established:
  - "Control stack tuple format: (qbool_ref, and_ancilla_or_None) for consistent multi-level support"
  - "emit_ccx does NOT check controlled context -- raw primitive for stack-internal use"

requirements-completed: [CTRL-02, CTRL-03]

# Metrics
duration: 14min
completed: 2026-03-09
---

# Phase 117 Plan 01: Control Stack Infrastructure Summary

**Stack-based control context replacing flat globals, with emit_ccx and Toffoli AND-ancilla primitives for nested control composition**

## Performance

- **Duration:** 14 min
- **Started:** 2026-03-09T18:14:50Z
- **Completed:** 2026-03-09T18:29:10Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Replaced _controlled/_control_bool/_list_of_controls flat globals with single _control_stack list in _core.pyx
- Added emit_ccx raw Toffoli gate, _toffoli_and allocator, and _uncompute_toffoli_and reversal to _gates.pyx
- Updated __enter__/__exit__ in qint.pyx to use stack push/pop semantics
- Updated compile.py control context save/restore to use full stack
- All 18 unit tests pass, plus 3 single-level regression tests

## Task Commits

Each task was committed atomically:

1. **Task 1: Create unit test scaffold for control stack primitives** - `155454b` (test)
2. **Task 2: Implement control stack in _core.pyx and gate helpers in _gates.pyx/_gates.pxd** - `3832c7f` (feat)

## Files Created/Modified
- `tests/python/test_control_stack.py` - 18 pytest functions covering stack push/pop, emit_ccx, toffoli_and, backward-compat
- `src/quantum_language/_core.pyx` - _control_stack global + push/pop/get/set accessors + backward-compat wrappers + circuit reset
- `src/quantum_language/_gates.pyx` - emit_ccx, _toffoli_and, _uncompute_toffoli_and functions
- `src/quantum_language/_gates.pxd` - ccx C declaration for Cython
- `src/quantum_language/qint.pyx` - __enter__/__exit__ updated to use _push_control/_pop_control
- `src/quantum_language/qint_preprocessed.pyx` - Same __enter__/__exit__ updates (sync)
- `src/quantum_language/compile.py` - Save/restore control context via stack shallow copy

## Decisions Made
- Replaced three flat globals with single `_control_stack` list -- stack depth implies controlled state
- Backward-compat wrappers are no-ops: `_set_controlled()` and `_set_control_bool()` do nothing, as state is managed by push/pop
- Updated `__enter__`/`__exit__` immediately (rather than deferring to Plan 02) because making `_set_controlled`/`_set_control_bool` no-ops broke single-level conditionals
- Updated compile.py save/restore to use stack-based context for the same reason

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Updated __enter__/__exit__ to use stack push/pop**
- **Found during:** Task 2 (verification)
- **Issue:** Making _set_controlled/_set_control_bool no-ops broke single-level with-blocks because __enter__ still called _set_controlled(True) and _set_control_bool(self)
- **Fix:** Updated __enter__ to call _push_control(self, None) and __exit__ to call _pop_control(), plus matching changes in qint_preprocessed.pyx
- **Files modified:** src/quantum_language/qint.pyx, src/quantum_language/qint_preprocessed.pyx
- **Verification:** test_nested_with_blocks.py::TestSingleLevelConditional all 3 tests pass
- **Committed in:** 3832c7f (Task 2 commit)

**2. [Rule 3 - Blocking] Updated compile.py save/restore to use stack**
- **Found during:** Task 2 (verification)
- **Issue:** compile.py saved/restored control context via _set_controlled/_set_control_bool which are now no-ops
- **Fix:** Updated to use _get_control_stack()/_set_control_stack() for save/restore
- **Files modified:** src/quantum_language/compile.py
- **Verification:** test_compile_performance.py passes
- **Committed in:** 3832c7f (Task 2 commit)

**3. [Rule 1 - Bug] Fixed test expectation for CCX self-adjoint cancellation**
- **Found during:** Task 2 (test_uncompute_toffoli_and)
- **Issue:** Test expected 2 CCX gates after toffoli_and + uncompute_toffoli_and, but circuit auto-cancels self-adjoint gates on same qubits
- **Fix:** Changed test to verify _is_uncomputed flag and allocated_qubits=False instead of CCX count
- **Files modified:** tests/python/test_control_stack.py
- **Verification:** All 18 tests pass
- **Committed in:** 3832c7f (Task 2 commit)

---

**Total deviations:** 3 auto-fixed (1 bug fix, 2 blocking)
**Impact on plan:** All auto-fixes necessary for correctness. The __enter__/__exit__ and compile.py updates were essential because making _set_controlled/_set_control_bool no-ops requires callers to use the new push/pop API. No scope creep.

## Issues Encountered
None beyond the deviations documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Control stack infrastructure complete, ready for Plan 02 to wire __enter__/__exit__ multi-level composition
- _push_control/_pop_control provide the API that Phase 118 will use for AND-ancilla composition
- emit_ccx and _toffoli_and provide the gate primitives Phase 118 needs

## Self-Check: PASSED

All files verified present. All commit hashes verified in git log.

---
*Phase: 117-control-stack-infrastructure*
*Completed: 2026-03-09*
