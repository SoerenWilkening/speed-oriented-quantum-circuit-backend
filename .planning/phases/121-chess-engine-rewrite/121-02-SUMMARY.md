---
phase: 121-chess-engine-rewrite
plan: 02
subsystem: examples
tags: [chess, quantum-walk, nested-with, qarray-2d, compile, diffusion, dag]

# Dependency graph
requires:
  - phase: 121-chess-engine-rewrite
    provides: "opt=1 DAG-only replay mode (Plan 01)"
  - phase: 117-nested-controls
    provides: "nested with-blocks via AND-ancilla composition"
  - phase: 120-2d-qarrays
    provides: "2D qarray construction with dim=(rows, cols)"
provides:
  - "Complete quantum chess engine example demonstrating v9.0 features"
  - "Fix: skip forward-call tracking for non-inverse compiled functions on replay"
affects: [chess-engine, examples, compile]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Use += 1 instead of ^= ql.qbool(True) for toggles inside with blocks (controlled context)"
    - "Self-inverse XOR uncomputation via double-call instead of .inverse()"
    - "3-level nested with-blocks for multi-condition legality checking"

key-files:
  created: []
  modified:
    - examples/chess_engine.py
    - src/quantum_language/compile.py

key-decisions:
  - "Use += 1 for qbool toggles inside with blocks (^= ql.qbool(True) raises NotImplementedError in controlled context)"
  - "Uncompute attack map via double forward call (XOR is self-inverse) instead of .inverse() due to forward-call tracking conflict"
  - "Skip forward-call tracking for non-inverse compiled functions to allow repeated replay on same qubits"
  - "compute_white_attacks uses @ql.compile(opt=1) without inverse=True since uncomputation is done via self-inverse double-call"

patterns-established:
  - "Controlled qbool toggle: use += 1 inside with blocks, ^= ql.qbool(True) outside"
  - "Self-inverse uncomputation: call compiled XOR function twice instead of using .inverse()"
  - "Forward-call tracking: only track when inverse support is requested"

requirements-completed: [CHESS-01, CHESS-03, CHESS-04, CHESS-05]

# Metrics
duration: 7min
completed: 2026-03-10
---

# Phase 121 Plan 02: Chess Engine Rewrite Summary

**Quantum chess engine with nested with-blocks for 3-condition legality, 2D qarrays for 8x8 boards, compiled walk operators with DAG-only replay, and inline diffusion**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-10T00:21:03Z
- **Completed:** 2026-03-10T00:28:24Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Complete rewrite of chess_engine.py as readable quantum chess engine (199 lines)
- King + 2 Knights vs King endgame with quantum walk search tree over 8 directions
- 3-level nested with-blocks for move legality: piece-exists, no-friendly-capture, not-in-check
- Compiled attack predicate called twice per direction (XOR self-inverse for uncomputation)
- Walk step compiled with @ql.compile(opt=1) and replayed 3 times from DAG
- Circuit stats: 14,004 gates, 693 depth, 277 qubits, 75,117 T-count, 20 DAG nodes
- Fix: compile.py forward-call tracking now skipped for non-inverse functions

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite chess_engine.py with full v9.0 features** - `008082b` (feat)
2. **Task 2: Smoke test and existing test regression check** - (verification only, no code changes)

## Files Created/Modified
- `examples/chess_engine.py` - Complete quantum chess engine: board setup, compiled attack predicates, walk operators (R_A/R_B/diffusion), DAG stats output
- `src/quantum_language/compile.py` - Skip forward-call tracking for non-inverse compiled functions on replay

## Decisions Made
- Used `+= 1` instead of `^= ql.qbool(True)` for all qbool toggles inside `with` blocks because the framework raises `NotImplementedError: Controlled quantum-quantum XOR not yet supported` in controlled contexts. The `+= 1` on a 1-bit qbool is equivalent (classical-quantum addition, which supports controlled context).
- Used self-inverse XOR uncomputation (calling `compute_white_attacks` twice) instead of `.inverse()` because the forward-call tracking system conflicts with calling a function on the same qubits from within a nested compiled context.
- Removed `inverse=True` from `compute_white_attacks` since `.inverse()` is not used (double-call pattern replaces it).
- Fixed `compile.py` to skip forward-call tracking when `self._inverse_func is None` -- this allows non-inverse compiled functions to be replayed multiple times on the same qubits without error.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Controlled quantum-quantum XOR not supported**
- **Found during:** Task 1 (first run of chess_engine.py)
- **Issue:** `^= ql.qbool(True)` inside nested `with` blocks raises `NotImplementedError: Controlled quantum-quantum XOR not yet supported`
- **Fix:** Changed all board toggles inside `with` blocks to `+= 1` (classical-quantum addition, which works in controlled context)
- **Files modified:** examples/chess_engine.py
- **Verification:** chess_engine.py runs to completion
- **Committed in:** 008082b

**2. [Rule 3 - Blocking] .inverse() forward-call tracking conflict**
- **Found during:** Task 1 (second error on first run)
- **Issue:** `compute_white_attacks.inverse()` raises `ValueError: No prior forward call` because the forward call was captured in a different controlling context
- **Fix:** Changed to self-inverse uncomputation (call function twice, since XOR is self-inverse) and removed `inverse=True`
- **Files modified:** examples/chess_engine.py
- **Verification:** chess_engine.py runs to completion
- **Committed in:** 008082b

**3. [Rule 3 - Blocking] Forward-call tracking prevents walk_step replay**
- **Found during:** Task 1 (third error: Step 2 replay fails)
- **Issue:** `walk_step` replay records forward call in `_forward_calls` dict; second replay sees existing entry and raises error
- **Fix:** Modified compile.py to pass `track_forward=False` when `self._inverse_func is None` (non-inverse functions don't need forward tracking)
- **Files modified:** src/quantum_language/compile.py
- **Verification:** chess_engine.py completes all 3 walk steps; test_compile_dag_only.py passes (4/4)
- **Committed in:** 008082b

---

**Total deviations:** 3 auto-fixed (3 blocking)
**Impact on plan:** All three fixes were necessary to make the chess engine functional. The `+= 1` pattern and self-inverse double-call are equivalent to the planned `^= ql.qbool(True)` and `.inverse()` patterns -- same quantum behavior, different API surface. The compile.py fix is a 1-line change that correctly restricts forward tracking to functions that actually need it.

## Issues Encountered
- `test_chess.py` has 12 pre-existing failures (all importing `get_legal_moves_and_oracle` which doesn't exist) -- unrelated to our changes
- `test_qarray.py` referenced in plan does not exist -- used `test_bug02_qarray_imul.py` instead (25 tests, all pass)
- `pytest --timeout` flag not available (no pytest-timeout plugin)

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Chess engine example is complete and demonstrates the full v9.0 feature set
- All framework features verified: nested with-blocks, 2D qarrays, @ql.compile(opt=1), ql.diffusion()
- DAG stats accessible via `walk_step._call_graph.aggregate()`
- Pre-existing test failures in test_chess.py (oracle imports) and test_compile_nested_with.py (replay_both_true) are carry-forward issues

## Self-Check: PASSED

- All 2 source files verified present on disk
- Task 1 commit 008082b verified in git log
- SUMMARY.md created at expected path

---
*Phase: 121-chess-engine-rewrite*
*Completed: 2026-03-10*
