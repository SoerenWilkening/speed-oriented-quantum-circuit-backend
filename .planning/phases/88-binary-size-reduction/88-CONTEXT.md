# Phase 88: Binary Size Reduction - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Reduce compiled .so extension module size through compiler flags, linker options, and symbol stripping. Evaluate optimization levels (-Os vs -O3) with benchmarks. All changes must preserve functionality (full test suite passes) and keep performance regression within 15% on circuit generation workloads.

</domain>

<decisions>
## Implementation Decisions

### Size vs performance tradeoff
- Prefer size over speed — accept up to 15% slowdown for maximum size reduction
- Circuit generation is the primary benchmark workload
- Try optimization levels in order: -Os first, fall back to -O2 if regression >15%, then -O3 as last resort
- Apply one optimization level uniformly to all .so modules (no per-module tuning)

### Debug symbol handling
- Strip everything — no separate .debug symbol files
- Strip at link time using the -s linker flag (not post-build strip command)
- Focus on .so files only — do not optimize Python .pyc bytecode
- No size guard thresholds or CI checks — measure before/after and document

### Build mode scope
- Size optimizations (gc-sections, stripping, -Os/-O2) apply to Release builds only
- Keep current default build type (Debug or unset) — developers get debug builds by default
- pip install / setup.py should build with Release mode automatically
- No extra CMake option (e.g., ENABLE_SIZE_OPT) — tie optimizations to build type directly

### Benchmark methodology
- Use existing test circuits from the test suite (no custom benchmark circuits)
- Document results in a BENCHMARK.md markdown table in the phase directory
- Run each benchmark 3 times, report median
- Measure wall-clock time only (not CPU time)

### Claude's Discretion
- Exact CMake implementation for conditional flags per build type
- How to configure setup.py/pyproject.toml for Release builds
- Which specific linker flags to use beyond --gc-sections
- Benchmark script structure and output format

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. The success criteria from the roadmap provide clear targets (20% size reduction, 15% max regression, full test suite passing).

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 88-binary-size-reduction*
*Context gathered: 2026-02-24*
