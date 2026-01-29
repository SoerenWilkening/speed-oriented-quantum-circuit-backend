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
- [ ] **ARR-04**: Initialize array from Python list or NumPy array with auto-width inference: `ql.array([1, 2, 3])` or `ql.array(np_array)`
- [ ] **ARR-05**: Initialize array with explicit width parameter: `ql.array([1, 2, 3], width=8)`
- [ ] **ARR-06**: Initialize array from dimensions with dtype: `ql.array(dim=(3,3), dtype=ql.qint)`
- [ ] **ARR-07**: Support multi-dimensional arrays with arbitrary shapes
- [ ] **ARR-08**: Validate homogeneity (reject mixed qint/qbool)

### Array Reductions (RED)

- [ ] **RED-01**: AND reduction (`&A`) with pairwise tree for O(log n) depth
- [ ] **RED-02**: OR reduction (`|A`) with pairwise tree for O(log n) depth
- [ ] **RED-03**: XOR reduction (`^A`) with pairwise tree for O(log n) depth
- [ ] **RED-04**: Sum reduction `sum(A)` with pairwise tree for O(log n) depth

### Element-wise Operations (ELM)

- [ ] **ELM-01**: Element-wise arithmetic (`A + B`, `A - B`, `A * B`) between arrays
- [ ] **ELM-02**: Element-wise bitwise (`A & B`, `A | B`, `A ^ B`) between arrays
- [ ] **ELM-03**: Element-wise comparison (`A < B`, `A > B`, `A <= B`, `A >= B`, `A == B`, `A != B`) between arrays
- [ ] **ELM-04**: Array shape validation for element-wise operations
- [ ] **ELM-05**: In-place element-wise operations (`A += B`, `A -= B`, `A *= B`, `A &= B`, `A |= B`, `A ^= B`)

### Python Integration (PYI)

- [ ] **PYI-01**: `len(A)` returns total element count (flattened length)
- [ ] **PYI-02**: Iteration support (`for x in A` iterates over flattened elements)
- [ ] **PYI-03**: NumPy-style indexing (`A[i]`, `A[i,j]`, `A[:, j]` column access)
- [ ] **PYI-04**: NumPy-style slicing (`A[1:3]`, `A[1:3, 0:2]`)

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
| Axis parameter for reductions | Flatten-then-reduce simpler for v1.3 |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PKG-01 | Phase 21 | Complete |
| PKG-02 | Phase 21 | Complete |
| PKG-03 | Phase 21 | Complete |
| PKG-04 | Phase 21 | Complete |
| ARR-01 | Phase 22 | Complete |
| ARR-02 | Phase 22 | Complete |
| ARR-03 | Phase 22 | Complete |
| ARR-04 | Phase 22 | Complete |
| ARR-05 | Phase 22 | Complete |
| ARR-06 | Phase 22 | Complete |
| ARR-07 | Phase 22 | Complete |
| ARR-08 | Phase 22 | Complete |
| RED-01 | Phase 23 | Complete |
| RED-02 | Phase 23 | Complete |
| RED-03 | Phase 23 | Complete |
| RED-04 | Phase 23 | Complete |
| ELM-01 | Phase 24 | Complete |
| ELM-02 | Phase 24 | Complete |
| ELM-03 | Phase 24 | Complete |
| ELM-04 | Phase 24 | Complete |
| ELM-05 | Phase 24 | Complete |
| PYI-01 | Phase 22 | Complete |
| PYI-02 | Phase 22 | Complete |
| PYI-03 | Phase 22 | Complete |
| PYI-04 | Phase 22 | Complete |

**Coverage:**
- v1.3 requirements: 25 total
- Mapped to phases: 25
- Unmapped: 0

---
*Requirements defined: 2026-01-29*
*Last updated: 2026-01-29 after Phase 24 completion*
