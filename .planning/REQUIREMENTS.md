# Requirements: Quantum Assembly

**Defined:** 2026-01-29
**Core Value:** Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.

## v1.3 Requirements

Requirements for Package Structure & ql.array milestone. Each maps to roadmap phases.

### Package Structure (PKG)

- [ ] **PKG-01**: Create proper Python package with `__init__.py` files
- [ ] **PKG-02**: Split large Cython files to ~200-300 lines each
- [ ] **PKG-03**: Maintain all existing functionality after restructuring
- [ ] **PKG-04**: Update imports across codebase to use new package structure

### Array Class Core (ARR)

- [ ] **ARR-01**: Create `ql.array` class for homogeneous quantum arrays
- [ ] **ARR-02**: Support qint arrays (array of quantum integers)
- [ ] **ARR-03**: Support qbool arrays (array of quantum booleans)
- [ ] **ARR-04**: Initialize array from Python list with auto-width inference
- [ ] **ARR-05**: Initialize array with explicit width parameter
- [ ] **ARR-06**: Validate homogeneity (reject mixed qint/qbool)

### Array Reductions (RED)

- [ ] **RED-01**: AND reduction (`&A`) with pairwise tree for O(log n) depth
- [ ] **RED-02**: OR reduction (`|A`) with pairwise tree for O(log n) depth
- [ ] **RED-03**: XOR reduction (`^A`) with pairwise tree for O(log n) depth
- [ ] **RED-04**: Sum reduction `sum(A)` with pairwise tree for O(log n) depth

### Element-wise Operations (ELM)

- [ ] **ELM-01**: Element-wise arithmetic (`A + B`, `A - B`, `A * B`) between arrays
- [ ] **ELM-02**: Element-wise bitwise (`A & B`, `A | B`, `A ^ B`) between arrays
- [ ] **ELM-03**: Element-wise comparison (`A < B`, `A > B`, `A <= B`, `A >= B`, `A == B`, `A != B`) between arrays
- [ ] **ELM-04**: Array length validation for element-wise operations

### Python Integration (PYI)

- [ ] **PYI-01**: `len(A)` returns array length
- [ ] **PYI-02**: Iteration support (`for x in A`)
- [ ] **PYI-03**: Indexing support (`A[i]`)
- [ ] **PYI-04**: Slicing support (`A[i:j]`)

## Future Requirements

Deferred to later milestones.

### Visualization & Debugging

- **VIS-01**: Debug visualization for uncomputation
- **VIS-02**: Qubit statistics tracking
- **VIS-03**: OpenQASM comment markers for uncomputation blocks

### Advanced Operations

- **ADV-01**: Bit shift operations for qint (<<, >>)
- **ADV-02**: OpenQASM 3.0 export format

## Out of Scope

Explicitly excluded from v1.3.

| Feature | Reason |
|---------|--------|
| Mixed-type arrays | Homogeneous arrays simpler, cleaner semantics |
| Array broadcasting | NumPy-style broadcasting adds complexity, defer |
| Sparse arrays | Not needed for current use cases |
| Multi-dimensional arrays | 1D sufficient for this milestone |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PKG-01 | — | Pending |
| PKG-02 | — | Pending |
| PKG-03 | — | Pending |
| PKG-04 | — | Pending |
| ARR-01 | — | Pending |
| ARR-02 | — | Pending |
| ARR-03 | — | Pending |
| ARR-04 | — | Pending |
| ARR-05 | — | Pending |
| ARR-06 | — | Pending |
| RED-01 | — | Pending |
| RED-02 | — | Pending |
| RED-03 | — | Pending |
| RED-04 | — | Pending |
| ELM-01 | — | Pending |
| ELM-02 | — | Pending |
| ELM-03 | — | Pending |
| ELM-04 | — | Pending |
| PYI-01 | — | Pending |
| PYI-02 | — | Pending |
| PYI-03 | — | Pending |
| PYI-04 | — | Pending |

**Coverage:**
- v1.3 requirements: 22 total
- Mapped to phases: 0
- Unmapped: 22 (pending roadmap creation)

---
*Requirements defined: 2026-01-29*
*Last updated: 2026-01-29 after initial definition*
