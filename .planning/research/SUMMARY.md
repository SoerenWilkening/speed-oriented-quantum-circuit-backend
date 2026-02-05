# Project Research Summary

**Project:** Quantum Assembly v2.2 Performance Optimization
**Domain:** Cython/C Performance Optimization for Quantum Computing Framework
**Researched:** 2026-02-05
**Confidence:** HIGH

## Executive Summary

This research investigates profiling-driven performance optimization for the Quantum Assembly framework, a three-layer (Python/Cython/C) quantum circuit compiler. The project currently generates quantum circuits for arithmetic operations (addition, multiplication, comparison) but suffers from performance overhead in gate generation, particularly the run_instruction dispatch layer and dynamic sequence allocation. The recommended approach establishes profiling infrastructure across all layers (Python cProfile, Cython annotation, C-level timing), identifies hot paths via measurement, then applies targeted optimizations: completing static typing in Cython, migrating critical paths to C, eliminating malloc overhead via pre-allocation or pooling, and pre-computing gate sequences for common operations (1-16 bit addition).

The critical constraint is Python 3.11 for Cython profiling (Python 3.12+ breaks profiling due to PEP-669). The highest-value optimizations come from eliminating boundary crossing overhead (moving entire hot paths to C, not individual functions) and replacing dynamic QFT-based addition with hardcoded gate sequences for common bit widths. Current profiling shows inject_remapped_gates allocates per-gate (malloc in hot path - major bottleneck) and the compile.py optimizer runs Python-level gate list processing on every capture.

Key risks include premature optimization without profiling data (wasting effort on non-bottlenecks), boundary crossing overhead when moving code to C (can be 10x slower if done wrong), and validation failures for precomputed gate sequences (silently incorrect circuits). Mitigation: profile with production-scale workloads first, move entire hot paths (not individual functions) to C, and validate hardcoded sequences against dynamic computation during tests.

## Key Findings

### Recommended Stack

The profiling stack focuses on cross-layer visibility for Python/Cython/C hybrid architecture. Python 3.11 is mandatory for Cython-level profiling due to PEP-669 breaking changes in 3.12+. The stack provides coverage across all three layers with tools that integrate with the existing pytest/setup.py infrastructure.

**Core technologies:**
- **cProfile + snakeviz 2.2.2**: Python function-level timing with browser visualization - zero install overhead, identifies hot Python functions
- **line_profiler 5.0.0**: Line-by-line Python and Cython timing - drills into specific hot functions identified by cProfile
- **memray 1.19.1**: Memory allocation profiling - Bloomberg's production tool, traces Python AND native code allocations (critical for Cython/C)
- **cython -a (annotation HTML)**: Cython-to-C boundary visualization - yellow lines show Python API overhead, guides optimization targets
- **py-spy 0.4.1**: Sampling profiler with --native flag - profiles C extensions without code modification or slowdown
- **perf (Linux) / valgrind**: C-level CPU and memory profiling - deep-dive analysis for C backend hot spots

**Integration approach:**
- Add profiling optional dependency group to pyproject.toml
- Environment variable QUANTUM_PROFILE=1 enables profiling build (Cython directives: profile=True, linetrace=True)
- Makefile targets for profile, profile-line, profile-memory, cython-annotate

**Critical constraint:** Pin profiling work to Python 3.11. Cython profiling is non-functional in CPython 3.12+ due to PEP-669 changes.

### Expected Features

Performance optimization for Cython/C code requires profiling infrastructure first, followed by targeted optimizations based on measurement data. Features categorized by necessity and complexity.

**Must have (table stakes):**
- Profiling infrastructure (Python/Cython/C) - cannot optimize without measurement; establishes baselines
- Static typing for all cdef functions - Cython without types gives only 35% speedup; with types gives 10-100x
- Bounds checking disabled for production - boundscheck=False in tight loops is standard Cython optimization
- Memory views for array parameters - replaces slow Python list access with C-speed buffer protocol
- Avoid Python object creation in hot paths - PyObject allocation is slow; use C types directly

**Should have (competitive differentiators):**
- Fix f() vs f.inverse() depth discrepancy - identified bug where inverse produces lower depth than forward
- Hardcoded gate sequences for 1-16 bit addition - pre-computed sequences eliminate runtime QFT generation (10-50x improvement)
- Sequence caching at C level - extend existing precompiled_*_width pattern to eliminate value-dependent allocations
- nogil sections for parallel gate injection - release GIL during C backend calls if no Python callbacks
- Object pooling for gate_t structures - reuse gate structs instead of malloc/free per gate (inject_remapped_gates allocates per gate currently)

**Defer (post-MVP):**
- Memory arena for circuit operations - complex lifecycle management, requires profiling to confirm malloc is bottleneck
- Python-to-C migration for compile.py - large effort, only if Python optimization insufficient
- Custom allocator for all memory - glibc allocator is heavily optimized; custom allocator adds complexity overhead

**Anti-features (explicitly avoid):**
- Premature micro-optimization - optimize without profiling wastes effort on non-bottlenecks
- Global nogil without analysis - can cause segfaults if Python objects accessed; hard to debug
- Removing all bounds checking - masks bugs; crashes become silent corruption
- LTO (Link-Time Optimization) - setup.py notes GCC LTO bug; removed for a reason

### Architecture Approach

The architecture integrates profiling infrastructure and optimization into the existing three-layer Quantum Assembly framework (C backend -> Cython bindings -> Python frontend) while maintaining the ~400 LOC per file constraint. The approach enables cross-layer profiling from Python through Cython to C, provides migration paths for hot paths from Cython to C, and supports hardcoded gate sequences for common operations.

**Major components:**

1. **Profiler Module** (profiler.h/c, _profiler.pyx, profiler.py ~650 LOC total)
   - C-level instrumentation macros (PROF_ENTER, PROF_EXIT, PROF_COUNT) that compile to no-op when QUANTUM_PROFILE undefined
   - Python context manager API: with ql.profile() as stats
   - Zero overhead when disabled; cross-layer visibility when enabled
   - Integrates with cProfile for Python layer

2. **Hot Paths C API** (hot_paths.h/c ~480 LOC)
   - Direct circuit generation functions: hot_QQ_add, hot_CQ_add, hot_cQQ_add, hot_cCQ_add
   - Bypasses sequence_t/run_instruction overhead by generating gates directly into circuit
   - Pre-mapped qubits eliminate qubit remapping overhead
   - Falls back to dynamic generation for widths > 16 bits

3. **Hardcoded Gate Sequences** (hardcoded_add.h + 4x .c files ~1650 LOC total)
   - Pre-computed addition sequences for 1-16 bits (split into 4 files: 1-4, 5-8, 9-12, 13-16 bits)
   - Static const arrays of gate descriptors (type, target, control, angle)
   - Emission function remaps relative qubit indices to actual circuit qubits
   - Validation: compare hardcoded output to dynamic sequence generation in tests

**Data flow optimization:**
- Current: Python -> Cython qint.__add__ -> Cython addition_inplace -> C QQ_add (allocate sequence_t) -> C run_instruction (remap + inject per gate with malloc)
- Optimized: Python -> Cython qint.__add__ -> C hot_QQ_add (if width <= 16: emit hardcoded sequence; else: fallback) -> add_gate directly (no sequence_t allocation, no per-gate malloc)

**Integration points:**
- C backend: Instrument IntegerAddition.c, execution.c with PROF_ENTER/PROF_EXIT
- Cython: Update _core.pxd to expose hot_paths.h, modify qint_arithmetic.pxi to call hot_* functions
- Python: Add profiler.py wrapper for user-facing API
- Build: setup.py adds new C sources, QUANTUM_PROFILE env var enables profiling directives

### Critical Pitfalls

Based on Cython documentation, scikit-learn best practices, and existing codebase analysis, the top risks are:

1. **Optimizing Without Profiling (Premature Optimization)**
   - Risk: Spend days optimizing code that accounts for 2-5% of runtime while actual bottleneck remains untouched
   - Prevention: Profile first with cProfile/line_profiler on representative workloads; run cython -a to identify yellow lines; establish baselines before any optimization
   - Detection: Compare development effort to actual speedup - if hours of work yield < 5% improvement, wrong target was optimized
   - Phase: Address in Phase 1 (Profiling Infrastructure) - establish profiling before any optimization work

2. **Boundary Crossing Overhead When Moving Code to C**
   - Risk: Moving individual function to C creates 10x performance regression due to Python/C boundary overhead (argument conversion, result marshaling)
   - Prevention: Move entire hot paths, not individual functions; batch operations (pass array of N inputs, not call N times); keep data in C structs; measure call frequency
   - Detection: Profile the call site, not just the function; use perf to see time in marshaling code
   - Phase: Address in Phase 2 (C Migration) - design C API to minimize boundary crossings

3. **Malloc in Hot Paths**
   - Risk: Memory allocation can take 100-1000x longer than actual computation; current inject_remapped_gates allocates per gate
   - Prevention: Pre-allocate buffers once at initialization; use stack allocation for small fixed-size buffers; pool allocation for frequent structures; amortize allocation (allocate in blocks)
   - Detection: Use valgrind --tool=massif to track allocation patterns; count malloc calls with instrumentation
   - Specific target: inject_remapped_gates in _core.pyx lines 756-778 calls malloc(sizeof(gate_t)) per gate
   - Phase: Address in Phase 3 (Memory Optimization) - implement gate pooling or pre-allocation

4. **Unrepresentative Profiling Workloads**
   - Risk: Optimizations based on small test cases (10 gates) produce slower code on real workloads (10,000+ gates)
   - Prevention: Profile with production-size circuits (1000+ gates); vary circuit types (addition, multiplication, comparison, arrays); include edge cases (wide integers, deep control nesting)
   - Detection: Compare optimization gains on test vs production - if they differ by > 20%, workload is not representative
   - Phase: Address in Phase 1 (Profiling Infrastructure) - create representative benchmark suite before optimization

5. **Hardcoding Without Validation**
   - Risk: Precomputed gate sequences are wrong due to computation error, causing silently incorrect quantum circuits
   - Prevention: Generate lookup tables from same code path; validate on load (compare sample precomputed values to runtime computation); comprehensive test coverage for all precomputed paths; include assertions
   - Phase: Address in Phase 3 (Gate Sequence Hardcoding) - include validation infrastructure in tests

## Implications for Roadmap

Based on research, the optimization work must follow a strict profiling-driven methodology. The architecture research identifies clear dependencies: profiling infrastructure is foundation for all optimization decisions, hot path migration requires profiling data to identify targets, and hardcoded sequences build on the C migration infrastructure. Suggested phase structure below.

### Phase 1: Profiling Infrastructure
**Rationale:** Cannot optimize without measurement. Profiling identifies actual bottlenecks and prevents premature optimization (Pitfall 1, 4). Establishes baselines for measuring improvement. All subsequent optimization depends on profiling data.

**Delivers:**
- Cross-layer profiler (profiler.h/c, _profiler.pyx, profiler.py)
- Makefile targets for profiling (profile, profile-line, profile-memory, cython-annotate)
- Benchmark suite with production-scale workloads
- Python 3.11 environment for Cython profiling
- Baseline performance measurements

**Addresses:**
- Table stakes: Profiling infrastructure (required for optimization)
- Avoids Pitfall 1: Optimizing without profiling
- Avoids Pitfall 4: Unrepresentative profiling workloads

**Complexity:** MEDIUM - New module creation, cross-layer integration
**Risk:** LOW - Additive only, no existing code changes
**Research needed:** STANDARD - Profiling patterns well-documented in Cython docs and scikit-learn best practices

### Phase 2: Fix f() vs f.inverse() Depth Discrepancy
**Rationale:** Identified bug where f.inverse(x) produces lower circuit depth than f(x) followed by optimization. Quick win with correctness and performance benefits. Can be fixed once profiling infrastructure identifies the exact cause in optimizer vs capture flow.

**Delivers:**
- Corrected depth for forward operations matching inverse
- Understanding of optimizer behavior during capture vs post-capture
- Validation tests for depth parity

**Addresses:**
- Competitive feature: Fix depth discrepancy
- Correctness issue with performance implications

**Complexity:** MEDIUM - Requires understanding optimizer and capture interaction
**Risk:** MEDIUM - Must maintain correctness (tests validate)
**Research needed:** SKIP - Issue already identified in codebase, standard optimization pattern

### Phase 3: Cython Quick Wins (Static Typing, Compiler Directives)
**Rationale:** Low-risk optimizations guided by cython -a annotation HTML. Complete static typing in hot paths identified by profiling, add boundscheck=False/wraparound=False to verified tight loops, convert to memory views for array passing. Measurable 2-10x gains in typed sections with minimal code change risk.

**Delivers:**
- Complete static typing for hot path cdef functions
- Compiler directives (boundscheck=False) in verified loops
- Memory views for qubit array passing
- cython -a annotation reports showing reduced yellow lines

**Addresses:**
- Table stakes: Static typing, bounds checking disabled, memory views
- Avoids Pitfall 5: Over-typing (guided by annotation HTML)
- Avoids Pitfall 10: Annotation misinterpretation

**Complexity:** LOW - Standard Cython optimization patterns
**Risk:** LOW - Guided by profiling and annotation reports
**Research needed:** SKIP - Well-documented in Cython docs

### Phase 4: C Hot Path Migration
**Rationale:** Profiling data from Phase 1 identifies hot paths in Cython (likely qint.addition_inplace, run_instruction dispatch). Move entire hot paths to C (hot_paths.h/c) to eliminate run_instruction overhead and Python/C boundary crossings. Must move complete operations (not individual functions) to avoid Pitfall 2.

**Delivers:**
- hot_paths.h/c with hot_QQ_add, hot_CQ_add, hot_cQQ_add, hot_cCQ_add
- Updated _core.pxd to expose hot_paths API
- Modified qint_arithmetic.pxi to call hot_* functions directly
- Validation tests comparing hot path output to sequence_t generation

**Addresses:**
- Table stakes: Avoid Python object creation in hot paths
- Competitive feature: Sequence caching at C level
- Avoids Pitfall 2: Boundary crossing overhead (move entire hot paths)
- Avoids Pitfall 3: Malloc in hot paths (design API without per-call allocation)

**Complexity:** HIGH - Careful qubit mapping, complete path migration
**Risk:** MEDIUM - Must maintain correctness (tests validate)
**Research needed:** SKIP - Architecture clearly defined in research

### Phase 5: Hardcoded Gate Sequences (1-8 bits)
**Rationale:** For default 8-bit width (most common use case), pre-computed addition sequences eliminate runtime QFT generation. Expected 10-50x improvement for cached widths. Builds on hot_paths infrastructure from Phase 4. Start with 1-8 bits to validate approach before extending to 16 bits.

**Delivers:**
- hardcoded_add.h with API declarations
- hardcoded_add_1_4.c with 1-4 bit sequences
- hardcoded_add_5_8.c with 5-8 bit sequences
- hot_paths.c updated to use hardcoded sequences when available
- Validation tests: compare hardcoded output to dynamic generation

**Addresses:**
- Competitive feature: Hardcoded gate sequences (high-impact differentiator)
- Avoids Pitfall 8: Hardcoding without validation (include validation infrastructure)

**Complexity:** MEDIUM - Repetitive but mechanical pre-computation
**Risk:** LOW - Can validate against existing sequence_t output
**Research needed:** SKIP - Pattern established in existing precompiled_* caches

### Phase 6: Hardcoded Gate Sequences (9-16 bits)
**Rationale:** Extend hardcoded sequences to 9-16 bits for broader coverage while staying within ~400 LOC per file constraint. Mechanical extension of Phase 5 approach.

**Delivers:**
- hardcoded_add_9_12.c with 9-12 bit sequences
- hardcoded_add_13_16.c with 13-16 bit sequences
- Extended validation tests

**Addresses:**
- Competitive feature: Extended hardcoded sequence coverage

**Complexity:** LOW - Same pattern as Phase 5
**Risk:** LOW - Mechanical extension
**Research needed:** SKIP - Same approach as Phase 5

### Phase 7: Memory Optimization (Object Pooling)
**Rationale:** If profiling data shows malloc overhead remains after Phase 4 (likely in inject_remapped_gates if not yet migrated), implement gate_t object pooling. Only proceed if profiling shows malloc is bottleneck - deferred until measurement confirms need.

**Delivers:**
- Object pool for gate_t structures (if profiling confirms need)
- Modified inject_remapped_gates or equivalent to use pool
- Memory profiling showing reduced allocation count

**Addresses:**
- Competitive feature: Object pooling for gate_t
- Avoids Pitfall 3: Malloc in hot paths

**Complexity:** MEDIUM - Careful lifecycle management
**Risk:** MEDIUM - Pool implementation requires correct reuse logic
**Research needed:** SKIP - Standard object pooling pattern

### Phase 8: Validation and Benchmarking
**Rationale:** Confirm optimization achieved performance goals. Compare before/after benchmarks, generate profile reports demonstrating improvement, update documentation with optimization guidance.

**Delivers:**
- Benchmark comparison report (before vs after optimization)
- Profile reports showing improvement metrics
- Updated documentation for profiling workflow
- Performance regression tests integrated with CI

**Addresses:**
- Avoids Pitfall 9: Regression without baseline tests
- Validation of all optimization work

**Complexity:** LOW - Measurement and documentation
**Risk:** LOW
**Research needed:** SKIP - Standard benchmarking

### Phase Ordering Rationale

- **Phase 1 first (Profiling):** Fundamental dependency - cannot make informed optimization decisions without measurement data. Establishes baselines for measuring improvement. Prevents premature optimization.

- **Phase 2 early (f/f.inverse fix):** Quick win for correctness once profiling identifies root cause. Can be parallelized with Phase 3 if profiling shows independent code paths.

- **Phase 3 before Phase 4 (Cython before C):** Lower risk, faster to implement, provides measurable gains. Ensures Cython layer is optimized before moving to C (avoid moving already-optimizable Cython code).

- **Phase 4 enables Phase 5/6 (hot_paths enables hardcoded):** Hardcoded sequences integrate with hot_paths API. Must have direct circuit generation infrastructure before pre-computed sequences.

- **Phase 5 before Phase 6 (1-8 bits before 9-16):** Validate approach on most common use case (8-bit default) before extending. Mechanical extension reduces risk.

- **Phase 7 deferred (Object pooling):** Only proceed if profiling shows malloc remains bottleneck after Phase 4. May not be needed if hot_paths eliminates per-gate allocation.

- **Phase 8 last (Validation):** Requires all optimization work complete to measure total improvement.

### Research Flags

**Phases with standard patterns (skip research-phase):**
- **Phase 1 (Profiling):** Well-documented in Cython profiling tutorial, scikit-learn best practices, Bloomberg memray docs
- **Phase 2 (f/f.inverse):** Issue identified in codebase, standard optimization flow analysis
- **Phase 3 (Cython Quick Wins):** Official Cython documentation provides clear guidance on static typing, compiler directives, memory views
- **Phase 4 (C Hot Paths):** Architecture clearly defined in ARCHITECTURE-PROFILER-OPTIMIZATION.md, pattern exists in codebase (precompiled_* caches)
- **Phase 5/6 (Hardcoded Sequences):** Extension of existing precompiled pattern, validation approach defined in architecture research
- **Phase 7 (Object Pooling):** Standard object pooling pattern if needed (conditional on profiling)
- **Phase 8 (Validation):** Standard benchmarking and documentation

**No phases require deeper research during planning.** All optimization patterns are well-documented in official Cython documentation, scikit-learn Cython best practices, and existing codebase patterns.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All tools verified via PyPI (versions current as of 2026-02-05); profiling workflow documented in official Cython and Python docs; memray is Bloomberg production tool |
| Features | HIGH | Table stakes based on standard Cython optimization patterns (scikit-learn best practices); differentiators identified from existing codebase analysis (f/f.inverse discrepancy, precompiled_* pattern for hardcoded sequences) |
| Architecture | HIGH | Based on existing three-layer structure analysis; profiler module follows established instrumentation patterns; hot_paths and hardcoded sequences extend existing precompiled_* caching pattern |
| Pitfalls | HIGH | Verified against official Cython documentation, scikit-learn Cython best practices, and Python profiling guides; malloc in hot paths confirmed by _core.pyx line 756-778 analysis |

**Overall confidence:** HIGH

All recommendations grounded in official documentation, established best practices, and existing codebase patterns. Python 3.11 constraint verified against Cython PEP-669 breaking change documentation.

### Gaps to Address

The research provides clear guidance for implementation, but the following should be validated during execution:

- **Profiling workload representativeness:** Create benchmark suite with production-scale circuits (1000+ gates, varied operation types) during Phase 1 to ensure profiling data reflects real usage patterns. Validate benchmark suite against actual production workloads if available.

- **GIL analysis for nogil sections:** During Phase 4 (C Migration), verify that run_instruction and hot path C code has no Python callbacks. Use cython -a to confirm all lines in nogil blocks are white (no Python API calls). This determines whether nogil optimization is viable.

- **Malloc bottleneck confirmation:** Phase 7 (Object Pooling) is conditional on profiling data showing malloc remains bottleneck after Phase 4. May not be needed if hot_paths eliminates per-gate allocation. Decide based on Phase 1 and Phase 4 profiling results.

- **Hardcoded sequence validation approach:** During Phase 5, establish automated validation that compares hardcoded gate sequences to dynamic generation output. This prevents Pitfall 8 (silently incorrect circuits). Use pytest fixtures that generate both paths and assert equality.

- **Cross-platform compiler flag compatibility:** The project notes setup.py removed -flto due to GCC LTO bug. During Phase 8, validate optimized builds work on both Linux and macOS. Document any platform-specific flags or limitations.

## Sources

### Primary (HIGH confidence)

**Stack:**
- [line_profiler 5.0.0 - PyPI](https://pypi.org/project/line-profiler/)
- [memray 1.19.1 - PyPI](https://pypi.org/project/memray/)
- [snakeviz 2.2.2 - PyPI](https://pypi.org/project/snakeviz/)
- [pytest-benchmark 5.2.3 - PyPI](https://pypi.org/project/pytest-benchmark/)
- [py-spy 0.4.1 - PyPI](https://pypi.org/project/py-spy/)
- [scalene 2.1.3 - PyPI](https://pypi.org/project/scalene/)
- [Cython 3.2.4 - PyPI](https://pypi.org/project/Cython/)
- [Python cProfile Documentation](https://docs.python.org/3/library/profile.html)
- [Cython Profiling Tutorial](https://cython.readthedocs.io/en/latest/src/tutorial/profiling_tutorial.html)
- [memray Documentation](https://bloomberg.github.io/memray/)

**Features:**
- [scikit-learn Cython Best Practices](https://scikit-learn.org/stable/developers/cython.html)
- [Cython Static Typing Documentation](https://cython.readthedocs.io/en/latest/src/quickstart/cythonize.html)
- [Finesse Cython Optimization Guide](https://finesse.ifosim.org/docs/latest/developer/codeguide/cython/optimising.html)

**Architecture:**
- [Cython Profiling Tutorial](https://cython.readthedocs.io/en/latest/src/tutorial/profiling_tutorial.html)
- [Score-P Performance Measurement](https://zenodo.org/records/8424550)
- Existing codebase: IntegerAddition.c, execution.c, qint_arithmetic.pxi, _core.pyx

**Pitfalls:**
- [Cython Faster Code via Static Typing](https://cython.readthedocs.io/en/latest/src/quickstart/cythonize.html)
- [Cython Typed Memoryviews](https://cython.readthedocs.io/en/latest/src/userguide/memoryviews.html)
- [scikit-learn Cython Best Practices](https://scikit-learn.org/stable/developers/cython.html)
- [Some Reasons to Avoid Cython](https://pythonspeed.com/articles/cython-limitations/)
- [Hidden Performance Overhead of Python C Extensions](https://pythonspeed.com/articles/python-extension-performance/)

### Secondary (MEDIUM confidence)

**Features:**
- [Real Python - Profiling in Python](https://realpython.com/python-profiling/)
- [Baeldung - Profiling C++ on Linux](https://www.baeldung.com/linux/profiling-c-cpp-code)

**Pitfalls:**
- [Premature Optimization Fallacy](https://the-pi-guy.com/blog/the_premature_optimization_fallacy_why_you_should_measure_performance_before_optimizing/)
- [Ultimate Python Performance Guide 2025](https://www.fyld.pt/blog/python-performance-guide-writing-code-25/)
- [Stack vs Heap Allocation in C](https://www.matecdev.com/posts/c-heap-vs-stack-allocation.html)

### Tertiary (LOW confidence)

**Architecture:**
- [Quantum Circuit Optimization Survey](https://arxiv.org/pdf/2408.08941)
- [Gate Decomposition Optimization](https://quantum-journal.org/papers/q-2025-03-12-1659/)

---
*Research completed: 2026-02-05*
*Ready for roadmap: yes*
