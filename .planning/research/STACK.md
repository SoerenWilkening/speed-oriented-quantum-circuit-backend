# Stack Research: v6.0 Quantum Walk Primitives (Montanaro 2015)

**Domain:** Quantum walk operators for backtracking tree search (constraint satisfaction speedup)
**Researched:** 2026-02-26
**Confidence:** HIGH (core gate primitives all exist; additions are algorithmic Python composing existing infrastructure)

## Executive Summary

The v6.0 quantum walk milestone requires **no new external dependencies** and **no new C-level gate primitives**. The existing gate set -- Ry, CRy, H, CH, X, CX, MCZ, MCX, P, CP -- is sufficient to implement every operator from Montanaro's 2015 backtracking algorithm. What is needed is a new Python-level `quantum_walk` module that composes these existing primitives into the walk operators R_A, R_B, and the local diffusion D_x with correct amplitudes based on variable branching factor d(x).

The detection mode (Montanaro Algorithm 1) requires estimating the eigenvalue of the walk operator R_B R_A. Rather than building full Quantum Phase Estimation (QPE) from scratch, the existing IQAE infrastructure (`ql.amplitude_estimate()`) can be adapted to work with walk-step powers instead of Grover-iterate powers, reusing the entire verified estimation pipeline.

The primary engineering work is algorithmic composition in Python -- about 400-600 lines in a new `quantum_walk.py` module, following the same patterns as `grover.py` and `amplitude_estimation.py`.

## Recommended Stack

### Core Technologies (All Existing -- No Changes)

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| Python | >=3.11 | Frontend quantum_walk module, algorithm orchestration | **Existing** -- no change |
| Cython | >=3.0.11,<4.0 | Gate emission (emit_ry, emit_h, emit_x, emit_mcz, emit_p) | **Existing** -- no change |
| C backend (gcc/clang, C11) | System | gate.h: ry, cry, h, ch, x, cx, mcz, mcx, p, cp | **Existing** -- no change |
| NumPy | >=1.24 | Angle calculations (arctan, sqrt) | **Existing** -- no change |
| SciPy | >=1.10 | IQAE confidence intervals (already used by amplitude_estimation.py) | **Existing** -- no change |
| Qiskit | >=1.0 | Verification via sim_backend.py | **Existing** -- no change |
| qiskit-aer | >=0.13 | Simulation (statevector + measurement) | **Existing** -- no change |
| Pillow | >=9.0 | Circuit visualization (optional) | **Existing** -- no change |

### New Module (Pure Python, No New Dependencies)

| Module | Lines (est.) | Purpose | Built From |
|--------|-------------|---------|------------|
| `src/quantum_language/quantum_walk.py` | 400-600 | Walk operators, local diffusion, tree encoding, detection | Composes emit_ry, emit_h, emit_x, emit_mcz, emit_p via existing _gates.pyx |

## What Exists vs What Is Needed

### Already Available (DO NOT Re-implement)

| Primitive | Location | How Walk Uses It |
|-----------|----------|-----------------|
| `emit_ry(target, angle)` | `_gates.pyx:37` | Local diffusion D_x: Ry rotations for walk state amplitudes |
| `emit_h(target)` | `_gates.pyx:80` | Child state superposition in U_x preparation |
| `emit_x(target)` | `_gates.pyx:60` | Bit flips for phase marking, state prep |
| `emit_mcz(target, controls)` | `_gates.pyx:114` | Phase flip on accept/reject in D_x |
| `emit_p(target, angle)` / `emit_p_raw` | `_gates.pyx:183/159` | Phase rotations for QPE (if used) |
| CRy gate (C level) | `gate.h:39 cry()` | Controlled Ry for variable branching controlled on height register |
| CH gate (C level) | `gate.h:40 ch()` | Controlled Hadamard in psi_prep |
| MCX decomposition | `gate.h:46 mcx()` | Multi-controlled operations in walk operators |
| CX gate (C level) | `gate.h:44 cx()` | SWAP decomposition (3 CX = 1 SWAP), parity computation |
| `_allocate_qubit()` | `_core.pyx:949` | Ancilla allocation for height register, QPE |
| `_deallocate_qubits()` | `_core.pyx:967` | Ancilla cleanup after walk step |
| `@ql.compile` decorator | `compile.py` | Cache and replay walk operator circuits |
| `GroverOracle` pattern | `oracle.py` | Reusable compute-uncompute pattern for predicate |
| `_predicate_to_oracle()` | `oracle.py:162` | Lambda tracing for accept/reject predicates |
| `diffusion()` X-MCZ-X | `diffusion.py:77` | Reusable sub-pattern; D_x generalizes this |
| `_collect_qubits()` | `diffusion.py:21` | Extract physical qubit indices from registers |
| `amplitude_estimate()` | `amplitude_estimation.py:482` | Adaptable for walk-operator detection mode |
| `_build_and_simulate()` | `amplitude_estimation.py:337` | Circuit build + multi-shot simulation pattern |
| `circuit()`, `option()` | `_core.pyx` | Circuit creation and mode configuration |
| `to_openqasm()` | `openqasm.pyx` | Export for Qiskit verification |
| `load_qasm()`, `simulate()` | `sim_backend.py` | Verification simulation pipeline |
| Controlled context (`with qbool:`) | `qint.pyx` | Auto-derives CRy/CH from Ry/H when inside controlled block |

### New Primitives Needed (All Composable from Existing Gates)

| Primitive | Complexity | Built From | Purpose |
|-----------|-----------|------------|---------|
| **Tree state encoding** | Medium | `_allocate_qubit()`, qint for branch registers | One-hot height register + branch array encoding tree vertices |
| **Walk state prep U_x** | Medium | `emit_ry(phi)`, `emit_h`, controlled context | Prepare \|psi_x> = (d(x)+1)^{-1/2} (\|x> + sum \|child>) |
| **Local diffusion D_x** | Medium | U_x, U_x_dag, MCZ | D_x = U_x_dag . phase_flip . U_x (identity for marked vertices) |
| **R_A operator** | Low | D_x controlled on even-depth parity | Direct sum of D_x across even-parity depth vertices |
| **R_B operator** | Low | D_x controlled on odd-depth parity + root reflection | Direct sum of D_x across odd-parity depth plus \|r><r\| |
| **Walk step U = R_B R_A** | Low | R_A followed by R_B | One complete walk step |
| **Detection routine** | Medium | Walk step powers + threshold measurement (or IQAE) | Algorithm 1: does a solution exist? |

## Detailed Technical Analysis

### 1. Local Diffusion D_x (The Core Primitive)

**Mathematical definition:**
For non-marked vertex x with d(x) children, the walk state is:
```
|psi_x> = (1 / sqrt(d(x) + 1)) * (|x> + sum_{y: child of x} |y>)
```

The diffusion operator is:
- D_x = I - 2|psi_x><psi_x| for non-marked (neither accepted nor rejected) vertices
- D_x = I (identity) for accepted vertices
- D_x reflects away from children for rejected vertices (prunes branch)

**Circuit implementation pattern (validated against Qrisp reference):**
```
D_x = U_x_dag . phase_flip_on_|x> . U_x
```

Where U_x creates the walk state superposition from \|x>. The phase flip uses MCZ controlled on accept/reject qbools (existing `emit_mcz`).

**Rotation angle for walk state preparation:**
```python
import math

# For non-root vertex with branching factor deg (e.g., deg=2 for binary):
phi = 2 * math.atan(math.sqrt(deg))
# Binary tree: phi = 2 * atan(sqrt(2)) approx 1.9106 radians

# For root vertex (absorbs depth factor):
phi_root = 2 * math.atan(math.sqrt(deg * max_depth))
# Binary depth-3: phi_root = 2 * atan(sqrt(6)) approx 2.3562 radians
```

**Why Ry at this angle:** The Ry(phi) rotation on a qubit starting in \|0> produces:
```
cos(phi/2)|0> + sin(phi/2)|1>
```
With phi = 2*atan(sqrt(deg)):
```
cos(atan(sqrt(deg))) = 1/sqrt(deg+1)
sin(atan(sqrt(deg))) = sqrt(deg)/sqrt(deg+1)
```
This gives amplitude 1/sqrt(deg+1) for \|0> (parent) and sqrt(deg)/sqrt(deg+1) distributed across children, matching the walk state definition. For uniform branching, each child gets 1/sqrt(deg+1) after further splitting with Hadamard gates.

**Existing gate coverage:** emit_ry for the rotation, emit_h for child splitting, emit_mcz for phase flip. Complete.

### 2. Tree State Encoding

**Register layout:**
```
|vertex> = |branch_qa[0]>|branch_qa[1]>...|branch_qa[d-1]>|h[0]>|h[1]>...|h[d]>
```

- **Height register (h):** One-hot encoding with (max_depth + 1) qubits. Root has h = max_depth. Leaves have h = 0. One-hot simplifies even/odd parity selection to a single XOR on alternating height qubits.
- **Branch register (branch_qa):** Array of max_depth entries, each encoding which child was taken at that level. For binary tree: 1 qubit per level. For k-ary tree: ceil(log2(k)) qubits per level.

**Implementation approach:** Use raw `_allocate_qubit()` for the one-hot height register (these are individually controlled, not treated as an integer). Use `qint` width-1 registers for binary branch entries, or multi-bit qint for higher branching.

**Why one-hot for height:** Parity selection (even/odd depth) reduces to XOR on a subset of height qubits: `for i in range(max_depth + 1): if i % 2 == target_parity: CX(h[i], parity_qubit)`. This is O(n) CX gates, simpler than extracting parity from binary encoding.

### 3. R_A and R_B via Parity-Controlled Diffusion

**Key insight from Qrisp reference:** R_A and R_B are NOT explicit loops over all tree vertices. They are implemented as a single diffusion operation controlled on the height parity bit:

```python
def qstep_diffuser(self, even):
    # Compute parity of current height
    parity_qubit = _allocate_qubit()
    for i in range(max_depth + 1):
        if (i % 2 == 0) == even:
            emit_x_controlled(h[i], parity_qubit)  # CX

    # Apply local diffusion D_x controlled on parity
    with parity_qubit:  # controlled context
        apply_local_diffusion(...)  # U_x_dag . phase . U_x

    # Uncompute parity
    for i in range(max_depth + 1):
        if (i % 2 == 0) == even:
            emit_x_controlled(h[i], parity_qubit)  # CX (self-inverse)

    _deallocate_qubits(parity_qubit, 1)
```

The walk step is then:
```python
def quantum_step(self):
    self.qstep_diffuser(even=not self.max_depth % 2)  # R_A
    self.qstep_diffuser(even=self.max_depth % 2)       # R_B
```

**Why this works:** The tree state is in superposition. The controlled diffusion acts on each branch of the superposition independently. A vertex at even depth has its parity qubit in state \|1>, activating the diffusion. Vertices at odd depth have parity qubit \|0>, leaving them unchanged. This achieves the direct sum implicitly.

**Existing support:** The `with qbool:` controlled context in the framework auto-derives CRy from Ry, CH from H, etc. -- exactly what is needed for height-parity-controlled diffusion.

### 4. Detection Mode

**Montanaro Algorithm 1 (Detect):**
1. Prepare initial state \|r> (root vertex)
2. Apply walk step U = R_B R_A repeatedly O(sqrt(T * n)) times
3. Measure: if walk state has non-trivial component at eigenvalue != 1, a marked vertex exists

**Implementation options:**

| Approach | Qubit Overhead | Circuit Depth | Reuses Existing |
|----------|---------------|---------------|-----------------|
| **IQAE adaptation** | 0 extra (reuses data qubits) | Moderate (iterative) | Yes -- `amplitude_estimate()` pipeline |
| Full QPE | p ancilla qubits | Deep (controlled walk powers) | Partial -- needs inverse QFT |
| Repeated measurement | 0 extra | Variable | Yes -- simple multi-shot |

**Recommended: IQAE adaptation** because:
1. Already implemented and verified (`amplitude_estimation.py`)
2. No additional ancilla qubits (critical with 17-qubit limit)
3. QFT-free (no inverse QFT circuit to build)
4. The walk step R_B R_A substitutes for the Grover iterate in the IQAE power sequence

The adaptation replaces `oracle + diffusion` in `_build_and_simulate()` with `walk_step()`, while keeping the IQAE outer loop, confidence interval computation, and result reporting identical.

### 5. Predicate Integration (accept/reject)

**User API design:**
```python
def detect(accept, reject, *, max_depth, branching_factor=2, epsilon=0.01):
    """Detect if a solution exists in the backtracking tree.

    Parameters
    ----------
    accept : callable
        accept(node_state) -> qbool. True if node is a solution.
    reject : callable
        reject(node_state) -> qbool. True if subtree should be pruned.
    max_depth : int
        Maximum tree depth.
    branching_factor : int
        Children per node (default 2 for binary).
    """
```

**Predicate tracing:** Reuse the `_predicate_to_oracle` tracing mechanism from `oracle.py`. Call `accept(branch_registers)` and `reject(branch_registers)` with real qint objects. The framework's existing comparison operators capture gates automatically. The resulting qbools control the MCZ phase in the local diffusion.

**Constraint (from Qrisp docs):** Both accept and reject must:
- Not modify the tree state registers
- Uncompute all temporary qubits (ancilla delta = 0)
- Never both return True on the same node

These constraints match the existing `@ql.grover_oracle` validation (ancilla delta check, compute-phase-uncompute pattern).

## Qubit Budget Analysis (17-Qubit Simulator Limit)

| Component | Binary Depth 2 | Binary Depth 3 | Notes |
|-----------|----------------|-----------------|-------|
| Height register (one-hot) | 3 | 4 | max_depth + 1 qubits |
| Branch register | 2 | 3 | max_depth qubits (1 per level for binary) |
| Parity ancilla | 1 | 1 | Allocated/deallocated within walk step |
| Accept/reject ancilla (est.) | 2-3 | 2-3 | Depends on predicate complexity |
| **Total (detection only)** | **8-9** | **10-11** | Fits within 17-qubit limit |
| QPE ancillae (if used) | +3-4 | +3-4 | Only if full QPE approach is chosen |
| **Total (with QPE)** | **11-13** | **13-15** | Tight but feasible for depth 3 |

**Recommended demo target:** Binary tree, depth 2-3, simple SAT predicate (2-3 variables). This keeps total qubit count at 10-11, well within the simulator limit with margin for predicate ancillae.

## Installation

```bash
# No new packages needed. Everything required is already installed.
# No pip install, no npm install, no new dependencies.

# Build (unchanged):
pip install -e ".[dev,verification]"
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| One-hot height encoding | Binary height encoding | Binary uses fewer qubits (ceil(log2(n+1)) vs n+1) but requires complex parity extraction circuit. Use binary only if qubit count is extremely tight (depth > 4). One-hot matches Qrisp reference and simplifies parity to XOR |
| IQAE for detection | Full QPE circuit | QPE is the textbook Algorithm 1. Use QPE only if eigenvalue precision beyond IQAE capability is needed. For v6.0 demos, IQAE is practical and reuses existing code |
| Pure Python quantum_walk.py | C-level walk operators | C implementation only worthwhile if walk operator compilation becomes a bottleneck. Walk step is O(n) gates for depth n -- negligible compilation time |
| Controlled context for R_A/R_B | Explicit controlled gate emission | Controlled context (`with qbool:`) auto-derives CRy/CH from Ry/H, matching existing codebase patterns. Explicit emission only if controlled context has bugs |
| Single quantum_walk.py module | Separate files (tree.py, diffusion.py, detect.py) | Single module matches existing pattern (grover.py = 563 lines, amplitude_estimation.py = 633 lines). Split only if module exceeds ~800 lines |
| @ql.compile for walk step | No caching | Walk step is called O(sqrt(T)) times. Caching avoids re-generation. Only skip caching if walk step parameters vary per call |
| accept/reject callable pair | Single 3-valued predicate | Two callables is cleaner API (matches Qrisp), avoids ternary return value encoding. Single predicate only if user ergonomics demand it |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| New C-level gate types | Existing gate set (Ry, CRy, H, CH, X, CX, MCZ, MCX, P, CP) covers everything. Adding C gates increases binary size and maintenance for zero benefit | Compose existing C gates via Python-level emit_* functions |
| Native SWAP gate | Framework has no SWAP gate. Decompose as 3 CX if needed. Most "swaps" in the walk can be Python-level qubit reference reassignment (zero gates) | 3 CX gates (emit_x with control) or Python pointer swap |
| qint.branch() for walk state prep | branch() applies uniform Ry to ALL qubits in a register. Walk state prep needs targeted per-qubit rotations with angles depending on d(x) and vertex height | Direct emit_ry() calls at specific qubit indices with computed angles |
| Full QFT for QPE inverse | Existing C-level QFT (gate.h: QFT/QFT_inverse) operates on sequence_t, not compatible with Python-level circuit building. Also, QFT adds significant depth | Adapt IQAE (no QFT needed); or build inverse QFT from emit_h + emit_p if QPE is required later |
| NetworkX or graph libraries | Tree structure is implicit in the qubit encoding. No explicit graph data structure needed | Height register + branch array encodes tree implicitly in superposition |
| New Cython modules | No performance-critical loop in walk step compilation. Python-level composition is sufficient | Pure Python quantum_walk.py composing existing Cython emit_* functions |
| Qrisp-style iSWAP/XX+YY gates | These are Qrisp-specific decomposition choices. Our framework's Ry + H + controlled-context achieves the same walk state preparation without exotic gates | Standard Ry rotations + Hadamard + controlled context |
| External quantum walk libraries | No Python library provides composable walk primitives that integrate with our gate-emission model | Build from scratch using existing gate infrastructure |

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| Python 3.11+ | All existing Cython, C backend | No version change |
| NumPy (existing) | math.atan, math.sqrt | Standard math functions; no version sensitivity |
| SciPy (existing) | scipy.stats.beta | Already used by IQAE; no new scipy features needed |
| Qiskit 1.x | sim_backend.py | Detection verification uses same pipeline as Grover verification |
| qiskit-aer (existing) | AerSimulator | May benefit from statevector_simulator for walk state verification (already supported) |

## Confidence Assessment

| Area | Confidence | Source | Notes |
|------|------------|--------|-------|
| No new deps needed | HIGH | Codebase gate.h + _gates.pyx analysis | Every required gate already exists in C backend |
| Walk state Ry angle formula | HIGH | Qrisp source code + Montanaro paper | phi = 2*atan(sqrt(deg)) validated against reference implementation |
| One-hot height encoding | HIGH | Qrisp implementation, Martiel 2019 | Standard approach in all implementations surveyed |
| IQAE adaptation for detection | MEDIUM | Architectural analysis | Novel adaptation -- IQAE replaces Grover iterate with walk step. Mathematically sound but untested in this codebase |
| Qubit budget estimates | HIGH | Register layout analysis | Binary depth 2-3 at 10-11 qubits, well within 17-qubit limit |
| Predicate integration pattern | HIGH | oracle.py existing infrastructure | Same tracing mechanism as lambda predicate oracles |
| R_A/R_B via parity control | HIGH | Qrisp backtracking_tree.py source | Validated implementation pattern, matches theory |

## Sources

### Primary (HIGH confidence)
- [Montanaro 2015 - Quantum walk speedup of backtracking algorithms (arXiv:1509.02374)](https://arxiv.org/abs/1509.02374) -- foundational algorithm: D_x, R_A, R_B, Algorithm 1 (detect)
- [Theory of Computing published version (v014a015)](https://theoryofcomputing.org/articles/v014a015/) -- peer-reviewed version of Montanaro
- [Qrisp QuantumBacktrackingTree documentation](https://qrisp.eu/reference/Algorithms/QuantumBacktrackingTree.html) -- reference implementation API, one-hot encoding, quantum_step
- [Qrisp backtracking_tree.py source](https://github.com/eclipse-qrisp/Qrisp/blob/main/src/qrisp/algorithms/quantum_backtracking/backtracking_tree.py) -- implementation patterns: qstep_diffuser, psi_prep, rotation angles, MCZ phase marking

### Secondary (MEDIUM confidence)
- [Martiel 2019 - Practical implementation of a quantum backtracking algorithm (arXiv:1908.11291)](https://arxiv.org/pdf/1908.11291) -- circuit depth analysis, practical considerations
- [Qrisp Sudoku application (arXiv:2402.10060)](https://arxiv.org/html/2402.10060v1) -- qubit count formulas: O(n*log(d)) data qubits, 6n+14 CX per controlled diffuser
- [Quantum Search on Computation Trees (arXiv:2505.22405)](https://arxiv.org/html/2505.22405) -- generalized walk state with weights: D_x = I - 2|psi_x><psi_x|, R_A/R_B definitions

### Codebase Analysis (HIGH confidence -- direct code reading)
- `c_backend/include/gate.h` -- Full gate function signatures: ry, cry, h, ch, x, cx, mcz, mcx, p, cp
- `c_backend/include/types.h` -- Gate type enum: X, Y, Z, R, H, Rx, Ry, Rz, P, M, T_GATE, TDG_GATE
- `src/quantum_language/_gates.pyx` -- Python gate emission: emit_ry, emit_h, emit_x, emit_mcz, emit_p, emit_p_raw with auto-controlled context
- `src/quantum_language/grover.py` -- Walk API pattern reference: circuit creation, register allocation, iteration loop, Qiskit verification
- `src/quantum_language/diffusion.py` -- X-MCZ-X pattern: walk local diffusion generalizes this
- `src/quantum_language/oracle.py` -- Predicate tracing and oracle validation: reusable for accept/reject
- `src/quantum_language/amplitude_estimation.py` -- IQAE infrastructure: adaptable for walk-operator detection
- `src/quantum_language/_core.pyx` -- _allocate_qubit, _deallocate_qubits for ancilla management

---
*Stack research for: Quantum Assembly v6.0 -- Quantum Walk Primitives (Montanaro 2015 backtracking)*
*Researched: 2026-02-26*
*Conclusion: Zero new external dependencies. Zero new C-level gates. One new Python module (~400-600 lines) composing existing gate primitives into walk operators.*
