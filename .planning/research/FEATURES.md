# Feature Research: v6.0 Quantum Walk Primitives (Montanaro 2015 Backtracking)

**Domain:** Quantum programming framework -- quantum walk operators for backtracking tree search (CSP speedup)
**Researched:** 2026-02-26
**Confidence:** MEDIUM (algorithm is well-established in literature; circuit-level construction details require careful mapping to existing gate primitives; reference implementation exists in Qrisp but our architecture differs significantly)

## Feature Landscape

### Table Stakes (Users Expect These)

Features that users of a quantum walk backtracking framework built on Montanaro 2015 would expect. Without these, the module is incomplete or non-functional.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Predicate function P(v) interface** | Core of the algorithm -- user defines the backtracking tree via `accept(tree_state) -> qbool` and `reject(tree_state) -> qbool`. Without this, there is no tree to walk. Montanaro defines P: D -> {true, indeterminate, false} | MEDIUM | Map to two user-supplied callables returning qbool. `accept` returns True on satisfying nodes (P=true). `reject` returns True on branches to prune (P=false). Neither returning True means P=indeterminate (continue exploring). Must preserve tree state (no side effects on node registers). Existing `@ql.compile` and lambda tracing infrastructure (from oracle.py) provides the foundation |
| **Tree state encoding (height + path registers)** | The quantum walk operates on a superposition of tree nodes. Each node is identified by its height in the tree and the path of branch choices from root. Without proper encoding, no walk is possible | HIGH | Following Qrisp's proven encoding: `h` register (one-hot encoded height, max_depth qubits), `branch_array` (ql.array of branch variables storing reversed path from root). Node at depth d with path [b1, b2, ..., bd] is encoded as branch_array entries + height register. One-hot height enables efficient controlled operations per level. Total data qubits: O(n * log(d)) where n=max_depth, d=branching degree |
| **Local diffusion operator D_x** | The fundamental building block. For unmarked vertex x with children, D_x = I - 2\|psi_x><psi_x\| where \|psi_x> is a uniform superposition of x and its valid children weighted by 1/sqrt(d(x)+1). For marked vertices (accept=True), D_x = Identity | HIGH | This is the hardest part. The state \|psi_x> = (1/sqrt(d(x)+1)) * (\|x> + sum over children \|y>). When d(x) is constant (uniform branching), this is a fixed Ry rotation. When d(x) varies per node (variable branching), requires controlled rotations conditioned on computed d(x). Implementation: prepare amplitude sqrt(1/(d(x)+1)) on parent vs sqrt(d(x)/(d(x)+1)) on children using Ry(2*arcsin(1/sqrt(d(x)+1))). Existing `emit_ry` and controlled Ry (`cry`) primitives suffice for gate emission |
| **Walk operators R_A and R_B** | R_A applies D_x in parallel for all vertices x at even depth. R_B applies D_x in parallel for all vertices x at odd depth (plus root reflection). A single quantum walk step is U = R_B * R_A. These form the core walk unitary | HIGH | R_A = direct_sum_{x in A} D_x where A = even-depth vertices. R_B = \|r><r\| + direct_sum_{x in B} D_x where B = odd-depth vertices. Because vertices at the same parity depth act on disjoint qubit sets (different height register positions), the local diffusions commute and can be applied in parallel. Implementation: iterate over height levels with matching parity, apply controlled diffusion conditioned on the one-hot height bit |
| **Detection algorithm (Algorithm 1)** | The primary use case: "Does a marked vertex (solution) exist in the backtracking tree?" Uses phase estimation of walk operator U = R_B * R_A. If the 0-eigenvalue component has probability > 3/8, a solution exists | HIGH | Apply quantum phase estimation to the walk operator starting from root state. Precision parameter controls accuracy vs circuit depth. The detection threshold 3/8 comes from Montanaro's analysis. Must allocate QPE ancilla register, apply controlled-U^(2^k) powers, inverse QFT, measure. Existing IQAE infrastructure provides a model but QPE is a different approach. Alternative: use iterative approach similar to IQAE to avoid explicit QFT |
| **Uniform branching support (constant d)** | Simplest case: every node has exactly d children (e.g., binary tree with d=2, or k-coloring with d=k). Most tutorials and papers assume this | LOW | When d is constant, the diffusion angle theta = 2*arcsin(1/sqrt(d+1)) is the same everywhere. Single Ry rotation, no controlled rotations needed. Dramatically simpler circuits. Must be the default/first implementation path |
| **Node initialization (init_node/init_root)** | Must be able to prepare the initial state for the walk -- typically the root node state \|r> (height=max_depth, all branch registers zero) | LOW | Set height register to max_depth (one-hot: flip the corresponding qubit), branch registers stay \|0>. Simple X gate on the appropriate height qubit. Also need `init_phi` for normalized initial state used in phase estimation |

### Differentiators (Competitive Advantage)

Features that set this framework apart from Qrisp and other quantum walk implementations.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Variable branching support (dynamic d(x))** | Real CSPs have variable branching: at each node, only some children are valid. Montanaro's algorithm handles this via d(x) varying per node, but most implementations assume uniform d. Supporting variable d(x) via predicate-driven pruning is the theoretically correct approach | VERY HIGH | When d(x) varies, the diffusion angle depends on the number of valid children at each node, which itself depends on evaluating the reject predicate on all potential children. Implementation requires: (1) Compute d(x) by evaluating reject on each potential child, storing results in ancilla register, (2) Count valid children (popcount), (3) Controlled rotation conditioned on the count, (4) Uncompute the ancilla. This is where the framework's existing arithmetic (popcount via sum of qbools, controlled Ry) becomes essential. Deferred to post-MVP |
| **Natural Python API consistent with ql.grover()** | `ql.quantum_walk(accept, reject, max_depth, branching)` returns a WalkResult, same style as `ql.grover(predicate, width)`. No quantum computing PhD needed to use it. Qrisp requires explicit QuantumVariable management; our framework can hide register allocation behind the API | MEDIUM | User provides accept/reject as lambdas or decorated functions, max_depth, and branching degree. Framework handles tree encoding, walk operator construction, phase estimation, and result extraction. Consistent with existing API patterns (grover.py, amplitude_estimation.py) |
| **Compile decorator integration** | Walk operators composed via `@ql.compile` for gate caching and optimization. The walk step U = R_B * R_A can be compiled once and replayed for each QPE power, avoiding re-generation overhead | MEDIUM | Wrap the walk step in `@ql.compile`. The compile infrastructure handles caching, controlled variant derivation (needed for QPE controlled-U^k), and inverse generation. Key advantage over Qrisp: our compile+cache system is battle-tested from v2.0-v5.0 |
| **Subspace optimization** | Qrisp demonstrated that restricting the reject function to handle "non-algorithmic subspace" uniformly can halve circuit depth (89->48 depth, 68->38 CNOTs in their example). Our framework can offer this as an option | MEDIUM | When `subspace_optimization=True`, the reject predicate is allowed to return True for states that are not valid tree nodes (e.g., branch_array entries below the current height). This simplifies the diffusion operator. Requires careful analysis but significant depth savings |
| **Solution finding (hybrid classical-quantum)** | Beyond detection: actually find a solution by recursively applying detection to subtrees. Montanaro's Algorithm 2 or the classical-quantum hybrid approach from Qrisp's `find_solution` | HIGH | Hybrid algorithm: (1) Start at root, (2) For each child, run detection on subtree, (3) Recurse into child where solution exists. Total complexity O(sqrt(T) * n^(3/2) * log(n)). Requires subtree extraction and repeated detection runs. Classical outer loop, quantum inner loop |
| **Circuit resource estimation** | Before running, report expected qubit count, circuit depth, and gate count for a given backtracking instance. Critical for users hitting the 17-qubit simulation limit | LOW | Count: max_depth * ceil(log2(branching)) (branch path) + max_depth (one-hot height) + ancilla for predicates + QPE register. Users need this to know if their problem fits in simulation budget |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| **Continuous-time quantum walk** | Alternative to discrete-time; some papers use CTQW for spatial search | Fundamentally different mathematical framework. Montanaro's algorithm is specifically discrete-time. CTQW requires Hamiltonian simulation (matrix exponentials), not gate sequences. Mixing paradigms would create confusion and double the maintenance burden | Stick to discrete-time quantum walk per Montanaro. CTQW is a separate future module if ever needed |
| **General graph quantum walk (non-tree)** | Quantum walks on arbitrary graphs have many applications (element distinctness, triangle finding) | Montanaro's backtracking specifically exploits tree structure. General graph walks need different state spaces, different coin operators, and different analysis. Trying to generalize the tree walk to graphs would compromise the clean tree API | Build the tree walk module cleanly. General graph walks are a separate future module with different abstractions |
| **Automatic CSP-to-predicate compilation** | Users want `ql.quantum_walk.from_cnf(formula)` to auto-generate accept/reject from a CNF formula | SAT/CSP encoding is a research problem in itself. Variable ordering heuristics dramatically affect tree size. Auto-generated predicates will be suboptimal. The predicate interface is the right abstraction boundary | Provide examples showing how to write accept/reject for common CSPs (SAT, graph coloring, Sudoku). Let users control the encoding |
| **Dynamic variable ordering heuristics** | Classical SAT solvers (DPLL) use variable ordering to prune search trees. Quantum version should too | Variable ordering changes the tree structure, requiring different walk operators per ordering. Dynamic reordering mid-walk is incompatible with the quantum walk framework. Static ordering must be fixed before walk construction | Document that variable ordering is chosen by the user when defining max_depth and branch semantics. Provide guidance on good orderings for common problems |
| **QPE with arbitrary precision** | Users want detection with arbitrarily high confidence | QPE precision register adds qubits linearly (k qubits for 2^k precision). With a 17-qubit simulation limit, even 3-4 bits of QPE precision consume scarce qubit budget. High-precision QPE also requires many controlled-U applications, each multiplying circuit depth | Use low precision (2-3 bits) for detection. The 3/8 threshold from Montanaro works with low precision. For higher confidence, repeat the detection classically (majority vote) rather than increasing QPE precision |
| **Direct IQAE-based detection (avoiding QPE)** | IQAE was used for amplitude estimation in v4.0/v5.0; reuse for walk detection | IQAE estimates amplitude of a Grover operator, but the walk operator U = R_B * R_A is not a Grover operator. The spectral analysis is different. IQAE's convergence guarantees assume Grover structure. Forcing IQAE onto the walk operator would give incorrect confidence intervals | Use QPE-based detection as specified by Montanaro. The walk operator's eigenvalue structure is specifically analyzed for QPE, not IQAE |

## Feature Dependencies

```
User-defined accept/reject predicates
    |
    v
Tree state encoding (height + branch_array registers)
    |
    +----> Node initialization (init_root, init_node)
    |          |
    |          v
    +----> Local diffusion operator D_x (uniform branching)
    |          |
    |          v
    +----> Walk operators R_A, R_B (parallel diffusions per level)
    |          |
    |          v
    +----> Walk step U = R_B * R_A
    |          |
    |          +----> @ql.compile integration (cache walk step)
    |          |
    |          v
    +----> Detection algorithm (QPE on walk step)
    |          |
    |          v
    +----> Solution finding (hybrid recursive detection)

Variable branching support (dynamic d(x))
    |
    +---requires--> reject predicate evaluation on all children
    +---requires--> popcount of valid children
    +---requires--> controlled Ry conditioned on count
    +---enhances--> Local diffusion operator D_x

Subspace optimization
    +---enhances--> Walk operators R_A, R_B (reduces depth)
    +---requires--> reject predicate with specific properties

Resource estimation
    +---enhances--> All features (user planning tool)
    +---requires--> Tree state encoding (to count qubits)
```

### Dependency Notes

- **Tree state encoding is foundational:** Every other feature depends on how nodes are represented in the quantum register. The one-hot height + branch array pattern must be decided first and cannot change later without rewriting everything.
- **Uniform branching before variable branching:** D_x with constant d is dramatically simpler (single fixed Ry angle). Variable d(x) requires computing d(x) at runtime, which needs reject evaluation + popcount + controlled rotation. Build the simple case first, extend later.
- **Walk step enables everything above it:** Detection uses QPE on the walk step. Solution finding uses detection. The walk step is the linchpin.
- **@ql.compile integration enables QPE:** QPE requires controlled-U^(2^k). The compile system's `.controlled()` variant derivation provides exactly this capability. Without compile integration, would need manual controlled walk step construction.
- **Detection must precede solution finding:** Solution finding calls detection on subtrees. Cannot build the outer algorithm without the inner one.

## MVP Definition

### Launch With (v6.0 core)

Minimum viable quantum walk module -- enough to demonstrate Montanaro's detection algorithm on a small CSP.

- [ ] **Tree state encoding** -- height register (one-hot) + branch_array (ql.array of qint) encoding tree nodes in quantum state
- [ ] **accept/reject predicate interface** -- User-supplied callables returning qbool, evaluated on tree state. Consistent with oracle.py patterns
- [ ] **Local diffusion D_x (uniform branching)** -- For constant branching degree d, implement D_x = I - 2|psi_x><psi_x| with fixed Ry angle theta = 2*arcsin(1/sqrt(d+1))
- [ ] **Walk operators R_A/R_B** -- Parallel application of D_x across even/odd depth levels, conditioned on one-hot height register
- [ ] **Walk step U = R_B * R_A** -- Composed walk operator, wrapped in @ql.compile for caching
- [ ] **Detection algorithm** -- Phase estimation on walk step, 0-eigenvalue probability > 3/8 indicates solution exists. 2-3 bit QPE precision
- [ ] **Python API: `ql.quantum_walk()`** -- Top-level function accepting accept, reject, max_depth, branching_degree, returns WalkResult with .has_solution boolean
- [ ] **Demo + Qiskit verification** -- Small SAT or graph coloring instance (3-4 variables) verified via Qiskit simulation within 17-qubit budget

### Add After Validation (v6.x)

Features to add once the core walk operator is verified correct.

- [ ] **Variable branching support** -- Dynamic d(x) via reject predicate evaluation + popcount + controlled rotation. Trigger: users need real CSP instances where branching varies
- [ ] **Subspace optimization** -- Reduced circuit depth when reject handles non-algorithmic subspace uniformly. Trigger: circuit depth exceeds simulation budget for interesting problems
- [ ] **Solution finding** -- Hybrid classical-quantum recursive algorithm. Trigger: detection works, users want actual solutions not just existence
- [ ] **Resource estimation** -- Pre-computation qubit/depth report. Trigger: users repeatedly hit qubit budget limits

### Future Consideration (v7+)

Features to defer until quantum walk module is mature.

- [ ] **General graph quantum walks** -- Non-tree walks for element distinctness, triangle finding. Why defer: completely different mathematical framework
- [ ] **CSP-to-predicate compiler** -- Auto-generation of accept/reject from CNF/CSP descriptions. Why defer: encoding optimization is a research problem
- [ ] **Fault-tolerant walk operators** -- Toffoli-decomposed walk step with T-count optimization. Why defer: need correct walk operators before optimizing them
- [ ] **Quantum walk for optimization** -- Extending beyond decision (exists?) to optimization (find best?). Why defer: requires different analysis (quantum walk for MCMC)

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Tree state encoding | HIGH | MEDIUM | P1 |
| accept/reject predicate interface | HIGH | LOW | P1 |
| Local diffusion D_x (uniform) | HIGH | HIGH | P1 |
| Walk operators R_A/R_B | HIGH | HIGH | P1 |
| Walk step U = R_B*R_A | HIGH | MEDIUM | P1 |
| Detection algorithm (QPE) | HIGH | HIGH | P1 |
| Python API ql.quantum_walk() | HIGH | MEDIUM | P1 |
| Demo + Qiskit verification | HIGH | MEDIUM | P1 |
| Variable branching (dynamic d(x)) | MEDIUM | VERY HIGH | P2 |
| Subspace optimization | MEDIUM | MEDIUM | P2 |
| Solution finding (hybrid) | MEDIUM | HIGH | P2 |
| Resource estimation | LOW | LOW | P2 |
| General graph walks | LOW | VERY HIGH | P3 |
| CSP-to-predicate compiler | LOW | HIGH | P3 |

**Priority key:**
- P1: Must have for v6.0 launch -- proves the algorithm works end-to-end
- P2: Should have, add when core is validated and working
- P3: Nice to have, future milestone consideration

## Competitor Feature Analysis

| Feature | Qrisp | Qiskit (no built-in) | Our Approach |
|---------|-------|---------------------|--------------|
| **Backtracking tree class** | `QuantumBacktrackingTree` with explicit register management | No built-in quantum walk backtracking module | `ql.quantum_walk()` function with automatic register allocation, consistent with `ql.grover()` API style |
| **Predicate interface** | `accept`/`reject` returning `QuantumBool`, must manually manage temps | N/A -- user builds from scratch | Lambda predicates auto-traced (like `ql.grover(lambda x: x > 5)`), or decorated functions with `@ql.compile` |
| **Tree encoding** | One-hot height + QuantumArray of QuantumFloat branch variables | N/A | One-hot height (qint) + ql.array of qint branch variables. Reuses existing ql.array infrastructure |
| **Walk operator** | `quantum_step()` method with optional `ctrl` parameter | N/A | @ql.compile-wrapped walk step with automatic controlled variant via compile system |
| **Detection** | `estimate_phase(precision)` using QPE | N/A | Phase estimation on compiled walk step, WalkResult with .has_solution |
| **Solution finding** | `find_solution(precision)` hybrid classical-quantum | N/A | Planned for v6.x, not MVP |
| **Variable branching** | Supported via QuantumFloat branch variable | N/A | Planned for v6.x via reject-predicate evaluation + popcount |
| **Gate efficiency** | 6n+14 CX for single controlled diffuser (binary tree, depth n) | N/A | Target similar or better via existing Toffoli arithmetic and MCZ primitives |
| **Subspace optimization** | Supported via `subspace_optimization=True` flag | N/A | Planned for v6.x |

### Key advantage over Qrisp:
Our framework's `@ql.compile` system automatically generates controlled variants (needed for QPE controlled-U^k), caches compiled gate sequences, and handles adjoint generation. Qrisp requires explicit `ctrl` parameter threading. Additionally, our existing lambda-to-oracle tracing (from Grover) can be adapted for accept/reject predicates, giving a more Pythonic API.

### Key challenge vs Qrisp:
Qrisp is built on a higher-level abstraction (QuantumVariable with automatic session management). Our framework is closer to the gate level. The tree encoding and diffusion operator construction require more explicit qubit management in our framework.

## Qubit Budget Analysis (Critical Constraint)

Given the 17-qubit simulation limit, here is the qubit budget for a minimal demo:

| Component | Qubits for binary tree, depth 3 | Qubits for ternary tree, depth 2 |
|-----------|----------------------------------|----------------------------------|
| Height register (one-hot) | 4 (depths 0,1,2,3) | 3 (depths 0,1,2) |
| Branch path (log2(d) per level) | 3 * 1 = 3 (1 bit per binary choice) | 2 * 2 = 4 (2 bits per ternary choice) |
| Predicate ancillae (estimate) | 2-4 | 2-4 |
| QPE precision register | 2-3 | 2-3 |
| **Total** | **11-14** | **11-14** |

This fits within the 17-qubit budget for small instances. A binary tree of depth 4 would need ~15-17 qubits (tight). Depth 5+ exceeds the budget.

**Target demo:** Binary tree of depth 3 (8 leaves, 15 nodes) with a simple predicate (e.g., 2-variable SAT or 3-coloring of a tiny graph). This gives enough room for QPE while staying within simulation limits.

## Sources

- [Montanaro 2015 - Quantum walk speedup of backtracking algorithms (arXiv:1509.02374)](https://arxiv.org/abs/1509.02374)
- [Montanaro 2018 - Published version in Theory of Computing](https://theoryofcomputing.org/articles/v014a015/)
- [Martiel 2019 - Practical implementation of a quantum backtracking algorithm (arXiv:1908.11291)](https://arxiv.org/abs/1908.11291)
- [Qrisp QuantumBacktrackingTree documentation](https://qrisp.eu/reference/Algorithms/QuantumBacktrackingTree.html)
- [Qrisp Sudoku tutorial with quantum backtracking](https://www.qrisp.eu/general/tutorial/Sudoku.html)
- [Seidel et al. 2024 - Quantum Backtracking in Qrisp Applied to Sudoku Problems (arXiv:2402.10060)](https://arxiv.org/abs/2402.10060)
- [Quantum Search on Computation Trees (arXiv:2505.22405)](https://arxiv.org/html/2505.22405) -- generalized variable-time walk
- [Jarret & Wan 2018 - Improved quantum backtracking using effective resistance estimates (arXiv:1711.05295)](https://arxiv.org/abs/1711.05295)

---
*Feature research for: v6.0 Quantum Walk Primitives (Montanaro 2015 Backtracking)*
*Researched: 2026-02-26*
