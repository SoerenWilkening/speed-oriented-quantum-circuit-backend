# Phase 84: Security Hardening - Research

**Researched:** 2026-02-23
**Domain:** C/Cython boundary safety, static analysis
**Confidence:** HIGH

## Summary

Phase 84 hardens the boundary between the Python/Cython layer and the C backend. The project has a clear architecture: Cython `.pyx` files call C functions declared in `.pxd` files, passing `circuit_t*` pointers, `sequence_t*` pointers, and populating a shared `qubit_array` numpy buffer (384 elements) before calling `run_instruction()`. The C backend performs no validation on these inputs -- `run_instruction()` only checks `res == NULL` and `reverse_circuit_range()` uses a debug-mode `assert(circ != NULL)` that compiles away in release builds. There are no bounds checks on `qubit_array` anywhere.

The codebase already uses a consistent pattern for error handling: C functions return `NULL` or error codes, and the Cython layer raises Python exceptions. This phase extends that pattern to cover pointer validation at entry points and buffer bounds checking. Static analysis with `cppcheck` and `clang-tidy` will catch remaining unsafe patterns.

**Primary recommendation:** Add a small validation header (`validation.h`) with inline macros that validate circuit pointers and return error codes; have Cython check return codes and raise exceptions. Add a pre-check function for `qubit_array` slot count at the Cython level before populating the buffer.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- C entry-point functions validate pointers and return error codes; Cython translates error codes into Python exceptions
- Only entry-point C functions (called from Cython) perform validation; internal C-to-C calls trust the pointer
- Validate NULL pointers only -- no magic-number or dangling-pointer detection
- Performance constraint: validation must not measurably impact runtime
- Scratch buffer limit is hardcoded at 384 slots (compile-time constant)
- Bounds checks happen at function entry (pre-check total slot requirement), not per individual write
- Only write operations are bounds-checked; reads are not guarded
- Performance is critical: pre-check-at-entry pattern chosen specifically to minimize overhead
- Error messages include diagnostic details (function name, pointer context) -- not just user-friendly text
- Strict template format across all validation points: `[Category] error in [function]: [detail]`
- Buffer overflow messages indicate the max was exceeded (e.g., "slot count exceeded, max 384") without including the requested count
- OverflowError for buffer overflow; ValueError for invalid pointer
- Exception only -- no separate stderr logging from C side
- Run both cppcheck AND clang-tidy on all C backend source files
- Fix findings at ALL severities (HIGH, MEDIUM, LOW) -- not just HIGH
- False positives handled via central suppression file (suppressions.txt) with justification per entry
- Manual execution during this phase; no CI or pre-commit integration yet

### Claude's Discretion
- Error code convention (return codes vs global error state) -- pick what fits existing codebase patterns
- Exact validation macro/function design
- clang-tidy check selection
- Suppression file location and format

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SEC-01 | Add circuit pointer validation at all Cython boundary entry points (prevent unsafe casts in _core.pyx) | Validation header with return codes; Cython wrapper pattern; entry point inventory below |
| SEC-02 | Add qubit_array bounds checking before writes to scratch buffer (prevent silent buffer overrun) | Pre-check-at-entry pattern; `qubit_array` is 384 elements (4*64 + 2*64); slot count validation function |
| SEC-03 | Run cppcheck static analysis on C backend and fix all HIGH severity findings | cppcheck + clang-tidy on c_backend/src/ and c_backend/include/; suppressions.txt for false positives |
</phase_requirements>

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| cppcheck | 2.x+ | C static analysis (bug detection, undefined behavior, memory) | Industry standard for C; zero false-positive focus |
| clang-tidy | 16+ | C linting + modernization (cert, bugprone, clang-analyzer checks) | Deeper analysis than cppcheck; catches different classes of bugs |

### Supporting
| Tool | Purpose | When to Use |
|------|---------|-------------|
| gcc -Wall -Wextra -Werror | Compiler warnings as errors | Already partially in Makefile (`-Wall -Wextra`); extend for validation code |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| cppcheck + clang-tidy | PVS-Studio, Coverity | Proprietary; cppcheck+clang-tidy cover equivalent ground for free |
| Return codes | errno-style global | Return codes fit existing codebase pattern (functions already return NULL on failure) |

### Installation
```bash
# Platform-dependent -- cppcheck and clang-tidy must be installed
# macOS: brew install cppcheck llvm
# Linux: apt install cppcheck clang-tidy
```

## Architecture Patterns

### Cython Boundary Entry Points (Inventory)

These are ALL functions where Cython passes a `circuit_t*` to C code. Each needs validation:

**_core.pyx** (direct C calls with `_circuit` pointer):
| Function | C Function Called | Pointer Used |
|----------|-------------------|--------------|
| `circuit.visualize()` | `circuit_visualize()` | `_circuit` cast |
| `circuit.gate_count` | `circuit_gate_count()` | `_circuit` cast |
| `circuit.depth` | `circuit_depth()` | `_circuit` cast |
| `circuit.qubit_count` | `circuit_qubit_count()` | `_circuit` cast |
| `circuit.gate_counts` | `circuit_gate_counts()` | `_circuit` cast |
| `circuit.draw_data()` | `circuit_to_draw_data()` | `_circuit` cast |
| `circuit.optimize()` | `circuit_optimize()` | `_circuit` cast |
| `circuit.can_optimize()` | `circuit_can_optimize()` | `_circuit` cast |
| `circuit_stats()` | `circuit_get_allocator()` + `allocator_get_stats()` | `_circuit` cast |
| `get_current_layer()` | direct struct access | `_circuit` cast |
| `extract_gate_range()` | direct struct access | `_circuit` cast |
| `reverse_instruction_range()` | `reverse_circuit_range()` | `_circuit` |
| `inject_remapped_gates()` | `add_gate()` | `_circuit` cast |
| `_get_layer_floor()` | direct struct access | `_circuit` cast |
| `_set_layer_floor()` | direct struct access | `_circuit` cast |
| `_allocate_qubit()` | `circuit_get_allocator()` + `allocator_alloc()` | `_circuit` cast |
| `_deallocate_qubits()` | `circuit_get_allocator()` + `allocator_free()` | `_circuit` cast |
| `option()` | direct struct field access | `_get_circuit()` cast |

**qint.pyx / qint_preprocessed.pyx** (uses `run_instruction()` and hot path functions):
| Pattern | C Function | Pointer Used |
|---------|------------|--------------|
| Arithmetic operations | `run_instruction(seq, qubit_array, invert, circ)` | `_circuit` via `_get_circuit()` |
| Hot path add | `hot_path_add_qq()` / `hot_path_add_cq()` | `_circuit` via `_get_circuit()` |
| Hot path mul | `hot_path_mul_qq()` / `hot_path_mul_cq()` | `_circuit` via `_get_circuit()` |
| Hot path xor | `hot_path_ixor_qq()` / `hot_path_ixor_cq()` | `_circuit` via `_get_circuit()` |

### Pattern 1: Validation Header with Return Codes

**What:** A `validation.h` header providing inline validation macros/functions that return error codes.

**Why this pattern:** The codebase already uses NULL returns for error (e.g., `init_circuit()` returns NULL on failure). For void functions like `run_instruction()`, we need an error code return. The user decision locks us to return codes, not global error state.

**Design:**
```c
// validation.h
#ifndef QUANTUM_VALIDATION_H
#define QUANTUM_VALIDATION_H

// Error codes for validated entry points
#define QV_OK          0
#define QV_NULL_CIRC  -1
#define QV_NULL_SEQ   -2
#define QV_NULL_ARG   -3

// Inline validation -- compiles to a single branch
static inline int qv_check_circuit(const circuit_t *circ, const char *func_name) {
    if (circ == NULL) {
        // Store error context for Cython to read
        // (or just return error code -- Cython knows the function name)
        return QV_NULL_CIRC;
    }
    return QV_OK;
}

#endif
```

**Cython side:**
```python
cdef int rc = validated_run_instruction(seq, qubit_array_ptr, invert, circ)
if rc == QV_NULL_CIRC:
    raise ValueError("[Validation] error in run_instruction: circuit pointer is NULL")
elif rc == QV_NULL_SEQ:
    pass  # NULL sequence is already handled (no-op)
```

### Pattern 2: Qubit Array Bounds Pre-Check (Cython-Level)

**What:** A validation function called at the start of each operation that populates `qubit_array`, checking that the total slot count will not exceed 384.

**Why Cython-level:** The slot count is known before any writes. Checking in Cython avoids any C-level overhead. The check is a single integer comparison.

**Design:**
```python
cdef inline void _check_qubit_slots(int required, str func_name) except *:
    if required > 384:
        raise OverflowError(
            f"[Buffer] error in {func_name}: slot count exceeded, max 384"
        )
```

This is called before each block of `qubit_array[...] = ...` assignments.

### Pattern 3: Wrapper Functions for Validated Entry Points

For C functions that currently return `void` (like `run_instruction`, `reverse_circuit_range`), create validated wrapper functions that return `int` error codes:

```c
// In execution.h -- new validated versions
int validated_run_instruction(sequence_t *res, const qubit_t qubit_array[],
                              int invert, circuit_t *circ);
int validated_reverse_circuit_range(circuit_t *circ, int start_layer, int end_layer);
```

The original functions remain unchanged for internal C-to-C calls (per user decision).

### Anti-Patterns to Avoid
- **Validating inside inner loops:** The user explicitly chose pre-check-at-entry to avoid per-write overhead
- **Logging to stderr from C:** User decided exception-only, no C-side logging
- **Including requested count in overflow message:** User explicitly excluded this
- **Validating internal C-to-C calls:** Only entry points validate (user decision)

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Static analysis | Custom lint scripts | cppcheck + clang-tidy | Decades of development, comprehensive rule sets |
| Buffer overflow detection at runtime | Custom memory allocator | Pre-check pattern (already decided) | Simpler, zero overhead on hot path |

## Common Pitfalls

### Pitfall 1: Breaking the Hot Path Performance
**What goes wrong:** Adding validation inside `run_instruction()` or the hot path C functions adds overhead to every gate operation.
**Why it happens:** Natural instinct is to validate at every C entry point, but `run_instruction` is called millions of times per circuit.
**How to avoid:** Validate the circuit pointer ONCE at the Cython level before entering the hot loop. The C entry-point wrappers are for functions called less frequently. For hot paths, the Cython code already has `_circuit_initialized` checks.
**Warning signs:** Benchmark regression > 15%.

### Pitfall 2: Forgetting to Update `_core.pxd` Declarations
**What goes wrong:** Adding new C functions (validated wrappers) without declaring them in `_core.pxd` causes Cython compilation failures.
**Why it happens:** The `.pxd` file is the Cython-visible interface to C.
**How to avoid:** For every new function added to a `.h` file that Cython calls, add a corresponding `cdef extern` declaration in `_core.pxd`.

### Pitfall 3: cppcheck False Positives on Sequence Files
**What goes wrong:** The ~150+ generated sequence `.c` files contain repetitive patterns that trigger style/performance warnings.
**Why it happens:** Auto-generated code optimizes for correctness and regularity, not cppcheck's preferences.
**How to avoid:** Use `suppressions.txt` to suppress known false positives in sequence files. Document each suppression with justification.

### Pitfall 4: clang-tidy Check Overload
**What goes wrong:** Enabling all clang-tidy checks produces thousands of warnings, many irrelevant to a C (not C++) codebase.
**Why it happens:** Many clang-tidy checks target C++ patterns.
**How to avoid:** Select C-relevant check groups only: `bugprone-*`, `cert-*`, `clang-analyzer-*`, `misc-*`. Exclude C++-specific checks like `modernize-*`, `cppcoreguidelines-*`.

### Pitfall 5: Changing Existing Function Signatures
**What goes wrong:** Changing `void run_instruction(...)` to return `int` breaks all internal callers.
**Why it happens:** Temptation to modify existing functions rather than adding wrappers.
**How to avoid:** Add NEW wrapper functions (e.g., `validated_run_instruction`) that call the originals. This keeps internal C-to-C calls unchanged (per user decision).

## Code Examples

### Validation Header Pattern
```c
// c_backend/include/validation.h
#ifndef QUANTUM_VALIDATION_H
#define QUANTUM_VALIDATION_H

#include "types.h"

// Error codes
#define QV_OK          0
#define QV_NULL_CIRC  -1
#define QV_NULL_SEQ   -2
#define QV_NULL_ARG   -3

// Qubit array scratch buffer limit
#define QV_MAX_QUBIT_SLOTS 384

#endif // QUANTUM_VALIDATION_H
```

### Cython Validation Wrapper Pattern
```python
# In _core.pyx -- before calling C functions
cdef inline void _validate_circuit() except *:
    """Raise ValueError if circuit pointer is NULL."""
    if not _circuit_initialized or _circuit == NULL:
        raise ValueError(
            "[Validation] error in circuit operation: circuit pointer is NULL"
        )

cdef inline void _validate_qubit_slots(int required, str func_name) except *:
    """Raise OverflowError if qubit_array write would exceed 384 slots."""
    if required > 384:
        raise OverflowError(
            f"[Buffer] error in {func_name}: slot count exceeded, max 384"
        )
```

### cppcheck Invocation Pattern
```bash
# Run cppcheck on all C backend source files
cppcheck --enable=all --inconclusive \
    --suppressions-list=c_backend/suppressions.txt \
    -I c_backend/include \
    --std=c11 \
    --error-exitcode=1 \
    c_backend/src/ c_backend/include/
```

### clang-tidy Invocation Pattern
```bash
# Run clang-tidy with C-relevant checks
clang-tidy \
    -checks='bugprone-*,cert-*,clang-analyzer-*,misc-*,-misc-unused-parameters' \
    -header-filter='c_backend/include/.*' \
    c_backend/src/*.c \
    -- -I c_backend/include -std=c11
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `assert()` for NULL checks | Return codes + exception translation | Current best practice | assert compiles away in release builds; return codes survive |
| No static analysis | cppcheck + clang-tidy as standard CI pair | ~2020+ | Catches undefined behavior, memory leaks, logic errors before runtime |

**Deprecated/outdated:**
- Using `assert()` for runtime safety in release builds: assert is debug-only, compiles to nothing with `-DNDEBUG`

## Open Questions

1. **Hot path validation granularity**
   - What we know: Hot path functions (`hot_path_add_qq`, etc.) take a `circuit_t*` and are called from Cython
   - What's unclear: Whether to add C-level validation wrappers for hot paths or only validate at the Cython level
   - Recommendation: Validate at Cython level only for hot paths (performance-critical); add C wrappers for non-hot-path functions (visualize, stats, optimize, etc.)

2. **Sequence file cppcheck scope**
   - What we know: There are ~150+ auto-generated sequence `.c` files in `c_backend/src/sequences/`
   - What's unclear: Whether to run cppcheck on these or suppress the entire directory
   - Recommendation: Run cppcheck on everything, suppress per-pattern in `suppressions.txt` (user decided to fix ALL severities, but false positives get suppressed with justification)

## Sources

### Primary (HIGH confidence)
- Codebase analysis: `_core.pyx`, `_core.pxd`, `execution.c`, `circuit.h`, `types.h`, `hot_path_add.c`
- Codebase analysis: `qint.pyx`, `qint_preprocessed.pyx`, `qint_arithmetic.pxi`, `qint_bitwise.pxi`, `qint_comparison.pxi`
- CONTEXT.md decisions (user-locked constraints)

### Secondary (MEDIUM confidence)
- cppcheck documentation: standard invocation patterns, suppression file format
- clang-tidy documentation: check group naming conventions

## Metadata

**Confidence breakdown:**
- Entry point inventory: HIGH - direct codebase analysis
- Validation pattern design: HIGH - follows existing codebase conventions
- cppcheck/clang-tidy approach: MEDIUM - tool availability and version-specific behavior may vary
- Performance impact assessment: HIGH - validation is single-branch per entry, user-confirmed pre-check pattern

**Research date:** 2026-02-23
**Valid until:** 2026-03-23 (stable domain, no external dependencies changing)
