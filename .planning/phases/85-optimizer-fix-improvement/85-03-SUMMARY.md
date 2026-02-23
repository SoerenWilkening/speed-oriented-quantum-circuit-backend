---
phase: 85-optimizer-fix-improvement
plan: 03
subsystem: compile-replay
tags: [cython, optimizer, profiling, stack-allocation, performance]

requires:
  - phase: 85-01
    provides: Golden-master infrastructure for regression verification
  - phase: 85-02
    provides: Binary search in optimizer
provides:
  - Stack-allocated gate_t in inject_remapped_gates (36% replay speedup)
  - Compile replay profiling infrastructure
  - Hot-path breakdown analysis
affects: []

tech-stack:
  added: []
  patterns: [stack-allocation-for-hot-path, profiling-driven-optimization]

key-files:
  created:
    - tests/python/test_compile_performance.py
  modified:
    - src/quantum_language/_core.pyx
    - docs/optimizer_benchmark_results.md

key-decisions:
  - "Stack allocation is safe because add_gate copies via memcpy (verified in append_gate)"
  - "Python overhead dominates at 93% -- further optimization needs Cython-level _replay"
  - "Did not pursue dict-to-list or gate pattern caching since profiling showed C path is only 7%"

patterns-established:
  - "Profiling pattern: measure inject_remapped_gates vs full _replay to identify bottleneck layer"

requirements-completed: [PERF-03]

duration: 8min
completed: 2026-02-23
---

# Plan 85-03: Compile replay profiling + optimization Summary

**Stack-allocated gate_t eliminates malloc per gate in inject_remapped_gates, providing 36% compile replay speedup**

## Performance

- **Duration:** 8 min
- **Started:** 2026-02-23
- **Completed:** 2026-02-23
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Profiled compile replay hot path: inject_remapped_gates is 7% of time, Python overhead is 93%
- Replaced per-gate malloc/free with stack-allocated gate_t in inject_remapped_gates
- Measured 36% overall replay improvement (before: 342.9us, after: 218.6us for medium workload)
- Per-gate time reduced from 7-8 us to 4-5 us across all workload sizes
- Created compile replay profiling infrastructure for future optimization

## Task Commits

1. **Task 1 + Task 2: Profiling + optimization** - `eefe7cd` (feat)

## Files Created/Modified
- `src/quantum_language/_core.pyx` - Stack-allocated gate_t in inject_remapped_gates
- `tests/python/test_compile_performance.py` - Compile replay profiling tests
- `docs/optimizer_benchmark_results.md` - Before/after comparison and hot-path analysis

## Decisions Made
- Did not pursue Python-level optimizations (dict-to-list, gate pattern caching) since profiling showed inject_remapped_gates is only 7% of replay time
- Further optimization would require moving _replay logic into Cython (larger scope, not in plan)
- Profiling methodology: 50 replays, median timing, breakdown by component

## Deviations from Plan
None - plan executed as written. Profiling data drove optimization choice (stack allocation only, skipping unnecessary Python-level changes).

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 85 complete (all 3 plans done)
- Golden-master infrastructure, binary search, and compile replay optimization all verified
- Ready for Phase 86 (QFT Bug Fixes)

---
*Phase: 85-optimizer-fix-improvement*
*Completed: 2026-02-23*
