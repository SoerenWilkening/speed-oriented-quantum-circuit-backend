---
phase: 87-scope-segfault-fixes
plan: 01
subsystem: c-backend
tags: [types.h, MAXLAYERINSEQUENCE, segfault, multiplication, QQ_mul]

requires:
  - phase: 86-qft-bug-fixes
    provides: QFT arithmetic bug fixes establishing stable multiplication baseline
provides:
  - MAXLAYERINSEQUENCE=300000 supporting up to 64-bit multiplication
  - Regression test for 32-bit multiplication segfault
affects: [87-03, qarray-multiplication]

tech-stack:
  added: []
  patterns: [generation-only tests for circuits exceeding 17-qubit sim limit]

key-files:
  created:
    - tests/python/test_bug01_32bit_mul.py
  modified:
    - c_backend/include/types.h

key-decisions:
  - "Static constant increase to 300,000 (not dynamic calculation) per user decision"
  - "Generation-only tests for widths > 5 (exceed 17-qubit simulation limit)"

patterns-established:
  - "Generation-only test pattern: verify circuit generates without crash for large widths"

requirements-completed: [BUG-01]

duration: 5min
completed: 2026-02-24
---

# Plan 87-01: 32-bit Multiplication Segfault Fix Summary

**Increased MAXLAYERINSEQUENCE from 10,000 to 300,000 in types.h, fixing 32-bit QQ_mul buffer overflow segfault**

## Performance

- **Duration:** 5 min
- **Tasks:** 1
- **Files modified:** 2

## Accomplishments
- Fixed 32-bit multiplication segfault (MAXLAYERINSEQUENCE 10000 -> 300000)
- Verified 64-bit headroom (~8% margin at 276,766 layers)
- Added 13 regression tests covering widths 1-32 and simulation correctness at width 4

## Task Commits

1. **Task 1: Increase MAXLAYERINSEQUENCE and add regression test** - `dca9d98` (fix)

## Files Created/Modified
- `c_backend/include/types.h` - MAXLAYERINSEQUENCE constant 10000 -> 300000
- `tests/python/test_bug01_32bit_mul.py` - 13 regression tests (generation + simulation)

## Decisions Made
- Static constant increase (300,000) per user decision, not dynamic allocation
- Used `_c = a * b` with noqa for generation-only tests where result isn't directly inspected

## Deviations from Plan
None - plan executed exactly as written.

## Issues Encountered
- ruff lint flagged unused variable `c` in generation-only tests; resolved with `_c` prefix and noqa comment

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- MAXLAYERINSEQUENCE=300000 enables Plan 87-03 (qarray *= scalar) to investigate without layer-limit confusion
- All widths 1-32 confirmed working for QQ_mul circuit generation

---
*Phase: 87-scope-segfault-fixes*
*Completed: 2026-02-24*
