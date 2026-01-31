# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-01-30)

**Core value:** Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.
**Current focus:** v1.5 Bug Fixes & Exhaustive Verification -- Phase 31 in progress

## Current Position

Phase: 31 of 33 (Comparison Verification)
Plan: 2 of 2
Status: Plan 31-02 complete
Last activity: 2026-01-31 -- Completed 31-02-PLAN.md (operand preservation verification)

Progress: [█████░░░░░] 36%

## Performance Metrics

**Velocity:**
- Total plans completed: 109 (v1.0: 41, v1.1: 13, v1.2: 10, v1.3: 16, v1.4: 6, v1.5: 23)
- Average duration: ~10 min/plan
- Total execution time: ~18.6 hours

**By Milestone:**

| Milestone | Phases | Plans | Status |
|-----------|--------|-------|--------|
| v1.0 MVP | 1-10 | 41 | Complete (2026-01-27) |
| v1.1 QPU State | 11-15 | 13 | Complete (2026-01-28) |
| v1.2 Uncomputation | 16-20 | 10 | Complete (2026-01-28) |
| v1.3 Package & Array | 21-24 | 16 | Complete (2026-01-29) |
| v1.4 OpenQASM Export | 25-27 | 6 | Complete (2026-01-30) |
| v1.5 Bug Fixes & Verification | 28-33 | TBD | In progress |

## Accumulated Context

### Decisions

Milestone decisions archived. See PROJECT.md Key Decisions table for full history.

**Recent (Phase 31):**

| Phase | Decision | Rationale |
|-------|----------|-----------|
| 31-02 | Module-level calibration with empirical position detection | gt uses widened temporaries; empirical detection handles all variants uniformly |
| 31-02 | No xfails needed for preservation tests | All 6 operators preserve operands correctly in both QQ and CQ variants |

### Blockers/Concerns

**Known C backend bugs (v1.5 targets):**
- **BUG-05 (FULLY FIXED):** circuit() now properly frees old circuit and resets all Python globals. Fixed in plan 29-15.
- **BUG-04 (FULLY FIXED):** All 7 QFT addition tests pass deterministically.
- **BUG-03 (FULLY FIXED):** QQ_mul rewrote with correct CCP decomposition and qubit mapping. 5/5 multiplication tests pass. Fixed in plan 29-17.
- **BUG-01 (FULLY FIXED):** All 5 subtraction tests pass.
- **BUG-02 (FULLY FIXED):** All 6 comparison tests pass. Three root causes fixed: MSB index, GC gate reversal, unsigned overflow. Fixed in plan 29-16.

**New bugs discovered (Phase 30):**
- **BUG-06 (_reduce_mod result corruption):** When modular result is 0, output is N-2; larger moduli have widespread failures. Affects all qint_mod operations.
- **BUG-07 (subtraction extraction instability):** qint_mod subtraction circuit layout varies by input, making position-based result extraction unreliable.
- **BUG-DIV-01 (comparison overflow in restoring division):** When divisor<<bit_pos >= 2^width, comparison produces wrong results. Affects widths >= 2.
- **BUG-DIV-02 (MSB comparison leak in division):** For values >= 2^(w-1), comparison ancillae from MSB iteration leak into LSB iteration. Affects a//1 for large a.

## Session Continuity

Last session: 2026-01-31
Stopped at: Completed 31-02-PLAN.md (operand preservation verification)
Resume file: None
Resume action: Continue Phase 31 or proceed to Phase 32

---
*State updated: 2026-01-31 after 31-02 completion (2568 tests: 2391 pass, 177 xfail)*
