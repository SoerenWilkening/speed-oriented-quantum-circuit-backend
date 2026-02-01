# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-01)

**Core value:** Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.
**Current focus:** v1.6 Array & Comparison Fixes

## Current Position

Phase: 35 of 36 (Comparison Bug Fixes)
Plan: 3 of 3 in current phase
Status: Phase 35 complete
Last activity: 2026-02-01 — Completed 35-03-PLAN.md (gap closure)

Progress: [█████░░░░░] 50% (v1.6: 4/5 plans)

## Performance Metrics

**Velocity:**
- Total plans completed: 123 (v1.0: 41, v1.1: 13, v1.2: 10, v1.3: 16, v1.4: 6, v1.5: 33, v1.6: 4)
- Average duration: ~13 min/plan
- Total execution time: ~21.3 hours

**By Milestone:**

| Milestone | Phases | Plans | Status |
|-----------|--------|-------|--------|
| v1.0 MVP | 1-10 | 41 | Complete (2026-01-27) |
| v1.1 QPU State | 11-15 | 13 | Complete (2026-01-28) |
| v1.2 Uncomputation | 16-20 | 10 | Complete (2026-01-28) |
| v1.3 Package & Array | 21-24 | 16 | Complete (2026-01-29) |
| v1.4 OpenQASM Export | 25-27 | 6 | Complete (2026-01-30) |
| v1.5 Bug Fixes & Verification | 28-33 | 33 | Complete (2026-02-01) |
| v1.6 Array & Comparison Fixes | 34-36 | 5 | In Progress |

## Accumulated Context

### Decisions

See PROJECT.md Key Decisions table for full history.

**Recent (v1.6):**
- Positional value + keyword width pattern for qint constructor calls in arrays (34-01)
- Comparison results persist without auto-uncompute (matches Phase 29-16 pattern) (35-01)
- MSB-first qubit ordering for C backend comparison operations (35-01)
- Widened (n+1)-bit comparison applied to __lt__ (35-02)
- Right-aligned storage means index [63] is always MSB (35-02)
- LSB-aligned CNOT bit copies for widened comparisons (35-03)
- Target index formula: 64 - comp_width + i_bit for proper zero-extension (35-03)

### Blockers/Concerns

**v1.6 targets (3 phases, 5 plans):**
- ✓ BUG-ARRAY-INIT: Fixed in 34-01 (qint constructor parameter swap)
- ✓ Array element-wise ops: Fixed by BUG-ARRAY-INIT resolution (9 tests pass)
- ✓ BUG-CMP-01: Fixed in 35-01 (dual-bug: GC gate reversal + bit-order reversal, 488 tests now pass)
- ✓ BUG-CMP-02: Fixed in 35-03 (LSB-aligned CNOT copies for unsigned comparison semantics)
- ✓ BUG-CMP-03: Confirmed as non-issue (linear circuit growth, not exponential)

**All comparison bugs resolved:**
- Phase 35-03 fixed XOR alignment bug in widened comparisons
- Proper LSB-aligned zero-extension now produces unsigned semantics
- All MSB-boundary tests passing (232 lt, 232 gt, 265 eq, 265 ne)
- Ready for Phase 36 (xfail marker cleanup)

**Deferred to future milestone:**
- BUG-DIV-01, BUG-MOD-REDUCE, BUG-COND-MUL-01

## Session Continuity

Last session: 2026-02-01
Stopped at: Completed 35-03-PLAN.md (Phase 35 complete)
Resume file: None
Resume action: Proceed to Phase 36 (comparison test cleanup)

---
*State updated: 2026-02-01 after 35-03 execution*
