# Architecture: Profiler Infrastructure and Performance Optimization

**Domain:** Quantum circuit compilation framework performance optimization
**Researched:** 2026-02-05
**Confidence:** HIGH (based on existing codebase analysis and established profiling patterns)

## Executive Summary

This document defines the architecture for integrating profiling infrastructure and performance optimization into the existing three-layer Quantum Assembly framework (C backend -> Cython bindings -> Python frontend). The architecture enables:

1. Cross-layer profiling that works from Python through Cython to C
2. Migration path for hot paths from Cython to C
3. Hardcoded gate sequences for common operations (addition 1-16 bits)
4. ~400 LOC per file constraint compliance

## Existing Architecture Analysis

### Current Three-Layer Structure

```
Layer 3: Python Frontend
  - compile.py (1393 LOC): @ql.compile decorator, gate capture/replay/optimization
  - qint.pyx (700 LOC + includes): Quantum integer class, operator overloading
  - qarray.pyx: Array operations
  - _core.pyx (875 LOC): Circuit management, global state

Layer 2: Cython Bindings
  - _core.pxd: C function declarations
  - Type wrappers (circuit_t, sequence_t, gate_t)
  - run_instruction() entry point to C backend

Layer 1: C Backend
  - IntegerAddition.c (526 LOC): QQ_add, CQ_add, cQQ_add, cCQ_add
  - IntegerMultiplication.c: Multiplication circuits
  - gate.c: Gate creation helpers
  - optimizer.c: Gate placement layer optimization
  - circuit_output.c: QASM export
```

### Current Hot Paths (Identified for Migration)

Based on codebase analysis, the following Cython operations are hot paths:

| Operation | Current Location | Call Pattern | Gate Count (8-bit) |
|-----------|------------------|--------------|-------------------|
| `qint.__add__` | qint_arithmetic.pxi:67 | allocate + XOR + add_inplace | 39 layers |
| `qint.addition_inplace` | qint_arithmetic.pxi:5 | QQ_add/CQ_add via run_instruction | ~38 gates |
| `qint.__mul__` | qint_arithmetic.pxi:558 | allocate + multiplication_inplace | O(n^2) layers |
| `run_instruction` | execution.c | Gate remapping + add_gate loop | per-gate overhead |

### Data Flow for Current Operations

```
Python: a + b
  |
  v
Cython: qint.__add__(self, other)
  |
  +-- qint() allocation (allocator_alloc)
  +-- a ^= self (XOR copy)
  +-- a += other (addition_inplace)
        |
        v
Cython: addition_inplace(self, other)
  |
  +-- Build qubit_array mapping
  +-- QQ_add(bits) -> sequence_t*
  +-- run_instruction(seq, qubit_array, invert, circuit)
        |
        v
C: run_instruction(sequence_t*, qubit_array[], invert, circuit_t*)
  |
  +-- For each layer in sequence:
  |     +-- For each gate in layer:
  |           +-- Remap qubits from sequence to qubit_array
  |           +-- add_gate(circuit, remapped_gate)
  |
  v
C: add_gate(circuit_t*, gate_t*)
  |
  +-- Find optimal layer (layer >= layer_floor, qubit not occupied)
  +-- Insert gate into circuit->sequence[layer]
```

## Proposed Profiler Architecture

### Design Principles

1. **Zero-overhead when disabled**: Profiling adds no cost in release builds
2. **Cross-layer visibility**: See call chains from Python through C
3. **Minimal code changes**: Use compile-time macros, not code rewrite
4. **Actionable output**: Report hot functions, not just call counts

### Profiler Module Structure

```
c_backend/
  include/
    profiler.h              # NEW: Profiler API header (~100 LOC)
  src/
    profiler.c              # NEW: Profiler implementation (~300 LOC)

src/quantum_language/
    profiler.py             # NEW: Python profiler API (~200 LOC)
    _profiler.pyx           # NEW: Cython bridge (~150 LOC)
    _profiler.pxd           # NEW: Cython declarations (~50 LOC)
```

### C Profiler API (profiler.h)

```c
#ifndef QUANTUM_PROFILER_H
#define QUANTUM_PROFILER_H

#include <stdint.h>

// Compile-time profiling toggle
#ifdef QUANTUM_PROFILE
  #define PROF_ENTER(name) profiler_enter(name)
  #define PROF_EXIT(name)  profiler_exit(name)
  #define PROF_COUNT(name, n) profiler_count(name, n)
#else
  #define PROF_ENTER(name) ((void)0)
  #define PROF_EXIT(name)  ((void)0)
  #define PROF_COUNT(name, n) ((void)0)
#endif

// Profiler lifecycle
void profiler_init(void);
void profiler_reset(void);
void profiler_destroy(void);

// Instrumentation hooks (only called when QUANTUM_PROFILE defined)
void profiler_enter(const char* func_name);
void profiler_exit(const char* func_name);
void profiler_count(const char* counter_name, uint64_t amount);

// Statistics retrieval
typedef struct {
    const char* name;
    uint64_t call_count;
    uint64_t total_ns;
    uint64_t min_ns;
    uint64_t max_ns;
} profiler_entry_t;

typedef struct {
    profiler_entry_t* entries;
    size_t count;
} profiler_stats_t;

profiler_stats_t profiler_get_stats(void);
void profiler_free_stats(profiler_stats_t* stats);

#endif // QUANTUM_PROFILER_H
```

### Integration Points

**C Backend Instrumentation:**

```c
// In IntegerAddition.c
sequence_t *QQ_add(int bits) {
    PROF_ENTER("QQ_add");

    // ... existing code ...

    PROF_EXIT("QQ_add");
    return add;
}

// In execution.c
void run_instruction(sequence_t *seq, const unsigned int qubit_array[],
                     int invert, circuit_t *circ) {
    PROF_ENTER("run_instruction");
    PROF_COUNT("gates_processed", total_gates);

    // ... existing code ...

    PROF_EXIT("run_instruction");
}
```

**Cython Bridge (_profiler.pyx):**

```python
# cython: profile=True

cdef extern from "profiler.h":
    void profiler_init()
    void profiler_reset()
    # ... other declarations

def enable_profiling():
    """Enable C-level profiling."""
    profiler_init()

def get_profile_stats():
    """Retrieve profiling statistics from C backend."""
    cdef profiler_stats_t stats = profiler_get_stats()
    # Convert to Python dict
    ...
```

**Python API (profiler.py):**

```python
from contextlib import contextmanager
import cProfile
from ._profiler import enable_profiling, get_profile_stats

@contextmanager
def profile():
    """Profile quantum operations across all layers.

    Usage:
        with ql.profile() as stats:
            a = ql.qint(5, width=8)
            b = a + 3
        print(stats.report())
    """
    enable_profiling()
    py_profiler = cProfile.Profile()
    py_profiler.enable()
    try:
        yield ProfileStats()
    finally:
        py_profiler.disable()
        # Merge Python and C stats
```

### Profiler Data Flow

```
Python: with ql.profile() as stats:
  |
  v
profiler.py: ProfileStats context manager
  |
  +-- enable_profiling() -> _profiler.pyx -> profiler_init()
  +-- cProfile.Profile().enable()  # Python layer
  |
  v
[User code runs with instrumentation active]
  |
  v
profiler.py: ProfileStats.report()
  |
  +-- get_profile_stats() -> _profiler.pyx -> profiler_get_stats()
  +-- cProfile.Profile().getstats()  # Python layer
  +-- Merge and format results
```

## Hot Path Migration Architecture

### Migration Strategy

Migrate hot paths from Cython to C in stages:

1. **Phase 1**: Identify hot paths via profiler (no code change)
2. **Phase 2**: Create C equivalents with same interface
3. **Phase 3**: Update Cython to call C directly (bypass run_instruction)
4. **Phase 4**: Validate correctness via existing tests

### New C Module: hot_paths.h/c

```
c_backend/
  include/
    hot_paths.h             # NEW: Optimized operation API (~80 LOC)
  src/
    hot_paths.c             # NEW: Optimized implementations (~400 LOC)
```

**hot_paths.h:**

```c
#ifndef QUANTUM_HOT_PATHS_H
#define QUANTUM_HOT_PATHS_H

#include "circuit.h"
#include "types.h"

/**
 * Optimized quantum-quantum addition directly into circuit.
 *
 * Bypasses sequence_t/run_instruction overhead by generating gates
 * directly into circuit with pre-mapped qubits.
 *
 * @param circ Target circuit
 * @param target_qubits Array of target qubit indices [0:bits-1]
 * @param source_qubits Array of source qubit indices [0:bits-1]
 * @param bits Operation width (1-64)
 * @param invert If true, generate inverse (subtraction)
 * @return 0 on success, -1 on error
 */
int hot_QQ_add(circuit_t* circ,
               const qubit_t* target_qubits,
               const qubit_t* source_qubits,
               int bits,
               int invert);

/**
 * Optimized classical-quantum addition directly into circuit.
 */
int hot_CQ_add(circuit_t* circ,
               const qubit_t* target_qubits,
               int64_t value,
               int bits,
               int invert);

/**
 * Controlled variants.
 */
int hot_cQQ_add(circuit_t* circ,
                const qubit_t* target_qubits,
                const qubit_t* source_qubits,
                qubit_t control,
                int bits,
                int invert);

int hot_cCQ_add(circuit_t* circ,
                const qubit_t* target_qubits,
                int64_t value,
                qubit_t control,
                int bits,
                int invert);

#endif // QUANTUM_HOT_PATHS_H
```

### Cython Integration

Update _core.pxd to expose hot_paths:

```python
cdef extern from "hot_paths.h":
    int hot_QQ_add(circuit_t* circ,
                   const qubit_t* target_qubits,
                   const qubit_t* source_qubits,
                   int bits,
                   int invert)
    int hot_CQ_add(circuit_t* circ,
                   const qubit_t* target_qubits,
                   int64_t value,
                   int bits,
                   int invert)
    # ... etc
```

Update qint_arithmetic.pxi to use hot paths:

```python
cdef addition_inplace(self, other, int invert=False):
    cdef circuit_t *_circuit = <circuit_t*><unsigned long long>_get_circuit()

    if type(other) == int:
        # NEW: Direct C path (no sequence_t intermediate)
        if hot_CQ_add(_circuit,
                      &self.qubits[64 - self.bits],
                      <int64_t>other,
                      self.bits,
                      invert) != 0:
            raise RuntimeError("CQ_add failed")
    else:
        # Quantum-quantum addition
        if hot_QQ_add(_circuit,
                      &self.qubits[64 - self.bits],
                      &(<qint>other).qubits[64 - (<qint>other).bits],
                      max(self.bits, (<qint>other).bits),
                      invert) != 0:
            raise RuntimeError("QQ_add failed")

    return self
```

## Hardcoded Gate Sequences Architecture

### Rationale

For common bit widths (1-16 bits), pre-generate the exact gate sequences at compile time. This eliminates:
- Runtime rotation angle computation
- Dynamic memory allocation
- Loop overhead in sequence generation

### Module Structure

```
c_backend/
  include/
    hardcoded_add.h         # NEW: Declarations for hardcoded sequences (~50 LOC)
  src/
    hardcoded_add_1_4.c     # NEW: 1-4 bit addition sequences (~400 LOC)
    hardcoded_add_5_8.c     # NEW: 5-8 bit addition sequences (~400 LOC)
    hardcoded_add_9_12.c    # NEW: 9-12 bit addition sequences (~400 LOC)
    hardcoded_add_13_16.c   # NEW: 13-16 bit addition sequences (~400 LOC)
```

### hardcoded_add.h

```c
#ifndef QUANTUM_HARDCODED_ADD_H
#define QUANTUM_HARDCODED_ADD_H

#include "circuit.h"
#include "types.h"

// Maximum hardcoded width
#define HARDCODED_ADD_MAX_BITS 16

/**
 * Check if hardcoded addition is available for given width.
 */
static inline int has_hardcoded_add(int bits) {
    return bits >= 1 && bits <= HARDCODED_ADD_MAX_BITS;
}

/**
 * Generate hardcoded QQ_add gates directly into circuit.
 *
 * @param circ Target circuit
 * @param target_qubits Target register [0:bits-1]
 * @param source_qubits Source register [0:bits-1]
 * @param bits Width (1-16)
 * @param invert Generate inverse if true
 * @return 0 on success, -1 if bits out of range
 */
int hardcoded_QQ_add(circuit_t* circ,
                     const qubit_t* target_qubits,
                     const qubit_t* source_qubits,
                     int bits,
                     int invert);

/**
 * Hardcoded CQ_add for specific classical values.
 *
 * For value=1 (increment), generates optimized single-increment circuit.
 * For other values, falls back to general CQ_add.
 */
int hardcoded_CQ_add(circuit_t* circ,
                     const qubit_t* target_qubits,
                     int64_t value,
                     int bits,
                     int invert);

#endif // QUANTUM_HARDCODED_ADD_H
```

### Hardcoded Sequence Format

Each hardcoded file contains pre-computed gate sequences as static data:

```c
// In hardcoded_add_1_4.c

#include "hardcoded_add.h"
#include "gate.h"
#include <math.h>

// Pre-computed rotation angles for 4-bit QFT addition
// QFT: H on each qubit, then controlled rotations
// Total layers: 5*bits - 2 = 18 for 4-bit

typedef struct {
    Standardgate_t type;
    int target;           // Relative qubit index (0 = LSB)
    int control;          // -1 for no control
    double angle;         // For rotation gates
} hardcoded_gate_t;

// 4-bit QQ_add: 18 layers, ~30 gates
static const hardcoded_gate_t QQ_add_4bit[] = {
    // QFT on target register
    {H, 3, -1, 0.0},      // Layer 0: H on MSB
    {P, 3, 2, M_PI/2},    // Layer 1: CP(pi/2) ctrl=2 tgt=3
    {H, 2, -1, 0.0},      // Layer 1: H on bit 2
    {P, 3, 1, M_PI/4},    // Layer 2: CP(pi/4) ctrl=1 tgt=3
    {P, 2, 1, M_PI/2},    // Layer 2: CP(pi/2) ctrl=1 tgt=2
    {H, 1, -1, 0.0},      // Layer 2: H on bit 1
    // ... (continued for all gates)
    {-1, -1, -1, 0.0}     // Terminator
};
#define QQ_ADD_4BIT_COUNT 30

int hardcoded_QQ_add_4bit(circuit_t* circ,
                          const qubit_t* target,
                          const qubit_t* source) {
    // Emit pre-computed gates with qubit remapping
    for (int i = 0; i < QQ_ADD_4BIT_COUNT; i++) {
        const hardcoded_gate_t* hg = &QQ_add_4bit[i];
        gate_t g = {0};
        g.Gate = hg->type;
        g.Target = target[hg->target];
        if (hg->control >= 0) {
            g.Control[0] = (hg->control < 4) ? target[hg->control] : source[hg->control - 4];
            g.NumControls = 1;
        }
        g.GateValue = hg->angle;
        add_gate(circ, &g);
    }
    return 0;
}
```

### Integration with hot_paths.c

```c
// In hot_paths.c

#include "hot_paths.h"
#include "hardcoded_add.h"

int hot_QQ_add(circuit_t* circ,
               const qubit_t* target_qubits,
               const qubit_t* source_qubits,
               int bits,
               int invert) {
    // Try hardcoded path first (1-16 bits)
    if (has_hardcoded_add(bits)) {
        if (invert) {
            // For inverse, negate all rotation angles
            return hardcoded_QQ_add_inverse(circ, target_qubits, source_qubits, bits);
        }
        return hardcoded_QQ_add(circ, target_qubits, source_qubits, bits, invert);
    }

    // Fall back to dynamic sequence generation for > 16 bits
    sequence_t* seq = QQ_add(bits);
    if (seq == NULL) return -1;

    // Build qubit mapping and call run_instruction
    // ... (existing logic)

    return 0;
}
```

## Component Boundaries and Dependencies

### New Module Dependencies

```
                    +-------------------+
                    |   profiler.py     |  (Python API)
                    +-------------------+
                            |
                            v
                    +-------------------+
                    |  _profiler.pyx    |  (Cython bridge)
                    +-------------------+
                            |
                            v
+-------------------+  +-------------------+
|   profiler.h/c    |  |   hot_paths.h/c   |
+-------------------+  +-------------------+
                            |
                            v
                    +-------------------+
                    | hardcoded_add.h   |
                    | hardcoded_add_*.c |
                    +-------------------+
                            |
                            v
                    +-------------------+
                    |   circuit.h       |  (Existing)
                    |   gate.h          |
                    |   types.h         |
                    +-------------------+
```

### File Size Compliance (~400 LOC limit)

| File | Estimated LOC | Notes |
|------|---------------|-------|
| profiler.h | ~100 | API declarations |
| profiler.c | ~300 | Implementation with hash table |
| profiler.py | ~200 | Python wrapper |
| _profiler.pyx | ~150 | Cython bridge |
| hot_paths.h | ~80 | Function declarations |
| hot_paths.c | ~400 | Dispatch logic, fallback paths |
| hardcoded_add.h | ~50 | Declarations |
| hardcoded_add_1_4.c | ~400 | 1-4 bit sequences |
| hardcoded_add_5_8.c | ~400 | 5-8 bit sequences |
| hardcoded_add_9_12.c | ~400 | 9-12 bit sequences |
| hardcoded_add_13_16.c | ~400 | 13-16 bit sequences |

## Build Integration

### setup.py Changes

```python
# Add new C sources
c_sources = [
    # ... existing sources ...
    os.path.join(PROJECT_ROOT, "c_backend", "src", "profiler.c"),
    os.path.join(PROJECT_ROOT, "c_backend", "src", "hot_paths.c"),
    os.path.join(PROJECT_ROOT, "c_backend", "src", "hardcoded_add_1_4.c"),
    os.path.join(PROJECT_ROOT, "c_backend", "src", "hardcoded_add_5_8.c"),
    os.path.join(PROJECT_ROOT, "c_backend", "src", "hardcoded_add_9_12.c"),
    os.path.join(PROJECT_ROOT, "c_backend", "src", "hardcoded_add_13_16.c"),
]

# Profiling build variant
if os.environ.get('QUANTUM_PROFILE'):
    compiler_args.append('-DQUANTUM_PROFILE')
```

### Makefile Targets

```makefile
# Standard build (no profiling)
build:
    python setup.py build_ext --inplace

# Profiling build
build-profile:
    QUANTUM_PROFILE=1 python setup.py build_ext --inplace

# Run benchmarks with profiling
benchmark-profile: build-profile
    python -m pytest tests/benchmarks/ -v --profile
```

## Suggested Build Order for Phases

Based on dependencies and incremental value delivery:

### Phase 1: Profiler Infrastructure (Foundation)

**Deliverables:**
- profiler.h/c (C instrumentation)
- _profiler.pyx/pxd (Cython bridge)
- profiler.py (Python API)
- Instrument existing IntegerAddition.c, execution.c

**Rationale:** Must measure before optimizing. Provides data to prioritize subsequent phases.

**Complexity:** MEDIUM - New module, cross-layer integration
**Risk:** LOW - Additive only, no existing code changes

### Phase 2: Hot Path C Migration (Quick Wins)

**Deliverables:**
- hot_paths.h/c
- Update _core.pxd
- Update qint_arithmetic.pxi to use hot_* functions

**Rationale:** Eliminate run_instruction overhead for common operations.

**Complexity:** MEDIUM - Requires careful qubit mapping
**Risk:** MEDIUM - Must maintain correctness (tests validate)

### Phase 3: Hardcoded Sequences 1-8 bits (High Impact)

**Deliverables:**
- hardcoded_add.h
- hardcoded_add_1_4.c
- hardcoded_add_5_8.c
- Update hot_paths.c to use hardcoded paths

**Rationale:** 8-bit is default width; covers most common use cases.

**Complexity:** MEDIUM - Repetitive but mechanical
**Risk:** LOW - Can validate against existing sequence_t output

### Phase 4: Hardcoded Sequences 9-16 bits (Extended Coverage)

**Deliverables:**
- hardcoded_add_9_12.c
- hardcoded_add_13_16.c

**Rationale:** Extends optimization to larger integers while staying in ~400 LOC per file.

**Complexity:** LOW - Same pattern as Phase 3
**Risk:** LOW - Mechanical extension

### Phase 5: Validation and Benchmarking

**Deliverables:**
- Benchmark suite comparing before/after
- Profile reports demonstrating improvement
- Documentation updates

**Rationale:** Confirm optimization achieved goals.

**Complexity:** LOW
**Risk:** LOW

## Performance Expectations

Based on analysis of current code and profiling research:

| Metric | Current | Expected After Optimization |
|--------|---------|----------------------------|
| 8-bit QQ_add overhead | ~5us per call | ~1us per call |
| run_instruction per-gate overhead | ~100ns | eliminated |
| Memory allocations per operation | 3-5 | 0-1 |
| Profiling overhead (when enabled) | N/A | <5% |

## Anti-Patterns to Avoid

1. **Over-instrumentation**: Don't profile every function. Focus on hot paths.
2. **Allocation in hot paths**: hardcoded sequences must be static const.
3. **Breaking existing API**: hot_paths must be drop-in replacements.
4. **Premature optimization**: Use profiler data to guide decisions.

## Testing Strategy

1. **Correctness tests**: Run existing test suite after each phase
2. **Equivalence tests**: Compare hot_path output to sequence_t output
3. **Benchmark tests**: Measure performance improvement
4. **Profile tests**: Verify profiler accuracy with known workloads

## Sources

- [Cython Profiling Documentation](https://cython.readthedocs.io/en/latest/src/tutorial/profiling_tutorial.html)
- [GCC Instrumentation Options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)
- [Score-P Performance Measurement](https://zenodo.org/records/8424550)
- [Quantum Circuit Optimization Survey](https://arxiv.org/pdf/2408.08941)
- [Gate Decomposition Optimization](https://quantum-journal.org/papers/q-2025-03-12-1659/)
- Existing codebase: IntegerAddition.c, execution.c, qint_arithmetic.pxi
