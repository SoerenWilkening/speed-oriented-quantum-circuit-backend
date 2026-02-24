# Phase 88: Binary Size Reduction - Research

**Researched:** 2026-02-24
**Domain:** C/Cython build system optimization (compiler flags, linker options, symbol stripping)
**Confidence:** HIGH

## Summary

Phase 88 targets compiled .so extension modules built by setuptools/Cython from C backend sources and .pyx files. The project builds 7 Cython extensions (_core, _gates, openqasm, qarray, qbool, qint, qint_mod), each linking ~70+ C source files including 105 hardcoded sequence files (22MB source). Current Linux .so total is ~65MB with no size optimization applied (compiled at -O3, unstripped).

The optimizations are standard GCC/Clang toolchain flags: `-ffunction-sections` + `-fdata-sections` + `--gc-sections` for dead code elimination, `-s` for symbol stripping, and `-Os` for size-optimized compilation. All changes are confined to `setup.py` where `compiler_args` and `linker_args` are defined, conditioned on Release build type.

**Primary recommendation:** Modify setup.py to detect Release build mode (via environment variable or pip install context), then apply gc-sections, stripping, and -Os in that order. Benchmark each step individually to document the contribution of each optimization.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Prefer size over speed -- accept up to 15% slowdown for maximum size reduction
- Circuit generation is the primary benchmark workload
- Try optimization levels in order: -Os first, fall back to -O2 if regression >15%, then -O3 as last resort
- Apply one optimization level uniformly to all .so modules (no per-module tuning)
- Strip everything -- no separate .debug symbol files
- Strip at link time using the -s linker flag (not post-build strip command)
- Focus on .so files only -- do not optimize Python .pyc bytecode
- No size guard thresholds or CI checks -- measure before/after and document
- Size optimizations apply to Release builds only
- Keep current default build type (Debug or unset) -- developers get debug builds by default
- pip install / setup.py should build with Release mode automatically
- No extra CMake option -- tie optimizations to build type directly
- Use existing test circuits from the test suite (no custom benchmark circuits)
- Document results in a BENCHMARK.md markdown table in the phase directory
- Run each benchmark 3 times, report median
- Measure wall-clock time only (not CPU time)

### Claude's Discretion
- Exact CMake implementation for conditional flags per build type (N/A -- this is setup.py, not CMake)
- How to configure setup.py/pyproject.toml for Release builds
- Which specific linker flags to use beyond --gc-sections
- Benchmark script structure and output format

### Deferred Ideas (OUT OF SCOPE)
- None
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SIZE-01 | Apply section garbage collection compiler flags (-ffunction-sections, -fdata-sections, --gc-sections) | Standard GCC/Clang flags; add to compiler_args and linker_args in setup.py for Release builds |
| SIZE-02 | Strip symbols from release builds (-s link flag or post-build strip) | Add `-s` to linker_args for Release builds; context decision locks to link-time stripping |
| SIZE-03 | Evaluate -Os vs -O3 for sequence files with benchmark verification (no performance regression) | Replace -O3 with -Os in compiler_args for Release; benchmark with test suite circuit generation |
</phase_requirements>

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| setuptools | >=61.0 | Build system | Already in use (pyproject.toml) |
| Cython | >=3.0.11 | .pyx compilation | Already in use |
| GCC/Clang | System | C compiler | Standard toolchain |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| time (Python) | stdlib | Wall-clock benchmarking | Benchmark script |
| os.path.getsize | stdlib | File size measurement | Before/after comparison |

### Alternatives Considered
None -- all tools are standard compiler/linker flags, no new dependencies needed.

## Architecture Patterns

### Current Build Architecture

```
setup.py
  |- compiler_args = ["-O3", "-pthread"]
  |- linker_args = []
  |- For each .pyx in src/quantum_language/:
  |    Extension(sources=[pyx] + c_sources, extra_compile_args=compiler_args, extra_link_args=linker_args)
  |- cythonize(extensions, language_level="3")
```

Each of the 7 extensions links ALL c_sources (~70+ files including 105 sequence files). This means every .so contains the same C object code. Section garbage collection (`--gc-sections`) will remove unreferenced functions per-module, which should significantly reduce each .so since most sequence dispatch functions are only called from specific modules.

### Pattern: Release Build Detection in setup.py

The context decision says "pip install / setup.py should build with Release mode automatically." The standard approach:

```python
import os

# Default to Release when pip-installing (BUILD_TYPE not explicitly set)
build_type = os.environ.get("BUILD_TYPE", "Release")
is_release = build_type.lower() == "release"

if is_release:
    compiler_args = ["-Os", "-ffunction-sections", "-fdata-sections", "-pthread"]
    linker_args = ["-Wl,--gc-sections", "-s"]
else:
    # Debug/development: keep -O3, no stripping
    compiler_args = ["-O3", "-pthread"]
    linker_args = []
```

This means:
- `pip install .` -> Release (optimized for size, stripped)
- `BUILD_TYPE=Debug pip install -e .` -> Debug (no size opts)
- `python setup.py build_ext --inplace` -> Release (default)
- `BUILD_TYPE=Debug python setup.py build_ext --inplace` -> Debug

### Pattern: macOS Linker Compatibility

macOS `ld` does NOT support `--gc-sections`. The equivalent is `-dead_strip`. Also, macOS `ld` does not support `-s` as a linker flag the same way. Instead, use `-Wl,-x` to strip local symbols, or rely on `strip` post-build. However, the context says to use `-s` linker flag. For cross-platform:

```python
import sys

if sys.platform == "darwin":
    gc_link_flags = ["-Wl,-dead_strip"]
    strip_flags = ["-Wl,-x"]  # Strip local symbols
else:
    gc_link_flags = ["-Wl,--gc-sections"]
    strip_flags = ["-s"]
```

**CRITICAL:** `-ffunction-sections` and `-fdata-sections` work on both GCC and Clang (macOS), but the linker flag differs.

### Anti-Patterns to Avoid
- **Applying size opts to Debug builds:** Context explicitly says Release only
- **Using -O0 or -Og:** Not requested; context says -Os -> -O2 -> -O3 fallback chain
- **Per-module optimization levels:** Context says "one optimization level uniformly"
- **Post-build strip command:** Context says link-time stripping via -s flag

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Dead code elimination | Custom function analysis | -ffunction-sections + --gc-sections | Compiler/linker handles this correctly |
| Symbol stripping | Post-build strip script | -s linker flag | Context decision; cleaner build pipeline |
| Benchmark timing | Custom profiling framework | time.perf_counter() in simple script | Only wall-clock time needed |

## Common Pitfalls

### Pitfall 1: macOS Linker Flag Incompatibility
**What goes wrong:** `--gc-sections` is a GNU ld flag. macOS uses Apple's ld which expects `-dead_strip`.
**Why it happens:** Linux-centric build instructions applied to macOS.
**How to avoid:** Platform-conditional linker flags using `sys.platform`.
**Warning signs:** Linker errors like "unknown option: --gc-sections" on macOS.

### Pitfall 2: -Os Breaking Function Inlining
**What goes wrong:** -Os may not inline functions that -O3 does, causing performance regression in hot paths (gate injection, sequence dispatch).
**Why it happens:** -Os prioritizes code size, -O3 prioritizes speed; inlining increases size but improves speed.
**How to avoid:** Benchmark circuit generation time. Context allows up to 15% regression.
**Warning signs:** >15% regression on circuit generation benchmarks.

### Pitfall 3: Stripping Breaks Cython Debugging
**What goes wrong:** Symbol stripping removes function names needed for meaningful stack traces.
**Why it happens:** -s strips all symbols including debug info.
**How to avoid:** Only strip in Release builds. Debug/development builds keep symbols.
**Warning signs:** Segfault stack traces showing `??` instead of function names.

### Pitfall 4: Profiling/Coverage Build Conflicts
**What goes wrong:** The existing QUANTUM_PROFILE and QUANTUM_COVERAGE env vars add flags that conflict with size optimization (e.g., --coverage adds instrumentation that inflates binary size).
**Why it happens:** Coverage instrumentation and size optimization are contradictory goals.
**How to avoid:** Size optimizations should NOT apply when QUANTUM_PROFILE or QUANTUM_COVERAGE is set.
**Warning signs:** Unexpectedly large binaries in coverage mode, or missing coverage data with stripped binaries.

### Pitfall 5: LTO Interaction
**What goes wrong:** The setup.py already has a comment "Removed -flto due to GCC LTO bug." Adding gc-sections with LTO can cause issues.
**Why it happens:** LTO and gc-sections interact -- LTO does its own dead code elimination.
**How to avoid:** Do NOT re-enable LTO. The existing comment warns against it.
**Warning signs:** Build failures or incorrect code generation.

## Code Examples

### setup.py Modification Pattern

```python
import os
import sys

# Build type: Release by default (optimized), Debug via BUILD_TYPE=Debug
build_type = os.environ.get("BUILD_TYPE", "Release")
is_release = build_type.lower() == "release"

# Base flags
compiler_args = ["-pthread"]
linker_args = []

if is_release:
    # SIZE-03: -Os for size optimization (fall back to -O2/-O3 if >15% regression)
    compiler_args.append("-Os")
    # SIZE-01: Section garbage collection
    compiler_args.extend(["-ffunction-sections", "-fdata-sections"])
    if sys.platform == "darwin":
        linker_args.append("-Wl,-dead_strip")
    else:
        linker_args.append("-Wl,--gc-sections")
    # SIZE-02: Strip symbols at link time
    if sys.platform == "darwin":
        linker_args.append("-Wl,-x")
    else:
        linker_args.append("-s")
else:
    # Debug: -O3 for speed, no stripping
    compiler_args.append("-O3")
```

### Benchmark Script Pattern

```python
"""Binary size benchmark: measures .so sizes and circuit generation time."""
import glob
import os
import time

import quantum_language as ql

SO_DIR = "src/quantum_language"

def measure_sizes():
    """Report total .so file sizes."""
    total = 0
    for so in glob.glob(os.path.join(SO_DIR, "*.so")):
        size = os.path.getsize(so)
        print(f"  {os.path.basename(so)}: {size:,} bytes ({size/1024/1024:.1f} MB)")
        total += size
    print(f"  TOTAL: {total:,} bytes ({total/1024/1024:.1f} MB)")
    return total

def benchmark_circuit_gen(runs=3):
    """Benchmark circuit generation using existing test patterns."""
    times = []
    for _ in range(runs):
        ql.reset_circuit()
        start = time.perf_counter()
        # Use representative circuit operations
        a = ql.qint(value=5, bits=8)
        b = ql.qint(value=3, bits=8)
        c = a + b
        c -= 2
        d = a * b
        elapsed = time.perf_counter() - start
        times.append(elapsed)
    times.sort()
    return times[len(times) // 2]  # median

if __name__ == "__main__":
    print("=== Binary Size Report ===")
    total = measure_sizes()
    print(f"\n=== Circuit Generation Benchmark ({3} runs, median) ===")
    median = benchmark_circuit_gen()
    print(f"  Median: {median*1000:.2f} ms")
```

## Open Questions

1. **Exact regression threshold measurement**
   - What we know: 15% max regression on circuit generation
   - What's unclear: What baseline timing to use (current -O3 build)
   - Recommendation: Benchmark current build first, then apply optimizations and compare

2. **macOS vs Linux size reduction differences**
   - What we know: Current Linux .so files are ~65MB total; macOS files are smaller (~35MB total)
   - What's unclear: Whether size reduction percentage will differ across platforms
   - Recommendation: Focus on Linux (primary target for the current environment), document macOS behavior

## Sources

### Primary (HIGH confidence)
- GCC documentation: `-ffunction-sections`, `-fdata-sections`, `--gc-sections` are standard and well-documented
- GCC documentation: `-Os` optimizes for size, equivalent to `-O2` minus size-increasing optimizations
- Apple ld documentation: `-dead_strip` is the macOS equivalent of `--gc-sections`
- Existing setup.py in project (direct codebase analysis)

### Secondary (MEDIUM confidence)
- Typical size reduction from gc-sections + stripping: 20-50% depending on dead code proportion
- Sequence files (105 files, 22MB source) contain generated gate sequences -- high likelihood of per-module dead code

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- compiler flags are well-established, decades-old technology
- Architecture: HIGH -- setup.py structure is clear, changes are isolated
- Pitfalls: HIGH -- known cross-platform issues are well-documented

**Research date:** 2026-02-24
**Valid until:** Indefinite (compiler flags are stable)
