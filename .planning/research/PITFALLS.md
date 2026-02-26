# Pitfalls Research: v6.0 Quantum Walk Primitives (Montanaro 2015 Backtracking)

**Domain:** Adding quantum walk primitives for Montanaro's 2015 backtracking speedup to an existing quantum programming framework with Grover's search, amplitude estimation, automatic uncomputation, and dual arithmetic backends
**Researched:** 2026-02-26
**Confidence:** HIGH for integration pitfalls (codebase inspected, existing bug patterns analyzed), MEDIUM for algorithmic pitfalls (literature-verified but Montanaro paper PDF not fully parseable -- details cross-referenced with Qrisp implementation and Martiel et al. 2019)

## Executive Summary

The v6.0 milestone adds quantum walk operators for Montanaro's backtracking speedup to a framework that already has Grover's search, `@ql.compile` capture-replay, automatic LIFO uncomputation, and 17-qubit simulation limits. The quantum walk algorithm differs fundamentally from Grover's search in three ways that create integration hazards: (1) the local diffusion operator R_x has node-dependent amplitude coefficients that vary with d(x) (number of valid children), unlike Grover's uniform diffusion; (2) the predicate must be evaluated in superposition and fully uncomputed within the walk operator, not once at circuit build time; and (3) the walk requires position+coin registers plus predicate ancillae, creating qubit pressure that collides with the 17-qubit simulator ceiling.

The most dangerous pitfalls are those that produce circuits that appear structurally correct but have subtly wrong amplitudes in the local diffusion operator -- these break the O(sqrt(T)) speedup guarantee without generating any error. The second class of danger involves the interaction between the walk operator's internal uncomputation requirements and the framework's existing LIFO scope-based uncomputation, which tracks layer ranges that become unreliable when the optimizer parallelizes gates (a known limitation documented in PROJECT.md).

The pitfalls below are ordered by severity: the first seven are critical (silent wrong results, broken speedup guarantees, or require architecture rework), the next five are moderate (specific failure modes or performance traps), and the remainder are integration-specific gotchas.

---

## Critical Pitfalls

### Pitfall 1: Wrong Amplitude Coefficients in Local Diffusion Operator R_x

**What goes wrong:**
The local diffusion operator D_x reflects in the subspace spanned by |psi_x> = (1/sqrt(d(x)+1))|x> + sum_{y child of x} (1/sqrt(d(x)+1))|y>, where d(x) is the number of valid children of node x. The coefficient 1/sqrt(d(x)+1) must include the parent state |x> in the count (hence "+1"), not just the children. Getting this wrong -- using 1/sqrt(d(x)) or 1/sqrt(n) where n is the maximum branching factor -- produces a diffusion operator that does not satisfy the spectral gap requirements of Montanaro's Theorem 1. The walk still runs, produces results, but has degraded or zero speedup.

**Why it happens:**
The Grover diffusion operator in the existing codebase (`diffusion.py`) uses uniform amplitudes -- all states get equal weight in the reflection. Developers naturally try to reuse this pattern for the local diffusion, using uniform 1/sqrt(d) or 1/sqrt(n) coefficients. But Montanaro's diffusion is state-dependent: the parent node x is included in the superposition being reflected over, and the count d(x) can vary per node. The Qrisp implementation uses controlled rotations with angle phi = 2*arctan(sqrt(d)) for non-root nodes and phi_root = 2*arctan(sqrt(N*d)) for the root, where N is a tuning parameter related to the tree size.

**How to avoid:**
1. Implement the amplitude preparation as a controlled Ry rotation: Ry(2*arcsin(1/sqrt(d(x)+1))) on the coin qubit, controlled on the node position, to create the correct superposition between parent and children states. This is the Qrisp approach and matches Montanaro's Definition 1.
2. For the root node, use a different coefficient: the root amplitude is sqrt(n)/sqrt(T_hat) where T_hat is the (unknown) effective tree size, which gets tuned by the outer phase estimation. Implement this as a separate rotation applied only at the root.
3. Write an explicit unit test that prepares |psi_x> for a known tree, measures the resulting amplitudes via statevector simulation, and compares against the analytically computed values. This catches coefficient errors before they propagate into the walk operator.
4. Never hardcode amplitudes for a maximum branching factor. The rotation angle must be computed from d(x), not from max_d.

**Warning signs:**
- Walk operator applied to a known 2-level tree with one marked leaf does not converge in O(sqrt(T)) steps
- Statevector of |psi_x> after amplitude preparation does not match 1/sqrt(d(x)+1) within floating-point tolerance
- Phase estimation returns eigenvalue 0 (no spectral gap) when there are marked nodes

**Phase to address:**
Phase 1 (Local Diffusion Primitives) -- the amplitude calculation must be correct from the first implementation. Incorrect amplitudes cannot be patched later without rewriting all walk operator tests.

**Confidence:** HIGH -- the Qrisp implementation (arXiv:2402.10060) explicitly documents using phi = 2*arctan(sqrt(d)) for the rotation angle, and Montanaro's paper defines the state |psi_x> with the 1/sqrt(d(x)+1) coefficient.

---

### Pitfall 2: Variable Branching Factor Requires Dynamic Controlled Rotations

**What goes wrong:**
In real backtracking trees (SAT, graph coloring), the number of valid children d(x) varies per node. A node at depth 3 in a 3-coloring problem might have 2 valid children (2 colors not yet used by neighbors) while another node at the same depth has 0. The local diffusion must apply Ry(2*arcsin(1/sqrt(d(x)+1))) where d(x) is computed in superposition from the predicate P. If d(x) is approximated as a constant (e.g., always using max_d), the diffusion operator has wrong amplitudes for most nodes, and the walk does not implement the correct Markov chain.

**Why it happens:**
Computing d(x) requires evaluating the predicate P on each potential child of x and counting how many are valid. This count must be stored in a quantum register, then used to control a rotation angle. The existing framework has no "data-controlled rotation" primitive -- `emit_ry` takes a classical angle parameter. Implementing a rotation whose angle depends on a quantum register requires either: (a) a lookup table of controlled rotations (for each possible value of d, apply the corresponding rotation controlled on d==value), or (b) a quantum arithmetic circuit that computes arcsin(1/sqrt(d+1)) from d and feeds it to a quantum-controlled rotation.

**How to avoid:**
1. Use the lookup-table approach: allocate a register for d(x), count valid children into it, then for each possible value k in [0, max_d], apply Ry(2*arcsin(1/sqrt(k+1))) controlled on (d == k). This is O(max_d) controlled rotations per diffusion step, which is acceptable for small branching factors (max_d <= 8).
2. For the special case of binary trees (d(x) in {0, 1, 2}), use only 3 controlled rotations. This covers SAT with 2-valued variables, which is the primary demo target.
3. The child-counting register must be fully uncomputed after the controlled rotation, before moving to the next step. Use the existing `@ql.compile` inverse capability: compile the child-counting function, apply it, do the rotation, then apply `.inverse()` to uncompute.
4. Never hard-wire d(x) as a constant unless the tree is provably regular (all nodes at the same depth have the same branching factor). Document this constraint explicitly in the API.

**Warning signs:**
- Walk operator gives correct results on uniform-branching test cases but fails on non-uniform ones
- The d(x) register is not returned to |0> after the diffusion step (measure ancilla qubits)
- Circuit qubit count grows unexpectedly (ancilla leak from uncomputed child-count register)

**Phase to address:**
Phase 2 (Variable Branching Support) -- this should be a separate phase after basic fixed-branching walk operators are verified. Attempting variable branching before the fixed case is solid will confuse debugging.

**Confidence:** HIGH -- the Qrisp paper explicitly documents this pattern and provides the controlled rotation angles.

---

### Pitfall 3: Predicate Evaluation Must Be Fully Reversible and Uncomputable Within Walk Step

**What goes wrong:**
The walk operator applies the predicate P(v) to determine whether a node is accepted, rejected, or undecided. Unlike Grover's oracle (which is called once per iteration as a complete compute-phase-uncompute block), the walk operator's predicate is called INSIDE the local diffusion as part of determining d(x) and node status. The predicate's temporary computations must be fully uncomputed before the diffusion reflection, or the entanglement between predicate ancillae and the walk state will destroy interference.

**Why it happens:**
In the existing Grover implementation (`oracle.py`), the oracle is a self-contained compute-phase-uncompute block validated by `_validate_compute_phase_uncompute()`. The walk operator has a different structure: P(v) is called to classify nodes (accept/reject/continue), the classification is used to select the diffusion operation (identity for accepted, minus-identity for rejected, reflection for continue), and then P(v) must be uncomputed. But P(v) might use arithmetic operations (+, -, ==, >=) that allocate qint/qbool temporaries tracked by the framework's scope-based uncomputation. If the predicate is called inside a walk-operator scope, the framework's automatic uncomputation may fire at the wrong time.

**How to avoid:**
1. Compile the predicate with `@ql.compile` and use the `.inverse()` method for uncomputation. This gives explicit control over when the predicate is uncomputed, bypassing the scope-based automatic uncomputation system.
2. The predicate function must return its result (accept/reject/continue) in a dedicated output qubit, not as a qbool that enters the scope stack. Use a raw qubit allocation (`_allocate_qubit()`) for the predicate result, not a qint/qbool constructor.
3. Validate the predicate's ancilla delta BEFORE integrating it into the walk operator. Reuse the `_validate_ancilla_delta()` pattern from `oracle.py` but applied to the predicate function separately.
4. The predicate must NOT use `with` blocks (controlled contexts) internally, because the walk operator itself may need to run in a controlled context for phase estimation. Nested controlled contexts would require the predicate to understand multi-level control, which the current framework handles (via `_get_list_of_controls`) but which has not been tested in walk-operator depth.

**Warning signs:**
- `circuit_stats()['current_in_use']` grows after each walk step (predicate ancillae not uncomputed)
- Walk operator produces different results when `qubit_saving_mode` is toggled (uncomputation timing changes)
- Predicate works correctly as a standalone Grover oracle but fails inside the walk operator

**Phase to address:**
Phase 1 (Walk Operator Core) -- predicate integration must be designed from the start, not bolted on later. The walk operator's internal structure depends on when and how the predicate is evaluated and uncomputed.

**Confidence:** HIGH -- this is the core architectural difference between Grover's oracle pattern and Montanaro's walk operator. The Qrisp framework handles this with `@auto_uncompute` decorator and explicit temporary variable management; the Quantum Assembly framework needs an equivalent mechanism.

---

### Pitfall 4: R_A and R_B Must Act on Disjoint Qubit Partitions -- Parity Violation Breaks Walk

**What goes wrong:**
Montanaro's walk operator is R_B * R_A, where R_A = product of D_x over x at even depth, R_B = product of D_x over x at odd depth. The critical requirement: R_A and R_B must act on disjoint sets of qubits so that the local diffusions within each operator commute and can be applied in parallel. If the node encoding causes R_A and R_B to overlap on shared qubits (e.g., a position register that is read by both), the walk operator does not implement the correct product of reflections.

**Why it happens:**
In a height-encoded tree (as used by Qrisp), the even/odd partition is determined by the height register. Each local diffusion D_x operates on the coin register for height h and the position register entries at depth h. If the position register uses a shared encoding (e.g., a single integer register for the full path), then D_x at depth h and D_y at depth h+1 both read/write the same position register, and R_A and R_B are NOT disjoint. The solution is to use a QuantumArray (one register per tree level) so that each D_x touches only its level's position register and the coin register for that level.

**How to avoid:**
1. Encode the tree path as a QuantumArray with one entry per tree depth level (Qrisp's `branch_qa` approach). This ensures D_x at depth h only touches `branch_qa[h]` and the coin/height registers for depth h.
2. Use one-hot encoding for the height register (n+1 qubits for depth-n tree). One-hot encoding enables efficient controlled operations (each diffusion is controlled on a single height qubit) and makes the even/odd partition trivial.
3. Verify disjointness with a test: apply R_A, then R_B, and confirm via statevector that the result equals applying all local diffusions simultaneously. If R_A and R_B commute (because they act on disjoint qubit sets), the order should not matter and the combined product should match independent application.
4. Never use a single position register that encodes the entire tree path as one integer. This seems qubit-efficient but makes disjoint partitioning impossible.

**Warning signs:**
- Applying R_A then R_B gives different results than R_B then R_A (should be identical up to global phase if disjoint)
- Local diffusions within R_A interfere with each other (they should commute)
- Walk operator does not converge even for trivial trees

**Phase to address:**
Phase 1 (Architecture Design) -- the qubit register layout must be designed for disjoint R_A/R_B from the start. Retrofitting a different encoding is a rewrite.

**Confidence:** MEDIUM -- verified from Qrisp's architecture (which uses QuantumArray branch_qa + one-hot height), but the disjointness requirement is implicit in Montanaro's paper and easy to miss.

---

### Pitfall 5: Qubit Budget Explosion Under 17-Qubit Simulator Ceiling

**What goes wrong:**
The walk operator requires: (1) height register: n+1 qubits (one-hot) or ceil(log(n+1)) (binary); (2) path register: n * ceil(log(max_d)) qubits (one entry per depth level, each encoding a branch choice); (3) coin register: 1-2 qubits per depth level for amplitude preparation; (4) predicate ancillae: depends on the predicate complexity (comparisons, arithmetic); (5) child-count register: ceil(log(max_d+1)) qubits for variable branching. For a SAT instance with n=4 variables and max_d=2 (binary branching): height=5 (one-hot) + path=4*1=4 + coin=4 + predicate ancillae >= 2 = minimum 15 qubits BEFORE any arithmetic in the predicate. This is already at the 17-qubit simulator limit.

**Why it happens:**
Each component of the walk operator demands its own qubit register. Unlike Grover's search (where the search register IS the computation space), the walk operator has auxiliary structure (height, coin, path) on top of the problem encoding. The 17-qubit Qiskit simulation limit (documented in MEMORY.md) makes it nearly impossible to verify non-trivial instances.

**How to avoid:**
1. Design the minimal demo instance first: a binary tree of depth 2-3 with a trivial predicate (constant accept/reject). Calculate exact qubit count before implementation.
2. Use binary encoding for the height register (ceil(log(n+1)) qubits) instead of one-hot (n+1 qubits) when qubit-constrained. One-hot is more gate-efficient but qubit-expensive.
3. Use matrix_product_state (MPS) simulator for verification of circuits exceeding 17 qubits, as already done for division tests (documented in Key Decisions). MPS can handle 40+ qubits for low-entanglement circuits.
4. Implement a resource estimator that calculates total qubits needed for a given tree before circuit construction. Fail fast with a clear error if the qubit budget exceeds the simulation target.
5. The predicate should reuse existing framework primitives (qint comparisons) but its ancilla overhead must be tracked and reported separately.

**Warning signs:**
- Simulation hangs or runs out of memory (exceeding 17-qubit statevector limit)
- Test instances seem trivially small but still exceed qubit budgets
- Cannot verify any non-trivial SAT instance end-to-end

**Phase to address:**
Phase 1 (Design) -- qubit budget must be calculated during architecture design, not discovered during testing. The demo instance choice must be qubit-budget-aware.

**Confidence:** HIGH -- the 17-qubit limit is documented in MEMORY.md, and the qubit counts above are computed from the algorithm structure.

---

### Pitfall 6: Interaction Between Walk Operator Uncomputation and Framework LIFO Scope Uncomputation

**What goes wrong:**
The walk operator internally creates and uncomputes quantum state (predicate evaluation, child counting, amplitude preparation). The framework's automatic uncomputation system uses LIFO (last-in-first-out) scope-based tracking tied to layer ranges. If the walk operator creates qint/qbool variables inside a Python scope, the framework may attempt to uncompute them at scope exit -- but the walk operator has already uncomputed them as part of its algorithm. This double-uncomputation corrupts the quantum state.

**Why it happens:**
The framework's uncomputation system (documented in KEY DECISIONS: "LIFO cascade uncomputation", "Layer tracking on all qint operations", "Strict < for LAZY scope comparison") tracks every qint/qbool creation and generates reverse gates at scope exit. The walk operator needs fine-grained control: compute predicate, use result, uncompute predicate, apply diffusion, etc. This interleaved compute-use-uncompute pattern within a single scope does not match the framework's all-at-once uncomputation at scope exit.

The known limitation "Layer-based uncomputation tracking unreliable when optimizer parallelizes gates" (PROJECT.md) makes this worse: if the walk operator's internal uncomputation gates are reordered by the optimizer, the layer ranges used for reverse_instruction_range become invalid.

**How to avoid:**
1. Implement the entire walk operator at a level below the automatic uncomputation system. Use `_allocate_qubit()` and `_deallocate_qubits()` directly (as `_wrap_bitflip_oracle` does in oracle.py) rather than qint/qbool constructors that enter the scope stack.
2. Alternatively, compile the walk operator with `@ql.compile` which captures the complete gate sequence and manages its own ancilla tracking (the v2.1 ancilla tracking system). The compiled function's `.inverse()` handles uncomputation independently of the scope system.
3. Set `qubit_saving_mode = False` (lazy uncomputation) during walk operator execution to prevent eager uncomputation from interfering with the walk's internal state management.
4. Test with both qubit_saving modes to verify the walk operator is self-contained and does not depend on the framework's uncomputation timing.

**Warning signs:**
- Walk operator produces different results depending on `qubit_saving_mode`
- "Oracle ancilla delta is N (must be 0)" errors from validation
- Walk operator works in isolation but fails when called inside a `with` block or from within a compiled function

**Phase to address:**
Phase 1 (Walk Operator Core) -- the decision about how the walk operator interacts with the uncomputation system must be made at the architecture level, before any gate emission code is written.

**Confidence:** HIGH -- this pitfall is a direct extrapolation from the PITFALLS_GROVER.md analysis of oracle-uncomputation interaction, which was confirmed as a real issue (Pitfall 2 in that document).

---

### Pitfall 7: Detection Mode vs Solution-Finding Mode Confusion

**What goes wrong:**
Montanaro's Algorithm 1 is a DETECTION algorithm: it determines whether a marked node exists in the tree, but does not return which node is marked. Users expect a "find the solution" API (like `ql.grover()` which returns the measured value). Converting detection to solution-finding requires a classical outer loop that traverses the tree top-down, at each node using the quantum detection algorithm to determine which subtree contains a solution. Implementing only detection and calling it "quantum walk search" will confuse users who expect solution output.

**Why it happens:**
The quantum walk detects the existence of a marked state by performing phase estimation on R_B * R_A and checking whether the resulting eigenvalue is close to 1 (no marked node) or separated from 1 (marked node exists). Extracting the actual solution requires Montanaro's Algorithm 2, which classically selects child nodes and recursively applies Algorithm 1 on subtrees. This hybrid classical-quantum loop adds n calls to the detection algorithm (one per tree depth level), increasing total complexity to O(sqrt(T) * n^(3/2) * log(n)) but still providing speedup.

**How to avoid:**
1. Implement detection mode first (Algorithm 1) as the fundamental primitive.
2. Implement solution-finding (Algorithm 2) as a separate higher-level API that calls detection repeatedly.
3. Make the API naming unambiguous: `ql.quantum_walk_detect(predicate, tree)` returns bool, `ql.quantum_walk_find(predicate, tree)` returns the solution path. Do not name anything "quantum_walk_search" unless it actually returns a solution.
4. Document the complexity difference: detection is O(sqrt(T)), solution-finding is O(sqrt(T) * n^(3/2) * log(n)).

**Warning signs:**
- Users call the detection API and get confused by a boolean result instead of a solution
- Phase estimation returns an eigenvalue but code does not threshold it correctly for detection
- Solution-finding mode does not reduce to detection mode as a building block

**Phase to address:**
Phase 1 (API Design) -- the distinction between detection and solution-finding should be in the API specification before implementation begins.

**Confidence:** HIGH -- Montanaro's paper explicitly separates Algorithm 1 (detection) from Algorithm 2 (finding), and the Qrisp documentation notes that "Montanaro's algorithm only determines solution existence."

---

## Moderate Pitfalls

### Pitfall 8: Phase Estimation Precision Determines Detection Reliability

**What goes wrong:**
The detection algorithm uses phase estimation on R_B * R_A to distinguish eigenvalue 1 (no solution) from eigenvalue cos^2(theta) (solution exists). The precision of phase estimation determines how small a spectral gap can be detected. If the phase estimation uses too few precision qubits, it cannot distinguish "solution exists with small spectral gap" from "no solution." This produces false negatives (says no solution when one exists).

**Why it happens:**
The spectral gap depends on the tree structure, the branching factor, and the effective resistance of the graph. For unbalanced trees or trees with many rejected branches, the spectral gap can be very small, requiring more precision qubits. The existing `ql.amplitude_estimate()` uses IQAE (Iterative Quantum Amplitude Estimation) which avoids QFT-based phase estimation, but Montanaro's algorithm specifically requires phase estimation on the walk operator, which is a different use case.

**How to avoid:**
1. Implement phase estimation using the framework's existing Ry rotation and controlled-operation capabilities. The walk operator must support controlled application (controlled-R_B * controlled-R_A) for phase estimation to work.
2. Start with a generous number of precision qubits (e.g., ceil(log2(sqrt(T))) + 5) and optimize downward based on empirical testing.
3. The threshold for detection should be conservative: eigenvalue < cos(pi/(2*sqrt(T))) indicates a marked node exists. Implement this threshold as a configurable parameter.
4. Add the precision qubit count to the total qubit budget calculation (Pitfall 5). Phase estimation qubits are often forgotten in initial estimates.

**Warning signs:**
- Detection returns "no solution" on instances known to have solutions
- Phase estimation eigenvalue is always exactly 0 or 1 (precision too low to resolve intermediate values)
- Adding more precision qubits changes the detection result

**Phase to address:**
Phase 3 (Detection Mode) -- after the walk operator is verified to be correct, phase estimation integration can be tuned.

**Confidence:** MEDIUM -- the precision requirements depend on the specific tree structure and are hard to predict analytically. Empirical tuning will be necessary.

---

### Pitfall 9: Node Encoding Choice (Height vs. Depth) Has Downstream Consequences

**What goes wrong:**
Montanaro's paper uses distance from the root (depth) as the primary node identifier. The Qrisp implementation uses height (distance from the leaves). These are complementary (height = max_depth - depth), but the encoding choice affects how the even/odd partition for R_A/R_B is computed, how the predicate evaluates node status, and how the tree traversal in solution-finding mode works. Mixing conventions within the implementation causes subtle off-by-one errors in the diffusion operators.

**Why it happens:**
The choice between height and depth encoding affects multiple components:
- Height encoding (Qrisp) puts the root at maximum height, leaves at height 0. The accept function checks height == 0 for full solutions.
- Depth encoding (Montanaro) puts the root at depth 0. The even/odd partition aligns with depth parity.
- One-hot encoding of height requires n+1 qubits where the root's qubit is `h[n]` and leaves are `h[0]`. Off-by-one in indexing produces valid-looking circuits with wrong diffusion targets.

**How to avoid:**
1. Choose ONE convention (recommend height, following Qrisp) and document it prominently.
2. Name all variables and functions with the chosen convention: `height_register` not `level_register`, `h[0]` is the leaf level.
3. Write a conversion utility `depth_to_height(d, max_depth)` for any interface that uses the other convention.
4. The even/odd parity for R_A/R_B should be computed from the chosen convention and tested explicitly: confirm that R_A applies to nodes at the correct levels.

**Warning signs:**
- Accept function returns True for root instead of leaves (or vice versa)
- R_A and R_B are applied to the wrong levels (odd instead of even)
- Walk converges on some trees but not others of the same depth with different parity

**Phase to address:**
Phase 1 (Architecture Design) -- convention must be established in the initial design document.

**Confidence:** MEDIUM -- the Qrisp paper (arXiv:2402.10060) explicitly discusses the height vs. depth choice and its impact. Within the Quantum Assembly framework, no convention yet exists.

---

### Pitfall 10: Accept and Reject Must Never Return True on the Same Node

**What goes wrong:**
The walk operator classifies each node into three categories: accepted (solution found), rejected (prune this subtree), or continue (explore children). If the accept and reject predicates are not mutually exclusive, a node can be simultaneously accepted and rejected. This puts the diffusion operator into an undefined state: it should apply identity (accepted), minus-identity (rejected), or reflection (continue), but cannot satisfy two conditions simultaneously.

**Why it happens:**
This constraint is easy to violate in practice. For example, in graph coloring, a partial assignment might be accepted (all variables assigned, valid coloring) AND rejected (some constraint violated). The predicates must be designed so that accept implies NOT reject and vice versa. In the Qrisp implementation, this is documented as a critical requirement: "accept and reject must never return True on the same node."

**How to avoid:**
1. Validate at API level: if both accept(x) and reject(x) return True for any classically enumerable state, raise an error before circuit construction.
2. Design the predicate interface as a single function returning a 3-valued result (accept/reject/continue) rather than two separate boolean functions. This makes mutual exclusion structural rather than a convention.
3. For SAT instances, the standard pattern is: accept = (all variables assigned AND all clauses satisfied), reject = (some clause is unsatisfied with all its variables assigned). These are naturally mutually exclusive because accept requires ALL clauses satisfied while reject requires SOME clause unsatisfied.
4. Add a classical pre-check that evaluates accept(x) and reject(x) on all nodes of a small test tree (depth 2-3) before launching quantum simulation.

**Warning signs:**
- Walk operator produces wrong results on specific instances but works on others
- Phase estimation returns unexpected eigenvalues (neither 1 nor the expected spectral gap)
- Debugging shows a node with both accept and reject flags set in statevector analysis

**Phase to address:**
Phase 1 (Predicate Interface Design) -- the predicate API must enforce mutual exclusion from the start.

**Confidence:** HIGH -- Qrisp documentation explicitly states this as a critical constraint, and it was the source of a bug fix in the Qrisp framework (reject function inconsistency on non-algorithmic states).

---

### Pitfall 11: Root Node Requires Special Treatment in the Diffusion Operator

**What goes wrong:**
The root node's diffusion amplitude is different from all other nodes. In Montanaro's formulation, the root uses a weighting coefficient that depends on n (a tuning parameter related to the effective tree size), not just d(root). The Qrisp implementation uses phi_root = 2*arctan(sqrt(N*d)) instead of the standard phi = 2*arctan(sqrt(d)). If the root is treated identically to other nodes, the initial state preparation is wrong, and the walk operator's spectral properties change.

**Why it happens:**
The root amplitude asymmetry comes from Montanaro's definition of the walk's initial state |r>. The walk starts from the root with amplitude proportional to sqrt(n)/sqrt(norm), while children have amplitude proportional to 1/sqrt(norm). Forgetting to implement this special case is easy because the root "looks like" any other node with children.

**How to avoid:**
1. Implement root diffusion as a separate code path from non-root diffusion. Control the selection with the height register: root is at h[max_depth] (one-hot) or h == max_depth (binary).
2. The root rotation angle must include the tree-size parameter n. This parameter is either known a priori or tuned by the outer detection algorithm.
3. Test the root diffusion independently: prepare the root state, apply D_root, measure amplitudes, and verify they match the formula.

**Warning signs:**
- Walk operator works for subtrees but not for the full tree starting from root
- Initial state has wrong amplitude distribution
- Detection algorithm converges too slowly or not at all (wrong spectral gap from root)

**Phase to address:**
Phase 1 (Local Diffusion Primitives) -- must be implemented together with the general diffusion, not deferred.

**Confidence:** MEDIUM -- the Qrisp paper documents the different root rotation angle. The exact formula depends on how Montanaro's parameter n is set, which varies between Algorithm 1 (detection) and Algorithm 2 (finding).

---

### Pitfall 12: Circuit Depth Explosion for Walk Operator Iterations

**What goes wrong:**
Each walk step (R_B * R_A) involves: (a) evaluate predicate on all nodes at even depths, (b) apply local diffusions at even depths, (c) uncompute predicates, (d) repeat for odd depths. Each predicate evaluation may involve arithmetic comparisons creating O(w) gates per comparison (where w is the qint width). For a tree of depth n with max branching d, each walk step has O(n * d * predicate_depth) gates. Phase estimation requires O(sqrt(T)) walk steps with controlled operation, and each controlled step doubles the control depth. Total circuit depth can easily reach millions of gates for modest instances.

**Why it happens:**
The existing framework can handle large circuits (QFT up to 2000 variables, benchmarks in `circuit-gen-results/`), but simulation time grows with depth. The 17-qubit limit means we are stuck with small-tree instances, but even these can produce deep circuits when the predicate involves arithmetic.

**How to avoid:**
1. Profile the circuit depth of a single walk step on the target demo instance before implementing the full detection algorithm. Use `circuit().depth` and `circuit().gate_count` to get exact numbers.
2. Optimize the predicate to minimize gate count. For SAT, the reject function should check only the newly assigned variable's constraints, not all constraints. This reduces per-step predicate cost from O(clauses) to O(clauses involving the current variable).
3. Use the Qrisp "subspace optimization" approach: if the predicate returns the same result on non-algorithmic states (states where branch_qa has values at unassigned positions), skip those evaluations. This can dramatically reduce gate count.
4. Consider whether the demo should use phase estimation at all, or just demonstrate the walk operator R_B * R_A applied for a fixed number of steps with measurement, similar to how `ql.grover()` applies a fixed number of iterations.

**Warning signs:**
- Circuit generation takes minutes for small instances
- Circuit depth exceeds 10,000 for a 3-variable SAT instance
- Qiskit simulation times out even with MPS backend

**Phase to address:**
Phase 2 (Demo Construction) -- circuit depth analysis should drive the choice of demo instance.

**Confidence:** HIGH -- circuit depth is a well-known issue with quantum walk implementations. The Qrisp paper reports 3,968 circuit depth for a 4x4 Sudoku with 9 empty fields using 91 qubits.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Hardcoding branching factor as constant | Simpler amplitude preparation | Cannot handle real CSP instances with variable branching | MVP/demo only, must be replaced before any real use |
| Using binary height encoding to save qubits | Saves n - ceil(log(n+1)) qubits | Controlled operations need multi-qubit equality checks instead of single-qubit controls, increasing gate count | When qubit budget is severely constrained (< 12 qubits available) |
| Skipping phase estimation, using fixed iterations | Avoids controlled-walk complexity and precision qubits | No detection guarantee, cannot determine if solution exists | Acceptable for walk operator verification phase only |
| Implementing predicate at Python level (not compiled) | Faster development iteration | Cannot use `.inverse()` for uncomputation, must manually reverse | Never for production; okay for initial prototyping |
| Using qubit_saving_mode=False globally | Avoids uncomputation timing conflicts | Wastes qubits, may exceed 17-qubit limit sooner | Acceptable during development if qubit budget allows |

## Integration Gotchas

Common mistakes when connecting quantum walk to existing framework subsystems.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| `@ql.compile` for walk operator | Compiling the entire walk operator as a single function -- cache key does not capture tree structure | Compile the predicate and local diffusion separately; compose the walk operator from compiled primitives |
| `ql.option('fault_tolerant')` | Forgetting that walk operator Ry rotations are NOT Toffoli-compatible -- fault_tolerant mode decomposes CCX but not Ry | Either (a) use QFT mode for walk operators, or (b) decompose Ry rotations into Clifford+T separately with `ql.option('toffoli_decompose')` |
| `ql.amplitude_estimate()` / IQAE | Trying to use IQAE for walk detection -- IQAE estimates amplitude of a marked state, not eigenvalue of a unitary | Walk detection needs phase estimation on R_B * R_A, which is a different algorithm from IQAE. Cannot reuse the existing amplitude estimation API directly. |
| `ql.grover_oracle` decorator | Wrapping the walk predicate with `@ql.grover_oracle` -- oracle validation checks for compute-phase-uncompute pattern which the walk predicate does not follow | Use `@ql.compile` only for the predicate; the walk operator handles the compute-use-uncompute pattern internally |
| Diffusion operator reuse | Reusing `diffusion.py`'s X-MCZ-X pattern for local diffusion D_x | The walk's local diffusion is a reflection over |psi_x> (a specific superposition), NOT over |0...0>. New diffusion implementation needed. |
| Phase proxy (`x.phase += theta`) | Using `x.phase += pi` inside the walk operator for phase marking | The walk operator uses diffusion-based marking (D_x = identity on accepted, -identity on rejected), not phase kickback. Different mechanism. |

## Performance Traps

Patterns that work at small scale but fail as instance size grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| O(max_d) controlled rotations per diffusion step | Walk operator works for binary trees but is too slow for higher-arity trees | Use QROM (quantum read-only memory) for the rotation lookup table for max_d > 4 | max_d > 8 |
| Predicate evaluates ALL constraints at every node | Gate count per walk step is O(n * constraints) | Incremental predicate: only check constraints involving the newly assigned variable | Trees deeper than 4-5 levels |
| Full statevector simulation for verification | Works for < 17 qubits | MPS backend for verification; additionally, use circuit_stats() for structural validation without simulation | > 17 qubits |
| Unoptimized controlled walk step for phase estimation | Controlled-R_B * controlled-R_A doubles gate count per control qubit | Implement efficient controlled walk using ancilla decomposition of multi-controlled gates (existing MCX infrastructure) | Phase estimation with > 3 precision qubits |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Walk operator R_B * R_A:** Often missing correct root treatment -- verify root diffusion uses different amplitude from all other nodes
- [ ] **Local diffusion D_x:** Often missing the parent state in the superposition -- verify |psi_x> includes |x> (parent) with weight 1/sqrt(d(x)+1), not just children
- [ ] **Predicate integration:** Often missing uncomputation of predicate ancillae -- verify `circuit_stats()['current_in_use']` returns to pre-step value after each walk step
- [ ] **Variable branching:** Often hardcodes max_d -- verify amplitude is correct for nodes with fewer than max_d valid children (d(x) < max_d)
- [ ] **Detection mode:** Often missing phase estimation threshold -- verify detection correctly maps eigenvalue to boolean (solution exists / does not exist)
- [ ] **Disjoint partition:** Often uses shared position register -- verify R_A qubits and R_B qubits have zero overlap via qubit index enumeration
- [ ] **Controlled walk step:** Often forgets that phase estimation needs controlled-U^k -- verify walk operator supports controlled application and power k
- [ ] **Accept/reject mutual exclusion:** Often missing runtime validation -- verify no node has both accept=True and reject=True in classical pre-check

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Wrong amplitude coefficients (Pitfall 1) | MEDIUM | Fix the rotation angle formula, re-run all walk operator tests. No architectural change needed. |
| Variable branching not implemented (Pitfall 2) | HIGH | Requires adding child-count register, controlled rotation lookup, and uncomputation. Touches walk operator internals. |
| Predicate ancilla leak (Pitfall 3) | HIGH | May require redesigning predicate interface to use raw qubit allocation instead of qint/qbool. Touches framework boundaries. |
| R_A/R_B overlap (Pitfall 4) | VERY HIGH | Requires changing the tree encoding (register layout). All walk operator code must be rewritten. |
| Qubit budget exceeded (Pitfall 5) | MEDIUM | Simplify demo instance, switch to MPS backend, or switch height encoding from one-hot to binary. |
| Uncomputation conflict (Pitfall 6) | HIGH | Requires moving walk operator below automatic uncomputation system. Significant refactoring. |
| Detection vs. finding confusion (Pitfall 7) | LOW | API renaming and documentation fix. Code structure unchanged if detection was implemented correctly. |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Pitfall 1 (Wrong amplitudes) | Phase 1: Local Diffusion Primitives | Statevector test: prepare |psi_x>, verify amplitudes match 1/sqrt(d(x)+1) |
| Pitfall 2 (Variable branching) | Phase 2: Variable Branching | Test with non-uniform tree: d(x)=2 for some nodes, d(x)=1 for others |
| Pitfall 3 (Predicate uncomputation) | Phase 1: Walk Operator Core | `circuit_stats()['current_in_use']` unchanged after walk step |
| Pitfall 4 (R_A/R_B overlap) | Phase 1: Architecture Design | Qubit index enumeration test: R_A qubits and R_B qubits are disjoint sets |
| Pitfall 5 (Qubit budget) | Phase 1: Architecture Design | Resource estimator outputs qubit count before circuit construction |
| Pitfall 6 (Uncomputation conflict) | Phase 1: Walk Operator Core | Walk operator produces same result with qubit_saving=True and False |
| Pitfall 7 (Detection vs. finding) | Phase 1: API Design | API names clearly distinguish detection from finding |
| Pitfall 8 (Phase estimation precision) | Phase 3: Detection Mode | Detection returns correct result for known-solution test tree |
| Pitfall 9 (Encoding convention) | Phase 1: Architecture Design | All code uses consistent height/depth convention |
| Pitfall 10 (Accept/reject exclusion) | Phase 1: Predicate Interface | Classical pre-check validates no overlap on test tree |
| Pitfall 11 (Root special case) | Phase 1: Local Diffusion | Root amplitude differs from non-root in statevector test |
| Pitfall 12 (Circuit depth) | Phase 2: Demo Construction | Circuit depth < 10,000 for target demo instance |

## Sources

- [Montanaro 2015, "Quantum walk speedup of backtracking algorithms"](https://arxiv.org/abs/1509.02374) -- original algorithm definition, Theorem 1, Algorithms 1 and 2
- [Martiel et al. 2019, "Practical implementation of a quantum backtracking algorithm"](https://arxiv.org/abs/1908.11291) -- practical O(n log d) qubit implementation, encoding choices
- [Qrisp QuantumBacktrackingTree documentation](https://qrisp.eu/reference/Algorithms/QuantumBacktrackingTree.html) -- implementation details, accept/reject constraints, subspace optimization
- [Qrisp Quantum Backtracking Sudoku paper (arXiv:2402.10060)](https://arxiv.org/html/2402.10060) -- rotation angles, circuit depth analysis, verification methodology
- [Ambainis & Kokainis 2017, "Improved quantum backtracking algorithms using effective resistance estimates"](https://arxiv.org/abs/1711.05295) -- improved spectral gap analysis, effective resistance tuning
- Quantum Assembly PROJECT.md -- known limitations (layer-based uncomputation, 17-qubit limit, QQ division ancilla leak)
- Quantum Assembly PITFALLS_GROVER.md -- oracle uncomputation pitfalls (directly relevant to predicate integration)
- Quantum Assembly oracle.py, diffusion.py, grover.py -- existing framework patterns for oracle and diffusion implementation
- Quantum Assembly KNOWN-ISSUES.md -- QQ division ancilla leak pattern (analogous to predicate ancilla leak risk)

---
*Pitfalls research for: v6.0 Quantum Walk Primitives (Montanaro 2015 Backtracking Speedup)*
*Researched: 2026-02-26*
