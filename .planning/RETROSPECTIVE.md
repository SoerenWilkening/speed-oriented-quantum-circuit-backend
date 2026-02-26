# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v5.0 — Advanced Arithmetic & Compilation

**Shipped:** 2026-02-26
**Phases:** 7 | **Plans:** 19

### What Was Built
- Quantum counting API (`ql.count_solutions()`) wrapping IQAE for exact solution count estimation
- C-level restoring divmod replacing broken QFT division; Beauregard modular reduction replacing orphan-qubit `_reduce_mod`
- Beauregard modular Toffoli arithmetic (12 C functions: add/sub/mul mod N with CQ/QQ/controlled variants)
- Depth/ancilla tradeoff with runtime CLA/CDKM dispatch via `ql.option('tradeoff', ...)`
- Parametric compilation (`@ql.compile(parametric=True)`) with topology-safe probe/detect/replay
- Full requirements closure: 20/20 satisfied, milestone audit PASSED

### What Worked
- Milestone audit process caught 7 procedural gaps (missing VERIFICATION.md files, unchecked requirements) before completion — gap closure phases 95-96 addressed all of them cleanly
- C-level arithmetic implementation pattern (established in v3.0) scaled well to modular Beauregard sequence — 12 functions implemented efficiently
- Exhaustive verification (statevector widths 2-4, MPS widths 5-8) caught the CDKM register ordering bug early
- Parametric compilation's defensive topology check prevents silent correctness errors

### What Was Inefficient
- First milestone audit found gaps that could have been prevented by writing VERIFICATION.md during execution (not after)
- QQ division ancilla leak is a fundamental algorithmic limitation — investigating the repeated-subtraction approach consumed time that could have been spent accepting the limitation earlier
- Phase 95 (verification closure) and Phase 96 (tech debt cleanup) were reactive; building verification into execution phases would have eliminated them

### Patterns Established
- Beauregard 8-step pattern for modular addition: add a, sub N, copy sign, cond add N, sub a, flip, copy sign, add a
- Set-once enforcement pattern: module-level flag frozen after first arithmetic operation
- _get_mode_flags() centralized cache key construction pattern
- Parametric probe/detect/replay lifecycle for compile-once semantics

### Key Lessons
1. **Run milestone audit early**: Audit before all phases are complete to catch procedural gaps while context is fresh
2. **Write VERIFICATION.md during execution, not after**: Verification should be part of phase execution, not a separate gap-closure phase
3. **Accept algorithmic limitations early**: The QQ division ancilla leak is fundamental — documenting and moving on was the right call
4. **Modular arithmetic forces RCA**: CLA subtraction limitation means all modular ops must bypass the tradeoff system — encode constraints in code, not documentation

### Cost Observations
- Model mix: ~90% opus, ~10% haiku (research agents)
- 80 commits across 3 days
- Notable: Phases 90-94 (5 implementation phases) completed in 2 days; Phases 95-96 (closure) took 1 additional day

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Phases | Plans | Key Change |
|-----------|--------|-------|------------|
| v3.0 | 11 | 35 | Established C-level Toffoli arithmetic pattern |
| v4.0 | 6 | 18 | Oracle infrastructure with validation-at-build-time |
| v4.1 | 8 | 21 | Quality-focused: security, coverage, size reduction |
| v5.0 | 7 | 19 | Milestone audit process; gap closure phases |

### Cumulative Quality

| Milestone | Requirements | Audit Score | Tech Debt Items |
|-----------|-------------|-------------|-----------------|
| v4.0 | 16 satisfied | N/A | 3 deferred bugs |
| v4.1 | N/A (quality) | N/A | 3 deferred bugs |
| v5.0 | 20/20 | PASSED (20/20 + 7/7 + 20/20 + 8/8) | 12 non-blocking |

### Top Lessons (Verified Across Milestones)

1. **C-level implementation scales well**: Pattern from v3.0 CDKM → v5.0 Beauregard confirms that implementing at C level first, then wiring through Cython, is the right approach
2. **Exhaustive verification catches real bugs**: CDKM register ordering (v5.0), QFT qubit mapping (v3.0), comparison MSB (v4.0) — all caught by systematic testing
3. **Profile before optimizing**: v2.2 principle continues to pay off — data-driven tradeoff threshold (width 4) in v5.0
