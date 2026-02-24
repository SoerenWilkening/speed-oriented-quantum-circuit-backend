---
phase: 88-binary-size-reduction
plan: 01
subsystem: build-system
tags: [size-optimization, compiler-flags, linker-flags]
requires: []
provides: [release-build-flags, gc-sections, symbol-stripping]
affects: [setup.py, compiled-so-files]
tech-stack:
  added: []
  patterns: [conditional-build-type, platform-specific-linker-flags]
key-files:
  created:
    - .planning/phases/88-binary-size-reduction/88-BASELINE.md
  modified:
    - setup.py
key-decisions:
  - BUILD_TYPE env var defaults to Release; profiling/coverage override to Debug
  - Platform-conditional linker flags (--gc-sections for Linux, -dead_strip for macOS)
requirements-completed: [SIZE-01, SIZE-02]
duration: 15 min
completed: 2026-02-24
---

# Phase 88 Plan 01: Add Section GC and Symbol Stripping Summary

Conditional Release/Debug build flags with section garbage collection and symbol stripping, reducing Linux x86_64 .so total from 64.4 MB to 27.9 MB (56.6% reduction).

## Duration
- Start: 2026-02-24T17:02:28Z
- End: 2026-02-24T17:18:22Z
- Duration: ~15 min
- Tasks: 2
- Files: 2

## Task Results

### Task 1: Record Baseline .so Sizes
- Created 88-BASELINE.md with all .so file sizes
- Linux x86_64 baseline: 7 modules, 67,555,624 bytes (64.4 MB)
- Commit: 4accf3d

### Task 2: Add Conditional Release Build Flags
- Modified setup.py with BUILD_TYPE environment variable (default: Release)
- Release mode: -ffunction-sections, -fdata-sections, -Wl,--gc-sections, -s
- Debug mode: -O3, -pthread (unchanged from original)
- Platform-conditional: macOS uses -dead_strip and -Wl,-x
- Profiling/coverage modes override to Debug flags
- Post-optimization sizes (Linux x86_64): 29,287,024 bytes (27.9 MB)
- **Size reduction: 56.6% from baseline**
- Commit: c2fc245

## Deviations from Plan

None - plan executed exactly as written.

## Size Results

| Module | Baseline (MB) | After (MB) | Reduction |
|--------|--------------|------------|-----------|
| _core | 9.0 | 4.0 | 55.7% |
| _gates | 8.6 | 3.9 | 54.7% |
| openqasm | 8.2 | 3.8 | 53.1% |
| qarray | 10.2 | 4.1 | 59.2% |
| qbool | 8.4 | 3.9 | 54.0% |
| qint | 11.5 | 4.3 | 62.4% |
| qint_mod | 8.6 | 3.9 | 54.8% |
| **TOTAL** | **64.4** | **27.9** | **56.6%** |

## Test Status

- Import test: PASSED
- API tests (48): PASSED
- Circuit generation tests (15): PASSED
- Optimizer benchmark tests (3): PASSED
- Pre-existing failures: test_qint_default_width (assert 3 == 8), test_tic_tac_toe_pattern (TypeError), qarray segfault (Phase 87 known issue)

## Next

Ready for 88-02 (benchmark -Os vs -O3).
