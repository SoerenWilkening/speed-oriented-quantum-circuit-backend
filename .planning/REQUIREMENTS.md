# Requirements: Quantum Assembly

**Defined:** 2026-01-25
**Core Value:** Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Foundation

- [ ] **FOUND-01**: Functions that modify circuit state accept explicit circuit_t* context (sequence-returning functions remain stateless)
- [ ] **FOUND-02**: Memory ownership documented at every allocation point
- [ ] **FOUND-03**: All sizeof() usage corrected (use sizeof(type) not sizeof(pointer))
- [ ] **FOUND-04**: All structure fields initialized before use
- [ ] **FOUND-05**: NULL checks after all malloc/calloc operations
- [ ] **FOUND-06**: Qubit allocator centralizes qubit lifecycle management
- [ ] **FOUND-07**: qint/qbool borrow qubits from circuit with tracked ownership
- [ ] **FOUND-08**: Ancilla qubit allocation is trackable without significant performance overhead

### Testing Infrastructure

- [ ] **TEST-01**: Characterization test suite captures current behavior before refactoring
- [ ] **TEST-02**: C backend unit tests cover core functions
- [ ] **TEST-03**: Python API unit tests cover qint/qbool operations
- [ ] **TEST-04**: Valgrind/ASan memory testing integrated into development workflow
- [ ] **TEST-05**: Pre-commit hooks enforce code quality (Ruff, linting)

### Variable-Width Integers

- [ ] **VINT-01**: QInt constructor accepts width parameter for arbitrary bit sizes
- [ ] **VINT-02**: Dynamic allocation based on width parameter
- [ ] **VINT-03**: Width validation in arithmetic operations
- [ ] **VINT-04**: Mixed-width integer operations handled correctly

### Arithmetic Operations

- [ ] **ARTH-01**: Addition works for variable-width integers
- [ ] **ARTH-02**: Subtraction works for variable-width integers
- [ ] **ARTH-03**: Multiplication works for any integer size (not just fixed)
- [ ] **ARTH-04**: Comparisons (>, <, ==, >=, <=) work for variable-width integers
- [ ] **ARTH-05**: Division operation implemented
- [ ] **ARTH-06**: Modulo operation implemented
- [ ] **ARTH-07**: Modular arithmetic (add/sub/mul mod N) implemented

### Bit Operations

- [ ] **BITOP-01**: Bitwise AND on quantum integers
- [ ] **BITOP-02**: Bitwise OR on quantum integers
- [ ] **BITOP-03**: Bitwise XOR on quantum integers
- [ ] **BITOP-04**: Bitwise NOT on quantum integers
- [ ] **BITOP-05**: Python operator overloading for bitwise ops (&, |, ^, ~)

### Circuit Output

- [ ] **CIRC-01**: Text-based circuit visualization for debugging
- [ ] **CIRC-02**: Automatic gate merging optimization
- [ ] **CIRC-03**: Inverse gate cancellation
- [ ] **CIRC-04**: Circuit statistics (depth, gate count, qubit usage)

### Code Organization

- [ ] **CODE-01**: QPU.c responsibilities separated into focused modules
- [ ] **CODE-02**: Circuit builder module (create/destroy/add gates)
- [ ] **CODE-03**: Circuit optimizer module (layer assignment, merging)
- [ ] **CODE-04**: Operations organized by category (arithmetic, comparison, logic)
- [ ] **CODE-05**: Clear dependency graph between modules

### Documentation

- [ ] **DOCS-01**: Comprehensive docstrings (NumPy style)
- [ ] **DOCS-02**: Sphinx documentation with examples
- [ ] **DOCS-03**: API reference auto-generated from docstrings
- [ ] **DOCS-04**: Tutorial examples demonstrating features

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Extended Bit Operations

- **BITOP-06**: Bit shifts (left/right logical)
- **BITOP-07**: Bit shifts (arithmetic)
- **BITOP-08**: Bit rotations (left/right)

### Output Formats

- **OUT-01**: OpenQASM 3.0 export (mid-circuit measurement, real-time conditionals)
- **OUT-02**: Multiple hardware backend support

### Advanced Features

- **ADV-01**: QFT-based arithmetic (more efficient than ripple-carry)
- **ADV-02**: Exponentiation operations
- **ADV-03**: Circuit templates library (QFT, Grover, phase estimation)
- **ADV-04**: Direct/immediate execution mode

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Direct execution mode | Future direction, keep circuit compilation approach for v1 |
| Hardware integration | OpenQASM export handles most cases |
| ML framework integration | Requires stable API first, complex integration |
| Real-time debugging | Complex infrastructure requirement |
| GUI interface | Programmatic API sufficient for v1 |
| Direct quantum state access | Violates no-cloning theorem |
| Automatic qubit cloning | Physically impossible |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| FOUND-01 | TBD | Pending |
| FOUND-02 | TBD | Pending |
| FOUND-03 | TBD | Pending |
| FOUND-04 | TBD | Pending |
| FOUND-05 | TBD | Pending |
| FOUND-06 | TBD | Pending |
| FOUND-07 | TBD | Pending |
| FOUND-08 | TBD | Pending |
| TEST-01 | TBD | Pending |
| TEST-02 | TBD | Pending |
| TEST-03 | TBD | Pending |
| TEST-04 | TBD | Pending |
| TEST-05 | TBD | Pending |
| VINT-01 | TBD | Pending |
| VINT-02 | TBD | Pending |
| VINT-03 | TBD | Pending |
| VINT-04 | TBD | Pending |
| ARTH-01 | TBD | Pending |
| ARTH-02 | TBD | Pending |
| ARTH-03 | TBD | Pending |
| ARTH-04 | TBD | Pending |
| ARTH-05 | TBD | Pending |
| ARTH-06 | TBD | Pending |
| ARTH-07 | TBD | Pending |
| BITOP-01 | TBD | Pending |
| BITOP-02 | TBD | Pending |
| BITOP-03 | TBD | Pending |
| BITOP-04 | TBD | Pending |
| BITOP-05 | TBD | Pending |
| CIRC-01 | TBD | Pending |
| CIRC-02 | TBD | Pending |
| CIRC-03 | TBD | Pending |
| CIRC-04 | TBD | Pending |
| CODE-01 | TBD | Pending |
| CODE-02 | TBD | Pending |
| CODE-03 | TBD | Pending |
| CODE-04 | TBD | Pending |
| CODE-05 | TBD | Pending |
| DOCS-01 | TBD | Pending |
| DOCS-02 | TBD | Pending |
| DOCS-03 | TBD | Pending |
| DOCS-04 | TBD | Pending |

**Coverage:**
- v1 requirements: 37 total
- Mapped to phases: 0
- Unmapped: 37 (pending roadmap creation)

---
*Requirements defined: 2026-01-25*
*Last updated: 2026-01-25 after initial definition*
