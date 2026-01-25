# Quantum Assembly

## What This Is

A quantum programming framework that enables writing quantum algorithms using standard programming constructs — operator overloading on quantum integers (qint) and quantum booleans (qbool), with Python's `with` statement implementing quantum conditionals. Operations compile to optimized quantum circuits via a C backend with Cython bindings.

## Core Value

Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.

## Requirements

### Validated

- ✓ Basic arithmetic (+, - for same-size integers) — existing
- ✓ Multiplication (fixed integer size) — existing
- ✓ Comparisons (<=, ==, >= via subtraction + MSB check, fixed size) — existing
- ✓ Logical operations for qbool (AND, OR, XOR, NOT) — existing
- ✓ Python operator overloading for qint/qbool — existing
- ✓ Quantum conditionals via `with` statement — existing
- ✓ Circuit generation and OpenQASM output — existing
- ✓ Performance benchmarks (QFT up to 2000 variables) — existing in circuit-gen-results/

### Active

- [ ] Clean C layer — remove dead code, fix memory bugs, simplify globals
- [ ] Proper memory allocation — classical data structures and qubit management
- [ ] Variable integer size support for all arithmetic operations
- [ ] Complete multiplication for variable integer sizes
- [ ] Complete comparisons for variable integer sizes
- [ ] Bit operations for qint (AND, OR, XOR, NOT at integer level)
- [ ] Separate multi-purpose functions (qint vs qbool handling)
- [ ] Unit tests for C backend
- [ ] Unit tests for Python API
- [ ] Documentation (API reference, usage guide)

### Out of Scope

- Direct/immediate execution mode — future direction, not current scope
- Hardware integration — future, keep circuit compilation approach
- Real-time simulator integration — future consideration

## Context

**Architecture:** Three-layer design — C backend (gate primitives, circuit management, integer operations) → Cython bindings → Python frontend (qint/qbool classes, operator overloading).

**Current state:** Mid-restructuring. C code contains remnants from before Python integration. Memory management inconsistent (incorrect sizeof, missing frees, uninitialized structures). Global state management is messy. Operations work but many are constrained to fixed integer sizes (INTEGERSIZE constant).

**Performance:** Benchmarks in `circuit-gen-results/` demonstrate speed and memory superiority over Qiskit, Cirq, PennyLane, and other backends for QFT circuits up to 2000 variables.

**Codebase map:** Detailed analysis in `.planning/codebase/` documents architecture, concerns, and tech debt.

**Target audience:** Open source quantum computing community.

## Constraints

- **Approach**: Clean C layer first, then build up — bottom-up restructuring
- **Preserve**: Benchmark results in `circuit-gen-results/` must remain intact
- **Architecture**: Keep `add_gate` circuit-building approach; direct execution is future work
- **Compatibility**: Maintain Python API compatibility where possible during restructuring

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Bottom-up restructuring (C first) | Foundation must be solid before adding features | — Pending |
| Open source release target | Requires clean code, docs, tests | — Pending |
| Keep circuit compilation model | Direct execution is future; current approach works | — Pending |

---
*Last updated: 2026-01-25 after initialization*
