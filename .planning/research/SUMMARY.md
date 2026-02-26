# Project Research Summary

**Project:** Quantum Assembly v6.0 — Quantum Walk Primitives (Montanaro 2015 Backtracking)
**Domain:** Quantum programming framework — discrete-time quantum walk for constraint satisfaction speedup
**Researched:** 2026-02-26
**Confidence:** HIGH (stack and architecture verified from codebase; algorithm from peer-reviewed literature; reference implementation studied in Qrisp)

## Executive Summary

v6.0 implements Montanaro's 2015 backtracking speedup algorithm by adding a new `quantum_walk` module (~400-600 lines) to the existing Quantum Assembly framework. The entire implementation is pure Python composing existing gate primitives — zero new C-level gates, zero new Cython modules, and zero new external dependencies. Every required gate (Ry, CRy, H, CH, X, CX, MCZ, MCX, P, CP) already exists in the C backend and is accessible via `_gates.pyx`. The core work is algorithmic: constructing the local diffusion operator D_x (a node-weighted reflection), the walk operators R_A and R_B (parity-partitioned parallel diffusions), and the detection algorithm (iterative phase estimation on the walk step). A Qrisp reference implementation was studied in detail and the critical patterns — one-hot height register, parity-controlled diffusion, phi = 2*arctan(sqrt(d)) rotation angle — have been validated against Montanaro's original paper and the Qrisp Sudoku paper.

The recommended approach is a 3-file module split mirroring the existing grover.py/oracle.py/diffusion.py split: `quantum_walk.py` (public API and detection algorithm), `quantum_walk_tree.py` (QuantumBacktrackingTree class managing the one-hot height + QuantumArray branch registers), and `quantum_walk_diffusion.py` (D_x operator, R_A/R_B via height-parity-controlled diffusion). The walk step U = R_B * R_A is wrapped in `@ql.compile` for caching and to enable the controlled variant needed for phase estimation. The 17-qubit simulator ceiling constrains demo scope to a binary tree of depth 2-3 with a simple SAT predicate (10-11 qubits total), which leaves comfortable headroom for predicate ancillae.

The highest-risk integration hazard is the interaction between the walk operator's internal compute-use-uncompute predicate pattern and the framework's LIFO scope-based automatic uncomputation. Unlike Grover's oracle (a self-contained block), the walk predicate must be evaluated, used as a diffusion control, and then uncomputed — all within a single operator scope — in a pattern the framework's auto-uncompute system was not designed for. The mitigation is to build the walk operator below the scope-tracking layer using `_allocate_qubit()` / `_deallocate_qubits()` directly, or to compile the predicate with `@ql.compile` and call `.inverse()` explicitly. A second critical silent-failure risk is incorrect amplitude coefficients in the local diffusion — using 1/sqrt(d) instead of 1/sqrt(d+1) (parent included in count) produces a structurally valid circuit that silently destroys the O(sqrt(T)) speedup guarantee with no error output.

## Key Findings

### Recommended Stack

No new dependencies are required. See `.planning/research/STACK.md` for full codebase-verified analysis.

**Core technologies:**
- Python 3.11+: new `quantum_walk.py` module — algorithmic composition of existing emit_* functions into walk operators; no new primitives
- Cython `_gates.pyx` (existing): gate emission (emit_ry, emit_h, emit_x, emit_mcz, emit_p) — all required gate primitives confirmed present
- C backend gate.h (existing): ry, cry, h, ch, x, cx, mcz, mcx, p, cp — complete coverage verified; no additions required
- NumPy (existing): angle calculations (arctan, sqrt) for diffusion amplitudes — no new features needed
- Qiskit + qiskit-aer (existing): verification via sim_backend.py — identical pipeline to Grover/amplitude estimation verification
- `@ql.compile` (existing): walk step caching and controlled variant derivation for phase estimation powers

### Expected Features

See `.planning/research/FEATURES.md` for the full feature landscape including dependency graph.

**Must have (table stakes — v6.0 MVP):**
- Tree state encoding (one-hot height register + QuantumArray branch registers) — foundational; every other feature depends on this decision
- accept/reject predicate interface — defines the backtracking tree; predicates must be mutually exclusive and uncomputable with zero ancilla delta
- Local diffusion operator D_x (uniform branching) — core building block; amplitude angle phi = 2*arctan(sqrt(d)); root uses different formula
- Walk operators R_A and R_B — parity-partitioned parallel diffusions; R_A on even-depth nodes, R_B on odd-depth nodes plus root reflection
- Walk step U = R_B * R_A — compiled and cacheable; controlled variant required for phase estimation
- Detection algorithm — iterative power method on walk step; threshold probability > 3/8 indicates solution exists
- Python API `ql.quantum_walk()` — API style consistent with `ql.grover()`; returns WalkResult with `.has_solution` boolean
- Demo + Qiskit verification — binary tree depth 2-3, 2-variable SAT predicate, within 17-qubit budget

**Should have (v6.x after core is validated):**
- Variable branching support (dynamic d(x)) — required for real CSP instances where rejection prunes differently at each node; deferred due to very high complexity (child counting + controlled Ry lookup table)
- Subspace optimization — halves circuit depth by allowing reject to return True on non-algorithmic states; requires specific predicate properties
- Solution finding via hybrid classical-quantum Algorithm 2 — recursive subtree detection; depends on detection working correctly first
- Circuit resource estimator — computes qubit count before construction; fail-fast before hitting simulation ceiling

**Defer (v7+):**
- General graph quantum walks (non-tree) — fundamentally different mathematical framework; separate future module
- CSP-to-predicate compiler (auto CNF to accept/reject) — encoding optimization is a research problem; let users control encoding
- Fault-tolerant walk operators with T-count optimization — need correct operators before optimizing them

**Anti-features explicitly rejected:**
- Continuous-time quantum walk (CTQW) — incompatible mathematical framework requiring Hamiltonian simulation
- IQAE reuse for detection — IQAE convergence assumes Grover operator structure; walk operator eigenvalue analysis is different; would give incorrect confidence intervals
- QPE with arbitrary precision — each precision qubit costs one register qubit under the 17-qubit ceiling

### Architecture Approach

The quantum walk module is a new, self-contained layer in the Python frontend that composes existing Cython/C infrastructure. No existing module is modified except `__init__.py` for exports. See `.planning/research/ARCHITECTURE.md` for full component breakdown, data flow diagrams, and integration point tables.

**Major components:**
1. `quantum_walk.py` — public API (`ql.quantum_walk()`, `ql.detect_solution()`), detection/search algorithms, adaptive walk iteration, simulation/measurement layer, `__init__.py` export point
2. `quantum_walk_tree.py` — `QuantumBacktrackingTree` class: allocates branch_qa (ql.array, one entry per depth level) and one-hot height register (max_depth+1 qubits via `_allocate_qubit()`), manages node initialization and height qubit access interface
3. `quantum_walk_diffusion.py` — D_x construction (Ry + MCZ + Ry_dag), R_A/R_B via height-parity-controlled diffusion using `with h_qubit:` controlled context, predicate evaluation and explicit uncomputation via capture-reverse pattern

**Key architectural decisions validated by research:**
- All Python, no new C code — walk is compositional at 3-5 depth levels, not computational at bit-width scale; Python overhead is negligible
- One-hot height encoding preferred over binary — single-qubit control per depth level vs. multi-qubit equality check; matches Qrisp reference
- Controlled context (`with qbool:`) for parity-controlled diffusion — auto-derives CRy from Ry, CH from H; no explicit controlled gate emission needed
- Predicate built below LIFO scope tracking — walk operator manages compute-use-uncompute cycle directly with `_allocate_qubit()` / `_deallocate_qubits()`

### Critical Pitfalls

See `.planning/research/PITFALLS.md` for all 12 pitfalls with severity ratings, recovery costs, and phase-to-address mapping.

1. **Wrong amplitude coefficients in D_x (silent failure)** — The local diffusion superposition is |psi_x> = 1/sqrt(d(x)+1) * (|x> + sum children); the "+1" counts the parent node. Using 1/sqrt(d) or 1/sqrt(max_d) instead produces structurally valid circuits that silently destroy the O(sqrt(T)) speedup guarantee. Prevention: unit test that prepares |psi_x> and verifies amplitudes via statevector simulation before any walk operator integration. This test must pass before Phase 2 is considered complete.

2. **Predicate ancilla leak into LIFO uncomputation (double-uncompute)** — The walk operator evaluates, uses, and uncomputes the predicate within a single operator scope. The framework's scope-based auto-uncompute fires at scope exit, causing double-uncomputation of the predicate ancillae and corrupting the walk state. Prevention: use `_allocate_qubit()` / `_deallocate_qubits()` directly inside the walk operator (bypassing scope tracking), or compile the predicate with `@ql.compile` and call `.inverse()` explicitly. Never construct qint/qbool inside walk operator scopes.

3. **R_A and R_B register overlap (walk operator invalid)** — If the branch path uses a single shared integer register, R_A and R_B are not disjoint and the walk operator does not implement the correct product of reflections. Prevention: QuantumArray with one register entry per depth level (branch_qa[depth]) enforces disjointness structurally. Validate with qubit-index enumeration test verifying zero overlap between R_A and R_B qubit sets.

4. **Qubit budget explosion under 17-qubit ceiling** — Height register (max_depth+1 one-hot) + branch register (max_depth * ceil(log2(d))) + predicate ancillae + precision qubits for detection can easily exceed 17 qubits for depth >= 4 trees. Prevention: implement resource estimator in Phase 1 that computes exact qubit count before any circuit construction; target binary tree of depth 2-3 as demo.

5. **Root node diffusion uses a different amplitude formula** — The root uses phi_root = 2*arctan(sqrt(N*d)) where N is a tree-size tuning parameter, not the standard phi = 2*arctan(sqrt(d)) used for all other nodes. Using uniform angles for all nodes corrupts the spectral gap. Prevention: implement root diffusion as an explicit separate code path controlled on h[max_depth]; test independently via statevector amplitude verification.

## Implications for Roadmap

Based on combined research, 5 phases are suggested. The ARCHITECTURE.md research provides an explicit build order; the PITFALLS.md maps each pitfall to the phase where it must be addressed. All Phase 1 decisions are architectural — changing them later requires rewrites.

### Phase 1: Tree Encoding Foundation and Module Architecture

**Rationale:** The tree register layout (QuantumArray branch_qa + one-hot height) is foundational. Every subsequent component depends on it. Four pitfalls (R_A/R_B overlap, qubit budget, uncomputation conflict, encoding convention) require architectural decisions that cannot be retrofitted. The predicate interface design (accept/reject callables, mutual exclusion validation, ancilla delta = 0 constraint) must also be settled before any gate emission code is written. This phase produces data structures and scaffolding only — no gate emission.
**Delivers:** `QuantumBacktrackingTree` class with register allocation and initialization; three-file module scaffold; resource estimator (outputs exact qubit count for given tree parameters); predicate interface specification with classical mutual exclusion validation; height/depth convention documented and enforced throughout
**Addresses:** Tree state encoding (FEATURES.md table stakes); accept/reject predicate interface (FEATURES.md table stakes)
**Avoids:** Pitfall 4 (R_A/R_B overlap — QuantumArray per level enforces disjointness), Pitfall 5 (qubit budget — resource estimator built here), Pitfall 6 (uncomputation conflict — raw qubit allocation strategy decided here), Pitfall 9 (encoding convention — height-based, one-hot, established in this phase), Pitfall 10 (accept/reject mutual exclusion — validated at API level before circuit construction)

### Phase 2: Local Diffusion Operator D_x

**Rationale:** D_x is the core building block of R_A, R_B, and the walk step. It must be verified correct in isolation before it is composed into larger operators. The amplitude coefficient formula (phi = 2*arctan(sqrt(d))) and the root special case (phi_root with tuning parameter N) must each be validated via statevector tests before any walk operator testing begins. The predicate compute-use-uncompute pattern is established in this phase as the canonical approach for all walk operator internals.
**Delivers:** `_emit_local_diffusion()` for uniform branching with correct phi formula; root diffusion as a separate code path controlled on h[max_depth]; predicate compute-use-uncompute pattern (evaluate, use as diffusion control, uncompute via `@ql.compile` inverse or raw capture-reverse); statevector tests verifying |psi_x> amplitudes at 1/sqrt(d(x)+1) tolerance
**Uses:** emit_ry, emit_h, emit_x, emit_mcz from `_gates.pyx`; `_allocate_qubit()` / `_deallocate_qubits()` from `_core.pyx`; `with qbool:` height-controlled context
**Avoids:** Pitfall 1 (wrong amplitudes — verified here before integration), Pitfall 3 (predicate uncomputation — canonical pattern established), Pitfall 11 (root special case — implemented and tested here)

### Phase 3: Walk Operators R_A, R_B, and Compiled Walk Step

**Rationale:** R_A and R_B compose from the verified D_x. The parity-controlled diffusion pattern — allocate parity qubit, XOR height qubits for target parity, apply D_x in controlled context, uncompute parity qubit — is the key architectural insight from the Qrisp reference. The walk step U = R_B * R_A is then wrapped in `@ql.compile` for caching and controlled variant generation required for phase estimation.
**Delivers:** `emit_R_A()` and `emit_R_B()` using parity-controlled diffusion; composed walk step U = R_B * R_A; `@ql.compile`-wrapped walk operator with controlled variant accessible for phase estimation; qubit disjointness test confirming R_A and R_B have zero qubit-index overlap; circuit depth and gate count profile of a single walk step
**Implements:** R_A/R_B operators (ARCHITECTURE.md component 3); walk step U (FEATURES.md table stakes)
**Avoids:** Pitfall 4 (disjointness verified here via qubit index enumeration), Pitfall 12 (circuit depth — profiled before detection is layered on to avoid surprises)

### Phase 4: Detection Algorithm and Public API

**Rationale:** Detection (Montanaro Algorithm 1) builds on a verified walk step. This phase implements the iterative power-method approach — applying walk step powers and measuring — rather than full QPE (which requires inverse QFT and precision qubits that stress the 17-qubit ceiling). The public `ql.quantum_walk()` API is finalized here, with unambiguous naming distinguishing boolean detection from solution-path finding.
**Delivers:** Detection algorithm (iterative, 2-3 precision bits, threshold > 3/8); `ql.quantum_walk()` and `ql.detect_solution()` public API; `WalkResult` type with `.has_solution` boolean; `__init__.py` exports; API documentation clearly distinguishing detection (exists?) from solution finding (which path?); comparison of iterative power method against classical brute-force on a known-solution instance
**Uses:** Compiled walk step controlled variant; sim_backend.simulate(); to_openqasm() export pipeline
**Avoids:** Pitfall 7 (detection vs. finding confusion — API naming enforced), Pitfall 8 (phase estimation precision — empirically tuned here against known instances)

### Phase 5: End-to-End Verification and Demo

**Rationale:** All components tested in isolation need an integration test on a real CSP instance. A binary tree of depth 3 (8 leaves, 15 total nodes) with a 2-variable SAT predicate targets 10-11 qubits total, comfortably within the 17-qubit ceiling. Qiskit statevector verification provides the confidence signal that the walk is mathematically correct end-to-end. Circuit depth and gate count analysis at this scale informs the priority and approach for future subspace optimization work.
**Delivers:** End-to-end demo (2-variable SAT, binary tree depth 3); Qiskit statevector verification confirming detection probability; test suite covering tree encoding, D_x amplitudes, R_A/R_B disjointness, walk convergence, detection accuracy on known-solution instances; circuit depth and gate count profile for the demo instance; documented qubit budget for depth 4 (to inform v6.x scope)
**Addresses:** Demo + Qiskit verification (FEATURES.md table stakes); full integration of all prior phases

### Phase Ordering Rationale

- Phases 1 through 5 follow strict dependency order: tree encoding -> diffusion -> walk operators -> detection -> integration
- Phase 1 architectural decisions (register layout, predicate interface, uncomputation strategy) are non-negotiable; five of twelve pitfalls require them to be settled before any gate emission code is written
- Variable branching (v6.x) is explicitly excluded from v6.0 scope: it requires counting valid children per node (evaluating reject on all d potential children), a popcount circuit, and controlled-Ry conditioned on the count register — a separate, very-high-complexity feature correctly deferred
- Solution finding (Algorithm 2) deferred similarly: it is a classical outer loop calling detection n times; correctly built after detection is independently verified
- No bug fixes required before v6.0 starts, unlike v5.0 — the existing infrastructure is sound for this addition

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 2 (Local Diffusion — root amplitude):** The root node amplitude formula phi_root involves a tuning parameter N whose exact value differs between Algorithm 1 (detection) and Algorithm 2 (finding). PITFALLS.md rates confidence MEDIUM on this specific formula. Validate the exact phi_root formula against Montanaro section 2 / Algorithm 1 and cross-reference against Qrisp `backtracking_tree.py` source before writing any gate emission code.
- **Phase 4 (Detection — iterative vs QPE approach):** STACK.md, FEATURES.md, and ARCHITECTURE.md reached slightly different conclusions on the detection approach. STACK.md recommends adapting IQAE; FEATURES.md warns against it (different eigenvalue structure); ARCHITECTURE.md recommends the iterative power method. The iterative power method is the lower-risk path but loses theoretical precision guarantees. Resolve during Phase 4 planning with an explicit decision record.

Phases with standard patterns (skip additional research):
- **Phase 1 (Tree Encoding):** One-hot height + QuantumArray branch encoding follows Qrisp's validated approach. Register allocation uses existing qint/qarray/`_allocate_qubit()` patterns. No new mechanisms.
- **Phase 3 (Walk Operators):** Parity-controlled diffusion via `with qbool:` is a direct application of the existing controlled context mechanism. `@ql.compile` wrapping follows established patterns from grover.py and amplitude_estimation.py.
- **Phase 5 (Verification):** Qiskit verification pipeline is identical to existing Grover and amplitude estimation verification. No new infrastructure needed.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Codebase directly inspected: all required gates confirmed in gate.h, all emit_* functions confirmed in _gates.pyx. Zero new dependencies verified. |
| Features | MEDIUM | Algorithm well-established in literature; MVP feature set is clear and conservative. Variable branching deferred correctly given very high complexity. Anti-feature analysis (especially IQAE misuse for detection) is a valuable and actionable finding. |
| Architecture | HIGH | Codebase-verified integration points; Qrisp reference implementation studied in detail; explicit data flow validated against existing module patterns; proposed 3-file split mirrors grover/oracle/diffusion split exactly. |
| Pitfalls | HIGH | 7 critical + 5 moderate pitfalls identified; each has specific prevention, phase-to-address mapping, recovery cost, and warning signs. Integration pitfalls grounded in direct codebase analysis. Algorithmic pitfalls cross-referenced with Qrisp implementation and Martiel 2019. |

**Overall confidence:** HIGH

### Gaps to Address

- **Root diffusion amplitude formula (phi_root):** Montanaro's exact root weighting coefficient depends on a parameter N whose relationship to tree size and detection mode is described in section 2 but was not fully confirmed due to PDF parsing limitations during research. PITFALLS.md rates this MEDIUM confidence. Resolve in Phase 2 planning by re-reading Montanaro's Definition 1 and Theorem 1 and comparing to Qrisp's `psi_prep` function in `backtracking_tree.py`.
- **Detection approach (iterative power method vs. IQAE adaptation):** Three research files give slightly different recommendations. The iterative power method (varying walk step count, measuring, using majority vote) is safer to implement because it avoids eigenvalue analysis complications, but it does not provide formal precision guarantees. Resolve with an explicit decision during Phase 4 planning before any detection code is written.
- **Predicate ancilla qubit count:** Total qubit count estimates assume 2-4 predicate ancillae, but the actual count depends on the predicate's arithmetic complexity. Resolve during Phase 5 demo design by choosing the specific predicate first, then running the resource estimator (built in Phase 1) to verify budget before committing to the demo instance.

## Sources

### Primary (HIGH confidence)
- [Montanaro 2015 — Quantum walk speedup of backtracking algorithms (arXiv:1509.02374)](https://arxiv.org/abs/1509.02374) — foundational algorithm: D_x definition, R_A/R_B, Algorithm 1 (detection), Algorithm 2 (finding), spectral gap theorem
- [Theory of Computing published version (v014a015)](https://theoryofcomputing.org/articles/v014a015/) — peer-reviewed version of Montanaro
- [Qrisp QuantumBacktrackingTree documentation](https://qrisp.eu/reference/Algorithms/QuantumBacktrackingTree.html) — reference implementation API, one-hot encoding, quantum_step, accept/reject constraints
- [Qrisp backtracking_tree.py source](https://github.com/eclipse-qrisp/Qrisp/blob/main/src/qrisp/algorithms/quantum_backtracking/backtracking_tree.py) — qstep_diffuser pattern, phi/phi_root rotation angles, parity control implementation
- Quantum Assembly codebase (direct read): gate.h, _gates.pyx, grover.py, oracle.py, diffusion.py, amplitude_estimation.py, _core.pyx — all integration points verified from source code

### Secondary (MEDIUM confidence)
- [Martiel & Remaud 2019 — Practical implementation of a quantum backtracking algorithm (arXiv:1908.11291)](https://arxiv.org/abs/1908.11291) — O(n log d) qubit implementation, circuit depth analysis, encoding choices
- [Seidel et al. 2024 — Quantum Backtracking in Qrisp Applied to Sudoku Problems (arXiv:2402.10060)](https://arxiv.org/abs/2402.10060) — phi_root formula, circuit depth benchmarks (3968 depth for 4x4 Sudoku), 6n+14 CX per controlled diffuser
- [Quantum Search on Computation Trees (arXiv:2505.22405)](https://arxiv.org/html/2505.22405) — generalized walk state with weights, D_x = I - 2|psi_x><psi_x| formulation, variable-time walk

### Tertiary (LOW confidence)
- [Jarret & Wan 2018 — Improved quantum backtracking via effective resistance (arXiv:1711.05295)](https://arxiv.org/abs/1711.05295) — improved spectral gap analysis via effective resistance; relevant to Phase 4 detection precision tuning but not needed for MVP

---
*Research completed: 2026-02-26*
*Ready for roadmap: yes*
