# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-01-30)

**Core value:** Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.
**Current focus:** v1.5 Bug Fixes & Exhaustive Verification -- Phase 29: C Backend Bug Fixes (gap closure round 3)

## Current Position

Phase: 29 of 33 (C Backend Bug Fixes)
Plan: 13 of 14 (gap closure round 3 - REGRESSION DETECTED)
Status: CRITICAL -- Attempted BUG-02/BUG-03 fixes caused regressions, need to revert commit 6328d19
Last activity: 2026-01-31 -- Completed 29-13-PLAN.md (verification reveals regressions)

Progress: [███░░░░░░░] 25%

## Performance Metrics

**Velocity:**
- Total plans completed: 100 (v1.0: 41, v1.1: 13, v1.2: 10, v1.3: 16, v1.4: 6, v1.5: 14)
- Average duration: ~10 min/plan
- Total execution time: ~17.5 hours

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

**Recent (Phase 28-29):**

| Phase | Decision | Rationale |
|-------|----------|-----------|
| 28-01 | Separate tests/conftest.py for verification tests vs tests/python/conftest.py for unit tests | Keep fixture namespaces distinct, avoid conflicts between test categories |
| 28-01 | verify_circuit returns (actual, expected) tuple instead of asserting directly | Gives tests flexibility in assertion style and failure message formatting |
| 28-01 | Exhaustive testing threshold at 4 bits, sampled testing for 5+ bits | Balance coverage (all 256 pairs at 4-bit) vs runtime (sampled edge cases + random at higher widths) |
| 28-01 | Deterministic sampling with random.seed(42) | Reproducible test cases across runs for debugging |
| 28-02 | Generate parametrize data at module level via helper functions | Clean test structure, avoids pytest collection overhead from dynamic generation |
| 28-02 | Use default argument binding in circuit_builder closures | Avoids Python closure variable capture issues in loops |
| 28-02 | Document C backend circuit() reset bug rather than attempting fix | C backend memory management fix is risky and time-consuming, beyond v1.5 scope |
| 29-01 | Defer BUG-01 and BUG-02 fixes, create test files only | Investigation revealed root cause is C backend QFT arithmetic (BUG-04), not Python operator logic |
| 29-01 | tests/bugfix/ directory for bug reproduction tests | Separate targeted bug tests from exhaustive verification tests |
| 29-02 | Use MAXLAYERINSEQUENCE for multiplication num_layer allocation | Conservative but safe - formula-based calculation was insufficient |
| 29-02 | Increase per-layer gate allocation to 10*bits | Progressive testing showed 2*bits, 3*bits, 4*bits all still segfaulted |
| 29-02 | Accept partial fixes for both BUG-03 and BUG-04 | Segfault fixed (primary goal), logic bugs documented for future work |
| 29-03 | Fix bit-ordering with bin[bits-1-bit_idx] reversal | two_complement() returns MSB-first, QFT formula needs LSB-first iteration |
| 29-03 | Fix cache update to use rotations[i] not rotations[bits-i-1] | Must match initial build path indexing for gate value consistency |
| 29-03 | Verify formula against Qiskit reference | Analytical verification when BUG-05 prevents reliable test execution |
| 29-05 | Accept partial fix for BUG-03 multiplication | Investigation identified root cause but fix incomplete - document for future work |
| 29-05 | Disable GCC LTO due to compiler bug | GCC 15 -flto triggers internal error, disabled until toolchain fixed |
| 29-04 | BUG-04 fix scope limited to CQ_add only | QQ_add (qint+qint) uses different implementation, needs separate fix |
| 29-04 | BUG-01 and BUG-02 remain blocked by incomplete BUG-04 | Cannot test/fix until QQ_add bit-ordering corrected |
| 29-06 | Fixed QQ_add control qubit mapping: 2*bits-1-bit instead of bits+bit | Control bits were swapped - outer loop MSB-first but needs LSB-first qubit access |
| 29-06 | CQ_add analytically correct, test failures are BUG-05 interference | Plan 29-03 fix is mathematically sound, BUG-05 cache pollution causes wrong results |
| 29-06 | Accept partial verification due to BUG-05 blocking test suite | Simple tests pass (0+0, 1+0, 1+1), complex tests hit memory explosion from BUG-05 |
| 29-08 | QQ_add basic addition verified working (3+5=8) | Plan 29-06 control fix works for simple addition cases |
| 29-08 | Subtraction and comparison failures transitive from QQ_add errors | Comparison logic is correct, failures trace to subtraction which depends on QQ_add |
| 29-08 | BUG-05 prevents definitive QQ_add diagnosis | Cannot distinguish QQ_add bit-ordering bugs from BUG-05 cache pollution |
| 29-10 | Two-part CQ_add fix: qubit_array reversal + rotation reversal | QFT-no-swaps convention mismatch requires compensating at both Python and C layers |
| 29-10 | Path taken: convention fix (not BUG-05 cache) | Isolation test confirmed 0+1 fails in fresh process, ruling out cache pollution |
| 29-09 | Fix QFT processing order at the source (gate.c) rather than patching adders | Fixing the root cause (QFT convention) eliminates all downstream workarounds |
| 29-09 | Revert 29-10 CQ_add workarounds after QFT fix | With correct QFT, qubit_array reversal and rotation reversal double-compensate |
| 29-09 | Accept comparison failures as BUG-05 pre-existing | Verified identical failures with original code; BUG-05 cache contamination is root cause |
| 29-11 | Fix CQ_mul value formula by removing pow(2,bits-1-bit) factor | Double-counting positional weight caused phase wrapping; CQ_mul now 100% correct |
| 29-11 | Keep QQ_mul unchanged from baseline | Target reversal alone insufficient; CCP decomposition needs deeper algorithm redesign |
| 29-13 | Use PYTHONPATH=src instead of pip install -e . | Editable install fails with absolute path error; build_ext --inplace workaround |
| 29-13 | REVERT needed for BUG-02/BUG-03 fixes (commit 6328d19) | Both fixes caused regressions; __le__ rewrite ineffective, target qubit reversal made multiplication worse |

### Pending Todos

None.

### Blockers/Concerns

**Known C backend bugs (v1.5 targets):**
- **BUG-05 (CRITICAL BLOCKER - SEVERELY ESCALATED):** circuit() does not properly reset state - causes memory explosion. Plan 29-13 changes triggered EXPONENTIAL growth in memory requirements (16GB to 17PB for 4-bit operations), now affecting 9 additional tests that previously worked. MUST be fixed before any further BUG-02/BUG-03 work.
- **BUG-04 (FULLY FIXED):** QFT convention fix (29-09) resolves the root cause. CQ_add: all 7 tests pass. QQ_add: verified stable even after 29-13 changes.
- **BUG-03 (REGRESSION IN 29-13):** Plan 29-13 target qubit reversal (bits-i-1 → i) made multiplication WORSE - lost both trivial cases (0*5, 1*1) that previously passed. CQ_mul was 100% correct (10/10) before 29-13. NEED TO REVERT commit 6328d19.
- **BUG-01 (REGRESSION IN 29-13):** Was 5/5 pass after 29-09 QFT fix. Now 2/5 pass - BUG-05 memory explosion blocks 3 tests (0-1, 5-5, 15-0). REVERT needed to restore baseline.
- **BUG-02 (NO IMPROVEMENT IN 29-13):** __le__ rewrite to ~(self > other) had NO effect - still returns 0 for true comparisons. Plus 3 new BUG-05 memory explosions. The comparison logic itself is broken, not the QQ_add invocation pattern.

**Known pre-existing issues (not v1.5 scope):**
- Nested quantum conditionals require quantum-quantum AND
- Build system: pip install -e . fails with absolute path error
- Some tests/python/ tests cause segfaults during execution (discovered in 28-02, pre-existing)

## Session Continuity

Last session: 2026-01-31
Stopped at: Completed 29-13-PLAN.md -- CRITICAL REGRESSIONS DETECTED
Resume file: None
Resume action: URGENT - Revert commit 6328d19 (plan 29-13 changes) to restore baseline, then investigate BUG-05 root cause before attempting further fixes

**Critical path:**
1. Revert 6328d19 to restore 14/19 test baseline
2. Isolate BUG-05 minimal reproduction case
3. Fix BUG-05 OR work around it before touching BUG-02/BUG-03

---
*State updated: 2026-01-31 after Phase 29 plan 13 completion (REGRESSIONS)*
