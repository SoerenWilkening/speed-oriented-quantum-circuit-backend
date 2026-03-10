# Requirements: Quantum Assembly

**Defined:** 2026-03-09
**Core Value:** Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.

## v9.0 Requirements

Requirements for v9.0 Nested Controls & Chess Engine. Each maps to roadmap phases.

### Nested Controls

- [x] **CTRL-01**: User can nest `with qbool:` blocks at arbitrary depth with correct controlled gate emission
- [x] **CTRL-02**: Framework composes control qubits via Toffoli AND on `__enter__`, producing a combined control ancilla
- [x] **CTRL-03**: Framework uncomputes AND-ancilla and restores previous control on `__exit__`
- [x] **CTRL-04**: Controlled XOR (`~qbool`) works inside `with` blocks without NotImplementedError
- [x] **CTRL-05**: Existing single-level `with` blocks work identically (zero regression)
- [x] **CTRL-06**: Nested controls work inside `@ql.compile` captured functions

### 2D Qarray

- [x] **ARR-01**: User can create 2D qarrays via `ql.qarray(dim=(rows, cols), dtype=ql.qbool)`
- [x] **ARR-02**: User can index 2D qarrays with `arr[r, c]` for read and in-place mutation

### Chess Engine

- [x] **CHESS-01**: Chess engine in `examples/chess_engine.py` follows the readable natural-programming format
- [x] **CHESS-02**: Engine compiles successfully with `@ql.compile(opt=1)` without OOM
- [x] **CHESS-03**: Engine includes move legality checking (piece-exists, no-friendly-capture, check detection)
- [x] **CHESS-04**: Engine includes walk operators (R_A/R_B) and diffusion in readable style
- [x] **CHESS-05**: Engine is circuit-build-only (no simulation required)

## Future Requirements

Deferred to future release. Tracked but not in current roadmap.

### Controlled Bitwise

- **CBIT-01**: Controlled AND (`a & b`) works inside `with` blocks
- **CBIT-02**: Controlled OR (`a | b`) works inside `with` blocks

### Chess Verification

- **CVER-01**: Monte Carlo verification framework for heuristic circuit correctness
- **CVER-02**: Small-board simulation for nested control correctness

### Walk Simplification

- **WALK-01**: Remove V-gate CCRy workaround in walk.py once nested `with` blocks work

## Out of Scope

| Feature | Reason |
|---------|--------|
| Controlled multiplication/division inside `with` | Complex ancilla interactions, not needed for chess |
| General game engine API (`ql.game_tree()`) | Needs stable chess engine as reference first |
| Sparse 2D qarray | Quantum computation requires all positions in superposition |
| Full Shor's algorithm | Separate milestone, modular exponentiation not related |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CTRL-01 | Phase 118 | Complete |
| CTRL-02 | Phase 117 | Complete |
| CTRL-03 | Phase 117 | Complete |
| CTRL-04 | Phase 118 | Complete |
| CTRL-05 | Phase 118 | Complete |
| CTRL-06 | Phase 119 | Complete |
| ARR-01 | Phase 120 | Complete |
| ARR-02 | Phase 120 | Complete |
| CHESS-01 | Phase 121 | Complete |
| CHESS-02 | Phase 121 | Complete |
| CHESS-03 | Phase 121 | Complete |
| CHESS-04 | Phase 121 | Complete |
| CHESS-05 | Phase 121 | Complete |

**Coverage:**
- v9.0 requirements: 13 total
- Mapped to phases: 13
- Unmapped: 0

---
*Requirements defined: 2026-03-09*
*Last updated: 2026-03-09 after roadmap creation*
