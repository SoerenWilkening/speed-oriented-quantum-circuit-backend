# Architecture Research: v6.0 Quantum Walk Primitives (Montanaro 2015 Backtracking)

**Domain:** Quantum programming framework -- quantum walk operators for backtracking speedup
**Researched:** 2026-02-26
**Confidence:** HIGH (codebase-verified integration points, algorithm from published literature, reference implementation studied in Qrisp)

## System Overview: Current Architecture

```
+------------------------------------------------------------------+
|                    Python Frontend (ql.*)                          |
|  +-----------+  +----------+  +---------+  +---------+  +-------+ |
|  | qint      |  | qint_mod |  | compile |  | grover  |  | amp   | |
|  | qbool     |  | (v1.0)   |  | (v2.0)  |  | (v4.0)  |  | est   | |
|  | qarray    |  |          |  |         |  |         |  | (v4.0)| |
|  +-----------+  +----------+  +---------+  +---------+  +-------+ |
|       |              |             |            |            |     |
|  +-----------+  +--------------------------------------------------+
|  | quantum_  |  | quantum_counting (v5.0)                         |
|  | walk      |  +--------------------------------------------------+
|  | (v6.0)    |                                                     |
|  | **NEW**   |                                                     |
|  +-----------+                                                     |
+--------------------------------------------------------------------+
|                    Cython Bindings                                  |
|  +----------+  +----------+  +---------+  +----------+             |
|  | _core.pyx|  | qint.pyx |  |_gates.pyx| |openqasm  |            |
|  | (circuit)|  | (arith)  |  |(emit_*)  | |(.pyx)    |            |
|  +----------+  +----------+  +---------+  +----------+             |
+--------------------------------------------------------------------+
|                    C Backend                                        |
|  +-------------+  +----------------+  +------------+  +-----------+|
|  | hot_path_   |  | ToffoliAddition|  | circuit.h  |  | qubit_   ||
|  | add/mul/xor |  | CDKM/CLA/Mul  |  | (circuit_t)|  | allocator||
|  +-------------+  +----------------+  +------------+  +-----------+|
|  +-------------+  +----------------+  +------------+               |
|  | arithmetic_ |  | execution.c    |  | sequences/ |               |
|  | ops.h (QFT) |  | run_instruction|  | (hardcoded)|               |
|  +-------------+  +----------------+  +------------+               |
+--------------------------------------------------------------------+
```

### Component Responsibilities (v6.0 changes marked)

| Component | Responsibility | v6.0 Impact |
|-----------|----------------|-------------|
| `grover.py` | Grover search with BBHT adaptive / exact-M modes, oracle synthesis | **NO CHANGE** -- quantum walk is architecturally separate |
| `oracle.py` | `GroverOracle` wrapper, predicate-to-oracle tracing, validation | **REFERENCE PATTERN** -- predicate tracing mechanism reused |
| `diffusion.py` | Grover's global S_0 diffusion (X-MCZ-X) | **REFERENCE PATTERN** -- local diffusion D_x is analogous but per-node |
| `amplitude_estimation.py` | IQAE with multi-shot simulation | **REUSE** -- detection algorithm uses phase estimation internally |
| `compile.py` | Gate capture/replay with caching | **REUSE** -- walk step operators can be compiled for replay |
| `_gates.pyx` | emit_ry, emit_h, emit_x, emit_mcz, emit_p_raw | **REUSE** -- D_x needs emit_ry for controlled rotations, emit_x, emit_mcz |
| `_core.pyx` | circuit(), option(), allocator, gate extract/inject | **REUSE** -- standard circuit building path |
| `sim_backend.py` | Qiskit QASM load and simulate | **REUSE** -- verification and demo execution |
| `qint.pyx` | branch(), arithmetic, comparisons | **REUSE** -- predicate evaluation uses existing operators |

## New Component: `quantum_walk.py`

### Why a Separate Module (Not Extending `grover.py`)

Quantum walk and Grover search share the *concept* of amplitude amplification but differ fundamentally:

| Aspect | Grover (`grover.py`) | Quantum Walk (`quantum_walk.py`) |
|--------|---------------------|----------------------------------|
| State space | Flat superposition over N items | Tree of partial assignments (exponentially structured) |
| Diffusion | Global S_0 on all qubits (X-MCZ-X) | Local D_x per node, amplitude depends on children count |
| Oracle | Boolean predicate returning accept/reject via qbool | Tri-valued predicate returning accept/reject/indeterminate |
| Operators | Oracle + diffusion (2 operators) | R_A + R_B (2 operators, each composed from many D_x) |
| Detection | Direct measurement after iterations | Quantum phase estimation on walk operator |
| Registers | Search registers only | Node register (branch_qa) + height register (h) + coin qubits |

Grover is a special case of quantum walk, but the implementation has almost no shared code. The shared pieces (emit_ry, emit_mcz, circuit building, qint operators for predicate evaluation) are already in the lower layers.

### Module Structure

```
src/quantum_language/
    quantum_walk.py          # NEW: Main module -- public API + algorithm
    quantum_walk_tree.py     # NEW: Tree state encoding (height, path registers)
    quantum_walk_diffusion.py # NEW: Local diffusion D_x and R_A/R_B operators
```

**Rationale for splitting into 3 files:**
- `quantum_walk.py` owns the public API (`ql.quantum_walk()`, `ql.detect_solution()`) and the detection/search algorithms
- `quantum_walk_tree.py` owns the qubit register layout and tree node encoding -- this is the data structure
- `quantum_walk_diffusion.py` owns the D_x construction, R_A, R_B -- this is the operator construction

This mirrors how grover.py (algorithm), diffusion.py (operator), and oracle.py (predicate handling) are split today.

## Algorithm Architecture: Montanaro Backtracking

### The Predicate Interface

The predicate is the user-supplied function that defines the constraint satisfaction problem. Unlike Grover's boolean predicate (returns qbool), the walk predicate is **tri-valued**:

```python
class PredicateResult(enum.IntEnum):
    ACCEPT = 0      # Node is a valid complete solution
    REJECT = 1      # Node violates constraints; prune entire subtree
    INDETERMINATE = 2  # Node is consistent so far; continue exploring children
```

**Implementation via two qbools:** The predicate returns two qbool values `(is_accept, is_reject)` rather than a single tri-state. This maps naturally to the existing tracing infrastructure:

```python
def my_predicate(tree):
    """User defines accept and reject as separate conditions."""
    # tree.node gives current partial assignment as qint values
    # tree.depth gives current depth
    # User evaluates constraints using existing qint operators
    is_accept = (tree.depth == 0) & all_constraints_satisfied(tree)
    is_reject = some_constraint_violated(tree)
    return is_accept, is_reject
```

**Key difference from Grover's oracle interface:** Grover's `_predicate_to_oracle` calls the predicate with qint args and expects a single qbool return. The walk predicate takes a tree state object and returns two qbools. However, the underlying mechanism is identical: the predicate calls are traced (gates captured into the circuit) using existing qint/qbool operators. No new tracing infrastructure is needed.

**Alternative considered and rejected:** Single predicate function called twice (once for accept, once for reject). Rejected because it would double the gate count for predicates that share computation between accept and reject evaluation.

### Tree State Encoding

The tree is encoded in quantum registers following the Montanaro/Martiel convention:

```
Registers:
  branch_qa[0..max_depth-1]  : QuantumArray of qint, each width = ceil(log2(branching_factor))
                                Stores the path from root to current node (reversed)
  h[0..max_depth]            : One-hot encoded height register (max_depth+1 qubits)
                                Root has h = max_depth, leaves have h = 0

Node encoding example (binary tree, depth 4):
  Node at path [left, right, left] = branch_qa = |0,1,0,*| with h = |00010| (height 1)
  Root = branch_qa = |*,*,*,*| with h = |10000| (height 4)
```

**Why one-hot for height:** The height register participates in conditional operations (controlled gates). One-hot encoding means "is height == k?" is a single qubit check (control on h[k]) rather than a multi-qubit comparison. This saves gates in the local diffusion operator which must condition on height.

**Qubit budget:**

| Register | Qubits | Purpose |
|----------|--------|---------|
| branch_qa | max_depth * ceil(log2(branching_factor)) | Path encoding |
| h | max_depth + 1 | One-hot height |
| predicate ancillae | varies per predicate | Accept/reject evaluation |
| Total (binary, depth n) | n + (n+1) + ancillae = 2n + 1 + ancillae | |

For the 17-qubit simulator limit: a binary tree of depth 5 needs 11 data qubits, leaving 6 for predicate ancillae. Depth 4 needs 9 data qubits, leaving 8. Small SAT demos (2-3 variables, binary branching, depth 3-4) are feasible.

### Tree State Class

```python
class QuantumBacktrackingTree:
    """Quantum state representing a node in the backtracking tree.

    Manages qubit registers for the tree encoding and provides
    methods to initialize, traverse, and inspect nodes.
    """
    def __init__(self, max_depth, branching_factor, accept_fn, reject_fn):
        self.max_depth = max_depth
        self.branching_factor = branching_factor
        self._accept_fn = accept_fn
        self._reject_fn = reject_fn

        # Compute widths
        self._branch_width = math.ceil(math.log2(branching_factor))

        # Allocate registers (qint/qarray from existing infrastructure)
        self.branch_qa = ql.array(dim=max_depth, width=self._branch_width)
        self.h = ql.qint(0, width=max_depth + 1)  # One-hot height

        # Initialize root: h = max_depth (one-hot)
        # Set bit at position max_depth in h register
```

**Integration with existing types:** The tree uses `ql.array` (qarray) for branch_qa and `ql.qint` for the height register. No new quantum types needed. The predicate functions receive the tree object and use its `.branch_qa` and `.h` attributes to evaluate constraints using standard qint operators.

### Local Diffusion Operator D_x

The local diffusion D_x is the core building block. For each non-leaf, non-marked node x with d(x) valid children:

```
D_x acts on subspace H_x = span{|x>, |child_1>, ..., |child_d(x)>}

Define |psi_x> = (1/sqrt(d(x)+1)) * (|x> + sum_{y: child of x} |y>)

D_x = 2|psi_x><psi_x| - I    (reflection about |psi_x>)

Special cases:
  - If x is accepted (solution found): D_x = I (identity)
  - If x is rejected (prune subtree): D_x = -I (global phase flip)
```

**Gate-level implementation of D_x:**

The key insight is that D_x is a Grover-like diffusion but restricted to the subspace of x and its children. The amplitude `1/sqrt(d(x)+1)` determines a rotation angle:

```
theta_x = 2 * arcsin(1/sqrt(d(x)+1))
```

This is implemented as:

1. **Evaluate predicate** on node x to get (is_accept, is_reject)
2. **If accepted:** Apply identity (skip diffusion, or apply phase flip for detection)
3. **If rejected:** Apply -I (Z gate on height qubit, controlled on is_reject)
4. **If indeterminate:** Apply local Grover diffusion:
   a. Controlled-Ry(theta_x) on coin qubits to create superposition of children
   b. X gates on all children labels
   c. MCZ on the children-label subspace
   d. X gates (undo)
   e. Controlled-Ry(-theta_x) to undo coin rotation

**Variable branching (d(x) varies per node):**

For fixed branching (all nodes have the same number of children), theta_x is a constant and can be precomputed. For variable branching, the predicate must additionally compute d(x) -- the number of valid children -- and the Ry angle is controlled on d(x). This requires:

```python
# Count valid children by evaluating predicate on each potential child
count = 0
for child_label in range(branching_factor):
    # Check if this child is not rejected
    child_valid = ~reject_fn(child_state(x, child_label))
    count += child_valid

# theta depends on count: theta = 2 * arcsin(1/sqrt(count+1))
# Implemented as controlled-Ry rotations conditioned on count value
```

This is the most expensive part of the algorithm. For a branching factor of b, evaluating all b children requires b predicate calls. The controlled-Ry rotation conditioned on the count value requires at most b different angle values, each controlled on the count register.

**Architecture decision: Variable branching is deferred to v6.1.** The v6.0 implementation assumes fixed branching factor (all nodes have the same number of children, which is the common case for SAT/CSP backtracking where each level assigns one variable with d possible values). Variable branching requires counting valid children per-node, which is significantly more complex and can be layered on top of the fixed-branching infrastructure.

### R_A and R_B Operators

The vertices of the backtracking tree are partitioned into two sets based on distance from root:

```
A = {vertices at even distance from root}
B = {vertices at odd distance from root}

R_A = direct_sum_{x in A} D_x     (apply D_x independently to each even-level node)
R_B = |root><root| + direct_sum_{x in B} D_x   (root reflection + odd-level diffusions)
```

**Why R_A and R_B parallelize naturally:**

Nodes at the same depth operate on disjoint qubits (different branch_qa entries and different height register bits). The existing circuit builder in `_core.pyx` (`add_gate` -> `optimizer.c`) automatically parallelizes gates on disjoint qubits into the same layer. Therefore, emitting all D_x for x in A sequentially in Python code produces a circuit where the D_x operations on sibling nodes are automatically parallelized. No explicit parallelization code is needed.

**Implementation pattern:**

```python
def emit_R_A(tree):
    """Emit R_A: local diffusions on even-depth nodes."""
    for depth in range(0, tree.max_depth + 1, 2):
        # D_x for all nodes at this depth
        # Controlled on h[depth] being active (one-hot)
        _emit_local_diffusion(tree, depth)

def emit_R_B(tree):
    """Emit R_B: root reflection + local diffusions on odd-depth nodes."""
    _emit_root_reflection(tree)
    for depth in range(1, tree.max_depth + 1, 2):
        _emit_local_diffusion(tree, depth)
```

The local diffusion at each depth is automatically conditioned on the height register (one-hot), so it only activates for nodes at that depth. This is implemented using the existing `with qbool:` controlled context mechanism -- the height qubit serves as the control.

### Walk Step and Detection Algorithm

**Walk step:** W = R_B * R_A

One step of the quantum walk applies R_A then R_B. This is the analog of one Grover iteration (oracle + diffusion) but on the tree structure.

**Detection algorithm (Montanaro Algorithm 1):**

```
1. Prepare initial state |root> (set h to one-hot max_depth, branch_qa to |0>)
2. Apply quantum phase estimation on walk operator W:
   a. Allocate precision register (p qubits)
   b. Apply controlled-W^{2^k} for k = 0, ..., p-1
   c. Apply inverse QFT on precision register
   d. Measure precision register
3. If measured phase is close to 0: output "marked vertex exists"
   Otherwise: output "no marked vertex"
```

**Architecture decision: Phase estimation deferred; use power method instead.**

Full QPE requires an inverse QFT on the precision register, which adds significant qubit overhead and gate count. For the initial v6.0 implementation, use the detection approach from Montanaro Algorithm 1 directly:

```
Repeat O(sqrt(T) * n^{3/2}) times:
  1. Initialize |root>
  2. Apply t steps of the walk (W^t)
  3. Measure
  4. Check if result is a marked vertex

If no marked vertex found after all repetitions: output "no marked vertex"
```

This is analogous to how `ql.grover()` uses BBHT adaptive search rather than exact iteration counts. The walk step count t is varied adaptively.

**Alternative (QPE-based):** The full QPE approach offers better theoretical complexity but requires:
- Additional precision qubits (at least 3-5)
- Inverse QFT circuit
- Larger total qubit count

For v6.0 demos within the 17-qubit simulator limit, the iterative approach is more practical. QPE can be added as an enhancement in v6.1.

## Data Flow

### Walk Operation Data Flow

```
User Code
    |
    v
ql.quantum_walk(accept, reject, max_depth, branching_factor)
    |
    v
quantum_walk.py: _build_walk_circuit()
    |
    +-- quantum_walk_tree.py: QuantumBacktrackingTree()
    |       |-- ql.array(dim=max_depth, ...) -> qarray (branch_qa)
    |       |-- ql.qint(0, width=max_depth+1) -> qint (height)
    |       |-- Qubit allocation via _core._allocate_qubit()
    |
    +-- quantum_walk_diffusion.py: emit_walk_step()
    |       |-- emit_R_A(tree)
    |       |       |-- For each even depth:
    |       |       |     emit_local_diffusion(tree, depth)
    |       |       |       |-- Evaluate predicate via user fn (gates traced)
    |       |       |       |-- Controlled-Ry(theta) via emit_ry()
    |       |       |       |-- MCZ via emit_mcz()
    |       |       |       |-- Uncompute predicate (reverse gates)
    |       |
    |       |-- emit_R_B(tree)
    |               |-- emit_root_reflection()
    |               |-- For each odd depth: emit_local_diffusion()
    |
    +-- Repeat walk steps (adaptive or fixed count)
    |
    +-- to_openqasm() -> QASM string
    |
    +-- sim_backend.simulate() -> measurement
    |
    v
Parse result, verify classically, return
```

### Predicate Evaluation Flow (Key Integration Point)

```
quantum_walk_diffusion.py:
    _emit_local_diffusion(tree, depth)
        |
        v
    # 1. Condition on height = depth (use with tree.h_qubit(depth):)
    with height_control:
        |
        v
    # 2. Call user predicate (TRACED -- gates captured into circuit)
    is_accept, is_reject = tree._accept_fn(tree), tree._reject_fn(tree)
        |                       |
        v                       v
    # User code runs:       # User code runs:
    # qint comparisons      # qint comparisons
    # via existing           # via existing
    # operators              # operators
    # -> qbool result        # -> qbool result
        |
        v
    # 3. Controlled operations based on predicate results
    with is_reject:
        emit_z(height_qubit)     # -I for rejected nodes

    with ~is_accept & ~is_reject:
        _emit_grover_diffusion_local(tree, depth)  # Ry + MCZ + Ry^dag
        |
        v
    # 4. Uncompute predicate ancillae
    # (reverse the gates from step 2)
```

**Critical insight:** The predicate evaluation uses the exact same tracing mechanism as Grover's `_predicate_to_oracle`. The user writes Python code using qint operators; those operators emit gates into the circuit. The difference is that the walk predicate is evaluated inside a controlled context (conditioned on height), and its output is used to conditionally select between identity, phase-flip, and local diffusion -- rather than just phase-flipping.

### Comparison: Grover Oracle vs Walk Predicate Tracing

| Aspect | Grover Oracle Tracing | Walk Predicate Tracing |
|--------|----------------------|----------------------|
| Input | qint registers | Tree state (branch_qa, h) |
| Output | Single qbool (marks solutions) | Two qbools (accept, reject) |
| Context | Direct call | Inside height-controlled context |
| Uncompute | Automatic via compute-phase-uncompute | Must be explicit (reverse predicate gates) |
| Gate emission | Via `_predicate_to_oracle` wrapper | Directly in diffusion operator loop |
| Caching | Oracle caches captured gates | Walk operator compiled via `@ql.compile` |

## Integration Points

### New Module -> Existing Infrastructure

| New Code | Uses From | How |
|----------|-----------|-----|
| `quantum_walk.py` | `_core.circuit()` | Fresh circuit per walk attempt |
| `quantum_walk.py` | `_core.option()` | Set fault_tolerant mode |
| `quantum_walk.py` | `openqasm.to_openqasm()` | Export for simulation |
| `quantum_walk.py` | `sim_backend.simulate()` | Qiskit verification |
| `quantum_walk_tree.py` | `qint`, `qarray` | Register allocation |
| `quantum_walk_tree.py` | `_core._allocate_qubit()` | Height register qubits |
| `quantum_walk_diffusion.py` | `_gates.emit_ry()` | Local diffusion rotation |
| `quantum_walk_diffusion.py` | `_gates.emit_x()` | Pauli-X in diffusion |
| `quantum_walk_diffusion.py` | `_gates.emit_mcz()` | Multi-controlled Z |
| `quantum_walk_diffusion.py` | `_gates.emit_h()` | Hadamard in coin operations |
| `quantum_walk_diffusion.py` | `_core.extract_gate_range()` | Capture predicate gates for uncompute |
| `quantum_walk_diffusion.py` | `_core.reverse_instruction_range()` | Uncompute predicate evaluation |
| `quantum_walk_diffusion.py` | `diffusion._collect_qubits()` | Gather qubits for MCZ |

### Existing Infrastructure -> Unchanged

No modifications needed to:
- `grover.py` -- separate algorithm, no coupling
- `oracle.py` -- walk uses its own predicate interface
- `diffusion.py` -- walk uses local diffusion, not global S_0
- `compile.py` -- walk operators can optionally use `@ql.compile`
- `_gates.pyx` -- all needed gate primitives already exist
- `_core.pyx` -- all needed circuit operations already exist
- C backend -- no new gate types, no new C code needed

### `__init__.py` Modification

Add exports:

```python
from .quantum_walk import quantum_walk, detect_solution
```

## Architectural Patterns

### Pattern 1: Predicate Tracing via Existing Operators

**What:** User-defined predicates are Python functions that take quantum types as arguments. When called during circuit building, the qint/qbool operators automatically emit gates into the circuit. No AST parsing, no custom tracing infrastructure.

**When to use:** Any new algorithm that needs user-defined quantum functions.

**This is the same pattern used by:**
- `_predicate_to_oracle` in `oracle.py` (Grover lambda predicates)
- `@ql.compile` capture phase in `compile.py`

**For quantum walk:** The accept/reject predicates follow this pattern exactly. The user writes:

```python
def accept(tree):
    # tree.branch_qa[i] gives qint for assignment at depth i
    # tree.h is the height register
    return (tree.h == 0)  # Accept only leaves (complete assignments)

def reject(tree):
    # Check constraint: no two adjacent nodes same color (graph coloring)
    conflict = (tree.branch_qa[0] == tree.branch_qa[1])
    return conflict
```

These functions, when called during the walk operator construction, emit comparison gates into the circuit. The resulting qbool values are used as controls for the diffusion operator.

### Pattern 2: Height-Controlled Operations via `with` Context

**What:** Operations that should only activate at a specific tree depth are wrapped in `with h_qubit:` blocks, leveraging the existing quantum conditional mechanism.

**When to use:** Any depth-dependent operation in the walk.

**Implementation:**

```python
# In quantum_walk_diffusion.py
from quantum_language.qbool import qbool

def _emit_local_diffusion(tree, depth):
    """Emit D_x for all nodes at given depth."""
    # Get the one-hot height qubit for this depth
    h_qubit = tree.h_qubit(depth)  # Returns a qbool wrapping that qubit

    with h_qubit:
        # Everything inside here is controlled on h[depth] == 1
        # This means it only activates for nodes at this depth
        is_accept, is_reject = _evaluate_predicate(tree)
        _apply_diffusion_conditional(tree, is_accept, is_reject)
```

This reuses the existing `_controlled` / `_control_bool` mechanism in `_core.pyx`. When inside `with h_qubit:`, all emitted gates automatically become controlled gates (CRy instead of Ry, CX instead of X, etc.).

### Pattern 3: Capture-Reverse for Predicate Uncomputation

**What:** Capture the gate range emitted during predicate evaluation, then reverse it to uncompute ancillae.

**When to use:** After using the predicate result (is_accept, is_reject) as controls.

**Implementation:**

```python
start_layer = get_current_layer()

# Evaluate predicate (gates emitted into circuit)
is_accept, is_reject = _evaluate_predicate(tree)

pred_end_layer = get_current_layer()

# Use predicate results as controls for diffusion
_apply_conditional_diffusion(tree, is_accept, is_reject)

# Uncompute predicate (reverse the captured gates)
reverse_instruction_range(start_layer, pred_end_layer)
```

This is the same compute-phase-uncompute pattern used by `GroverOracle` (`_validate_compute_phase_uncompute`), but applied manually rather than via decoration.

### Pattern 4: Module-per-Concern (Algorithm / DataStructure / Operators)

**What:** Split a complex algorithm module into algorithm logic, data structure, and operator construction.

**Precedent:** Grover uses grover.py (algorithm), oracle.py (predicate handling), diffusion.py (operator). Quantum walk follows the same split.

**Benefits:**
- `quantum_walk_tree.py` can be tested independently (register allocation, initialization)
- `quantum_walk_diffusion.py` can be unit-tested with mock predicates
- `quantum_walk.py` composes the pieces and adds the simulation/measurement layer

## Anti-Patterns

### Anti-Pattern 1: Implementing D_x as a Matrix

**What people do:** Construct the D_x operator as a unitary matrix and apply it.
**Why it's wrong:** D_x acts on a subspace of size d(x)+1, which grows with branching factor. Matrix construction is O(d^2) and doesn't compose with circuit building.
**Do this instead:** Implement D_x as a sequence of Ry + MCZ + Ry^{dag} gates (Grover-like diffusion restricted to the local subspace). This emits gates into the circuit naturally.

### Anti-Pattern 2: Evaluating Predicate on Every Possible Node

**What people do:** Loop over all possible nodes in the tree and evaluate the predicate on each.
**Why it's wrong:** There are O(b^n) nodes in a tree of branching factor b and depth n. The whole point of the quantum walk is that it explores the tree in superposition.
**Do this instead:** The predicate is evaluated once per depth level inside the R_A/R_B operators. The quantum superposition handles all nodes at that depth simultaneously. The predicate gates are controlled on the height register, so they only activate for the appropriate depth.

### Anti-Pattern 3: Using Grover Diffusion (X-MCZ-X) for Local Diffusion

**What people do:** Reuse the existing `diffusion()` function from `diffusion.py` for D_x.
**Why it's wrong:** Grover diffusion reflects about the equal superposition |+...+>. Local diffusion D_x reflects about |psi_x> which has amplitudes `1/sqrt(d(x)+1)`, not `1/sqrt(N)`. Using the wrong amplitudes breaks the walk.
**Do this instead:** Implement local diffusion with Ry(theta_x) rotations where `theta_x = 2 * arcsin(1/sqrt(d(x)+1))`, then X-MCZ-X on the coin register, then Ry(-theta_x).

### Anti-Pattern 4: Adding New Gate Types to the C Backend

**What people do:** Add walk-specific gate types to the Standardgate_t enum in types.h.
**Why it's wrong:** The quantum walk uses only standard gates (Ry, X, CX, MCZ, H, Z). Adding walk-specific gates would require changes across the entire C backend (output, optimizer, QASM export, Clifford+T decomposition, visualization).
**Do this instead:** Compose walk operators from existing gate primitives. The circuit builder handles parallelization automatically.

## Scalability Considerations

| Circuit Size | Approach | Feasibility |
|-------------|----------|-------------|
| Depth 2-3, binary (5-7 data qubits) | Direct Qiskit simulation | Easy -- well within 17-qubit limit |
| Depth 4, binary (9 data qubits) | MPS simulator if ancillae push past 17 | Moderate -- depends on predicate complexity |
| Depth 5+, binary (11+ data qubits) | Likely exceeds 17-qubit limit with ancillae | Not feasible for simulation; circuit generation still works |
| Higher branching (4-ary, depth 3) | 6 branch qubits + 4 height = 10 data | Tight but possible for simple predicates |

**The 17-qubit simulator limit is the primary scaling constraint.** The quantum walk algorithm requires O(n log d) data qubits plus predicate ancillae. For v6.0 demos:

- **2-variable 2-SAT, binary tree depth 3:** 3 branch + 4 height = 7 data qubits. Predicate needs ~2-3 ancillae for comparisons. Total ~10 qubits. Feasible.
- **3-variable 2-SAT, binary tree depth 4:** 4 branch + 5 height = 9 data qubits. Total ~12-14 qubits. Tight but feasible.
- **Graph 3-coloring, 2 vertices:** 2-bit color per vertex, depth 3, branching 3. 3*2 + 4 = 10 data. Close to limit.

## Build Order (Suggested Phases)

### Phase 1: Tree State Encoding (quantum_walk_tree.py)

**Dependencies:** qint, qarray, _core._allocate_qubit
**New:** QuantumBacktrackingTree class, register allocation, initialization
**Test:** Create tree, verify qubit allocation, check one-hot height encoding
**No predicate needed yet -- pure data structure.**

### Phase 2: Local Diffusion Operator (quantum_walk_diffusion.py)

**Dependencies:** Phase 1 + _gates.emit_ry, emit_x, emit_mcz, _core.extract_gate_range, reverse_instruction_range
**New:** _emit_local_diffusion(), _emit_R_A(), _emit_R_B()
**Test:** Build diffusion circuit for fixed-branching tree, verify gate count, verify amplitudes via statevector simulation
**Requires mock predicate (always indeterminate) for initial testing.**

### Phase 3: Predicate Integration

**Dependencies:** Phase 2 + existing qint comparison operators
**New:** Predicate calling convention, accept/reject evaluation, predicate uncomputation
**Test:** Real predicate (e.g., simple equality check), verify gates traced correctly, verify uncomputation leaves zero ancilla delta
**This is where the oracle-like tracing is integrated.**

### Phase 4: Walk Step and Detection Algorithm (quantum_walk.py)

**Dependencies:** Phase 3 + sim_backend, to_openqasm
**New:** quantum_walk() public API, detection algorithm, adaptive walk
**Test:** End-to-end demo on small SAT instance, Qiskit verification
**Composes all pieces into the user-facing API.**

### Phase 5: Verification and Demo

**Dependencies:** Phase 4
**New:** Test suite, 2-SAT demo, graph coloring demo
**Test:** Verify detection probability, compare with classical brute force

## Decision: All Python, No New C Code

**Recommendation:** Keep all quantum walk code in Python. No new C backend code needed.

**Rationale:**

1. **All required gate primitives exist in C.** The walk uses Ry, X, CX, CCX, MCZ, H, Z -- all already available via `_gates.pyx` which calls C functions (`ry()`, `x()`, `mcz()`, etc.).

2. **The walk is compositional, not computational.** Unlike arithmetic (where inner loops over bit positions benefit from C speed), the walk constructs circuits by composing high-level operators. The inner loop is over tree depth levels (typically 3-5), not over bit positions (which could be 64).

3. **Predicate evaluation is in Python by design.** The user's predicate function is Python code. It uses qint operators that dispatch to C. Adding C code for the walk would not help because the bottleneck is the Python predicate, not the walk operator emission.

4. **Performance is not a concern.** Circuit generation time is dominated by the number of gates, not the Python overhead of emitting them. A depth-4 binary walk generates hundreds of gates; a depth-8 walk generates thousands. Both are built in milliseconds. The Cython/C path is relevant for arithmetic inner loops (millions of gates for large-width operations), not for walk construction.

5. **Maintainability.** The grover.py, diffusion.py, oracle.py, amplitude_estimation.py, and quantum_counting.py modules are all pure Python. Adding the walk module in the same style keeps the codebase consistent.

**When C would be appropriate (future):** If variable branching with many children (b > 16) requires many controlled-Ry rotations per node, the rotation emission loop could be moved to C for speed. But this is a v6.1+ concern, not v6.0.

## Sources

- [Montanaro 2015 -- Quantum walk speedup of backtracking algorithms (arXiv:1509.02374)](https://arxiv.org/abs/1509.02374)
- [Martiel & Remaud 2019 -- Practical implementation of a quantum backtracking algorithm (arXiv:1908.11291)](https://arxiv.org/abs/1908.11291)
- [Qrisp Quantum Backtracking reference implementation](https://qrisp.eu/reference/Algorithms/QuantumBacktrackingTree.html)
- [Qrisp Sudoku tutorial -- practical backtracking application](https://www.qrisp.eu/general/tutorial/Sudoku.html)
- [Jarret & Wan 2018 -- Improved quantum backtracking via effective resistance (arXiv:1711.05295)](https://arxiv.org/abs/1711.05295)

---
*Architecture research for: v6.0 Quantum Walk Primitives (Montanaro 2015 Backtracking)*
*Researched: 2026-02-26*
