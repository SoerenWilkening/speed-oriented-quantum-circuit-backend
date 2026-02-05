# Feature Landscape: Performance Optimization for Cython/C Code

**Domain:** Performance optimization for quantum circuit generation framework (Cython/C)
**Researched:** 2026-02-05
**Project Context:** Quantum Assembly v2.2 - Existing @ql.compile decorator with gate capture/replay/optimization, Cython bindings for C backend, circuit optimization passes

---

## Table Stakes

Features users expect from performance optimization work. Missing = optimization effort feels incomplete.

| Feature | Why Expected | Complexity | Integration Notes |
|---------|--------------|------------|-------------------|
| **Profiling infrastructure** | Cannot optimize without measurement; must know where time goes | Low | Python cProfile + Cython annotation HTML + C profiling |
| **Static typing for all cdef functions** | Cython without types gives only ~35% speedup; with types gives 10-100x | Medium | Existing `.pyx` files have partial typing; complete for hot paths |
| **Bounds checking disabled for production** | Standard Cython optimization; `boundscheck=False` in tight loops | Low | Add compiler directives to setup.py or per-function decorators |
| **Memory view usage for array parameters** | Replaces slow Python list access with C-speed buffer protocol | Medium | qubits arrays passed to run_instruction are candidates |
| **Avoid Python object creation in hot paths** | PyObject allocation is slow; use C types directly | Medium | Gate dict creation in compile.py is a bottleneck candidate |

### Table Stakes Rationale

These are standard optimizations that any Cython/C performance work should include. The existing codebase shows partial adoption:
- `_core.pyx` has `cdef` declarations but accessor functions use Python types
- setup.py uses `-O3` but no Cython compiler directives
- C backend uses malloc/calloc but may have unnecessary allocations

---

## Differentiators

Features that would set this optimization apart. High-impact but not universally expected.

| Feature | Value Proposition | Complexity | Integration Notes |
|---------|-------------------|------------|-------------------|
| **Fix f() vs f.inverse() depth discrepancy** | Forward path should match inverse optimization; correctness + performance | Medium | Identified in milestone - inverse produces lower depth than forward |
| **Hardcoded gate sequences for standard operations** | Pre-computed addition for 1-16 bits eliminates runtime QFT generation | High | PoC in milestone; reduces per-call overhead for common widths |
| **Sequence caching at C level (not just Python)** | Current precompiled_*_width arrays exist but still allocate per-value | Medium | Extend existing cache pattern to eliminate value-dependent allocations |
| **nogil sections for parallel gate injection** | Release GIL during C backend calls; enables true parallelism | High | run_instruction and add_gate are candidates if no Python callbacks |
| **Object pooling for gate_t structures** | Reuse gate structs instead of malloc/free per gate | Medium | High allocation rate in add_gate; pool would amortize cost |
| **Memory arena for circuit operations** | Batch allocation for sequence_t and gate arrays | High | Reduces malloc overhead; requires careful lifecycle management |
| **Python-to-C migration for compile.py hot paths** | Gate list optimization currently in Python; C would be faster | High | _optimize_gate_list processes every gate; inverse detection is O(n) |

### Differentiator Analysis

**f() vs f.inverse() depth discrepancy:**
- Current state: `f.inverse(x)` produces lower circuit depth than `f(x)` followed by optimization
- Root cause likely: Forward capture includes intermediate gates that optimizer removes
- Fix approach: Apply same optimization during capture, not just after
- Confidence: HIGH (based on codebase analysis)

**Hardcoded gate sequences:**
- Current state: QFT-based addition generates gates dynamically per call
- Opportunity: For fixed widths 1-16 bits, pre-compute entire gate sequences
- Trade-off: Binary size vs runtime performance
- Confidence: HIGH (C backend already has precompiled_* pattern)

**nogil sections:**
- Current state: All C calls hold GIL
- Opportunity: run_instruction does pure C work after qubit_mapping
- Risk: Must verify no Python callbacks in the call chain
- Confidence: MEDIUM (requires verification)

---

## Anti-Features

Features to explicitly NOT implement. Common mistakes in this domain.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **Premature micro-optimization** | Optimizing without profiling wastes effort on non-bottlenecks | Profile first; optimize measured hot paths only |
| **Global nogil without analysis** | Can cause segfaults if Python objects accessed; hard to debug | Analyze call graph; mark only verified pure-C sections |
| **Removing all bounds checking** | Masks bugs; crashes become silent corruption | Remove only in verified tight loops; keep for debug builds |
| **Custom allocator for all memory** | Complexity overhead; glibc allocator is heavily optimized | Use pools only for proven high-frequency allocations |
| **Inlining everything** | Code bloat; cache pressure; diminishing returns | Let compiler decide; use `inline` hint sparingly |
| **Optimizing the optimizer** | Gate optimization runs once per compile; not the hot path | Focus on capture/replay which runs per call |
| **LTO (Link-Time Optimization)** | setup.py comment notes GCC LTO bug; removed for a reason | Keep -flto disabled unless verified on target platform |

### Anti-Feature Rationale

**Premature micro-optimization:**
The codebase is substantial (~345,901 LOC). Without profiling, guessing bottlenecks is counterproductive. The compile.py decorator is complex; the actual hot path may not be obvious.

**Global nogil:**
Current code uses Python accessor functions (`_get_circuit()`, `_get_controlled()`) throughout. Converting to nogil requires eliminating all Python object access in the call chain.

**LTO disabled:**
From setup.py line 38: `compiler_args = ["-O3", "-pthread"]  # Removed -flto due to GCC LTO bug`
This is a known issue; re-enabling without verification would cause build failures.

---

## Feature Dependencies

```
Profiling Infrastructure
         |
         v
+--------+--------+
|                 |
v                 v
Cython           C Backend
Optimization     Optimization
|                 |
+---> Static types (cdef)
+---> Memory views
+---> Compiler directives
                  |
                  +---> Object pooling
                  +---> Memory arena
                  +---> Hardcoded sequences

                        |
                        v
            f() vs f.inverse() fix
            (requires understanding
             both layers)
```

---

## Complexity Estimates

| Feature | Effort | Risk | Payoff |
|---------|--------|------|--------|
| Profiling infrastructure | 1-2 days | Low | Enables all other work |
| Static typing completion | 2-3 days | Low | 2-10x in typed sections |
| Bounds/wraparound disable | 0.5 days | Low | 10-20% in array loops |
| Memory views | 1-2 days | Medium | 2-5x for array passing |
| f()/f.inverse() fix | 2-3 days | Medium | Correctness + depth match |
| Hardcoded sequences (PoC) | 3-5 days | Medium | 10-50x for cached widths |
| nogil sections | 2-3 days | High | Parallelism potential |
| Object pooling | 3-5 days | Medium | Reduces malloc overhead |
| Python->C migration | 5-10 days | High | Order of magnitude potential |

---

## MVP Recommendation

For v2.2 Performance Optimization, prioritize in this order:

### Phase 1: Measurement (required first)
1. **Profiling infrastructure** - Cannot optimize blind
   - cProfile for Python layer
   - `cython -a` annotation HTML for Cython layer
   - Time measurement in C backend (gprof or manual timing)

### Phase 2: Quick Wins (low risk, measurable impact)
2. **Fix f() vs f.inverse() depth discrepancy** - Identified bug/optimization
3. **Compiler directives** - boundscheck=False, wraparound=False in verified loops
4. **Complete static typing** - Type all hot path cdef functions

### Phase 3: Targeted Optimization (based on profiling results)
5. **Memory views** for qubit array passing (if profiling shows array access overhead)
6. **Hardcoded gate sequences PoC** for addition 1-16 bits (if per-call QFT generation is bottleneck)

### Defer to Post-MVP
- nogil sections (requires careful analysis, high risk)
- Object pooling (requires profiling to confirm malloc is bottleneck)
- Python->C migration (large effort, only if Python optimization insufficient)
- Memory arena (complex lifecycle management)

---

## Existing Optimization Patterns in Codebase

The C backend already uses several optimization patterns that should be extended:

### 1. Precompiled Sequence Caching (IntegerAddition.c)
```c
// Width-parameterized precompiled caches (index 0 unused, 1-64 valid)
sequence_t *precompiled_QQ_add_width[65] = {NULL};
sequence_t *precompiled_cQQ_add_width[65] = {NULL};
```
Pattern: First call computes and caches; subsequent calls return cached pointer.
Extension opportunity: Cache more operations, eliminate per-value recomputation.

### 2. Gate Reuse During Optimization (circuit_optimizer.c)
```c
// Copy circuit - add_gate already handles inverse cancellation
circuit_t *opt = copy_circuit(circ);
```
Pattern: add_gate's built-in inverse cancellation during copy.
Extension opportunity: Apply same optimization during capture, not just post-hoc.

### 3. Python Gate List Optimization (compile.py)
```python
def _optimize_gate_list(gates):
    """Multi-pass adjacent cancellation / merge."""
```
Pattern: Python-level optimization before caching.
Current limitation: Runs every capture; could be moved to C for speed.

---

## Verification Approach

Performance claims require measurement:

| Optimization | Measurement Method | Success Criterion |
|--------------|-------------------|-------------------|
| Static typing | cython -a before/after | Yellow lines reduced in hot paths |
| Bounds checking | pytest --benchmark | 10%+ improvement in array-heavy tests |
| f()/f.inverse() fix | circuit depth comparison | Forward depth matches inverse depth |
| Hardcoded sequences | time.perf_counter around addition | 10x+ improvement for cached widths |
| Memory views | memory_profiler | Reduced allocation count |

---

## Sources

### Cython Optimization
- [scikit-learn Cython Best Practices](https://scikit-learn.org/stable/developers/cython.html) - Comprehensive guide to cdef, nogil, compiler directives
- [Cython Static Typing Documentation](https://cython.readthedocs.io/en/latest/src/quickstart/cythonize.html) - Official guide to faster code via static typing
- [IPython Cookbook - Optimizing Cython](https://ipython-books.github.io/56-optimizing-cython-code-by-writing-less-python-and-more-c/) - Writing less Python, more C
- [Finesse Cython Optimization Guide](https://finesse.ifosim.org/docs/latest/developer/codeguide/cython/optimising.html) - Annotation HTML interpretation
- [Duke Computational Statistics - Cython](https://people.duke.edu/~ccc14/sta-663-2018/notebooks/S13B_Cython.html) - Yellow lines and profiling workflow

### C Memory Optimization
- [calloc vs malloc+memset Performance](https://openillumi.com/en/en-c-memory-calloc-optimization/) - OS-level lazy allocation optimization
- [Memory Allocation in Embedded C](https://medium.com/@juhinsohamdas/mastering-embedded-c-programming-part-2-memory-management-and-optimization-techniques-9ed1db2e0c58) - Memory pools for real-time systems
- [Apple Memory Allocation Tips](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/ManagingMemory/Articles/MemoryAlloc.html) - Buffer reuse patterns

### Python Profiling
- [Python Profilers Documentation](https://docs.python.org/3/library/profile.html) - Official cProfile guide
- [Real Python Profiling Guide](https://realpython.com/python-profiling/) - Finding performance bottlenecks
- [Top Python Profiling Tools](https://daily.dev/blog/top-7-python-profiling-tools-for-performance) - cProfile, line_profiler, memory_profiler comparison

### Quantum Circuit Optimization
- [Template-based Circuit Mapping](https://www.sciencedirect.com/science/article/abs/pii/S0141933122000515) - Pre-computed gate sequences
- [Quantum Circuit Synthesis Survey](https://arxiv.org/html/2407.00736v1) - Overview of optimization techniques
- [Quantum Lookup Tables](https://arxiv.org/abs/2406.18030) - Unified architecture for pre-computation

---

## Confidence Assessment

| Area | Level | Reason |
|------|-------|--------|
| Table stakes optimizations | HIGH | Standard Cython/C patterns with official documentation |
| f()/f.inverse() fix | HIGH | Identified in codebase; root cause understood |
| Hardcoded sequences | HIGH | Existing precompiled pattern to extend |
| nogil sections | MEDIUM | Requires verification of Python-free call paths |
| Object pooling | MEDIUM | malloc overhead not yet profiled |
| Python->C migration | LOW | Effort/benefit ratio needs profiling data |
