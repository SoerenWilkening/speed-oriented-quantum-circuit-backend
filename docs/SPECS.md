# Quantum Assembly — Project Specification

## 1. Overview

This document captures the specification for the next phase of development of the Quantum Assembly framework. Five task areas have been identified, with quantum game algorithms (Task 5) as the highest priority.

The framework compiles high-level Python code (arithmetic, comparisons, `with` blocks) into quantum circuits. Users never interact with gates directly. The goal is to extend this framework with a classical verification mode and use it to build quantum walk-based game solvers.

---

## 2. Task Areas

| Task | Area | Priority | Status |
|------|------|----------|--------|
| 1 | Code maintenance (speed, memory, stability, file sizes) | Normal | Not started |
| 2 | Codebase cleanup | Normal | Not started |
| 3 | Classical verification framework | High | Specification complete |
| 4 | Circuit compilation improvements | Normal | Not started |
| 5 | Quantum game algorithms (minimax) | **Highest** | Specification complete |

### Dependencies

- Tasks 3 and 5 can be developed **in parallel**.
- Task 3 is required before chess-scale verification, but tic-tac-toe predicates are simple enough to validate independently.
- Tic-tac-toe serves **double duty**: a Task 5 deliverable and a Task 3 validation benchmark.
- Tasks 3 and 4 are **independent** — call graphs do not play a role in classical verification.
- Tasks 1 and 2 are independent of all others.

---

## 3. Task 3: Classical Verification Framework

### 3.1 Purpose

Verify the logical correctness of quantum algorithms without quantum simulation. The 17-qubit simulation ceiling makes it impossible to simulate algorithms at meaningful scale (even tic-tac-toe exceeds it). Classical verification mode runs the same algorithm code on concrete classical values, sampling where superposition would occur.

### 3.2 Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Architecture | Framework-level global flag (Option C) | User code must not change between quantum and classical mode |
| Data model | `qint` holds a concrete Python `int` in classical mode | No qubit allocation, no ancilla, no gate emission |
| Sampling point | Before computation | Pick concrete values upfront, run classically |
| Superposition handling | Coin flip with correct probabilities at Hadamard/branching points | H on \|0> = 50/50; Ry(theta) = cos^2(theta/2) vs sin^2(theta/2) |
| Verification paradigm | Property-based testing | Check invariants, not exact outputs |
| Scope | Logical correctness only | Not qubit counts, ancilla management, or circuit structure |

### 3.3 Execution Model

A single classical verification run:

1. Enter classical mode (framework-level flag).
2. Initialize quantum registers — `qint` created with concrete integer values.
3. Execute algorithm code — all `qint` operations perform Python integer arithmetic.
4. At superposition points (Hadamard, branching), sample: coin flip with correct probabilities, assign a concrete value.
5. `with qbool:` becomes `if bool_value:` — executes or skips the block.
6. At the end, check properties/invariants on the result.
7. Repeat N times with different random samples for statistical confidence.

### 3.4 Implementation Requirements

#### 3.4.1 Global Mode Flag

- A flag accessible from `_core.pyx` (or equivalent) indicating classical mode is active.
- Activated via a public API, e.g., `ql.classical_mode()` context manager or `ql.set_mode('classical')`.

#### 3.4.2 `qint` Behavior in Classical Mode

- Holds a Python `int` value internally.
- Every operator (`__add__`, `__sub__`, `__mul__`, `__lt__`, `__eq__`, etc.) checks the global flag at the top.
- If classical mode: perform the integer operation, return a new `qint` wrapping the result. **Early return before any gate emission, qubit allocation, or ancilla management.**
- Width semantics preserved: results are masked/truncated to the `qint`'s bit width (e.g., 4-bit addition wraps at 16).

#### 3.4.3 `qbool` Behavior in Classical Mode

- Holds a Python `bool`.
- `__enter__` / `__exit__` (the `with` block): behaves as `if self.value:` — the body executes or is skipped entirely.
- `__and__`, `__or__`: Python boolean logic.

#### 3.4.4 `qarray` Behavior in Classical Mode

- Array of classical `int` / `bool` values.
- Indexing returns classical values.
- Element-wise operations are classical.

#### 3.4.5 Superposition Sampling

- When the framework would emit Hadamard gates (e.g., `ql.branch(qint)` or equivalent), classical mode replaces with random sampling:
  - H gate on |0>: 50/50 coin flip for 0 or 1.
  - Ry(theta): sample 0 with probability cos^2(theta/2), 1 with probability sin^2(theta/2).
  - Uniform superposition over n qubits: sample uniformly from [0, 2^n - 1].
- The `qint` is assigned the sampled concrete value and computation proceeds deterministically from there.

#### 3.4.6 Verification API

Property-based testing interface:

```python
# Pseudocode — exact API TBD
ql.verify(
    algorithm=chess_predicate,
    property=lambda board, result: is_valid_move(board, result),
    samples=1000
)
```

Or more informally: run in classical mode with assertions inside the algorithm code.

### 3.5 What Classical Mode Does NOT Do

- Does not generate any quantum circuit.
- Does not allocate qubits or ancilla.
- Does not verify circuit structure, gate counts, or depth.
- Does not simulate quantum interference or entanglement — it traces a single classical branch per run.

### 3.6 Validation of the Framework Itself

Tic-tac-toe serves as ground truth:

1. Implement tic-tac-toe predicates (legal move, win detection, move counting).
2. Run classical verification mode on them.
3. Independently verify results with a classical tic-tac-toe engine.
4. If both agree, the verification framework is trustworthy for chess-scale problems where independent verification isn't feasible.

---

## 4. Task 5: Quantum Game Algorithms

### 4.1 Goal

Build a quantum minimax solver for two-player perfect-information games (chess), achieving ~N^{1/2} query complexity over the game tree (vs classical N).

### 4.2 Theoretical Foundation

| Paper | Role |
|-------|------|
| Montanaro, arXiv:1509.02374 | Quantum backtracking algorithm — walk operator construction, tree search |
| Farhi/Gutmann, arXiv:0710.5794 | Quantum algorithms for evaluating min-max trees — N^{1/2+o(1)} complexity |
| (Bridging literature) | Connecting quantum backtracking to two-player games to full minimax evaluation |

The implementation path follows: **quantum backtracking → quantum two-player game trees → quantum minimax**.

### 4.3 Tic-Tac-Toe (Stepping Stone)

#### 4.3.1 Purpose

- End-to-end prototype of the full pipeline before scaling to chess.
- Validation benchmark for the classical verification framework (Task 3).
- Predicates are simple enough to verify exhaustively by classical means.

#### 4.3.2 Board Encoding

- 9 cells, each in one of 3 states: empty, X, O.
- 2 qubits per cell → **18 qubits minimum** for the board alone.
- Exceeds the 17-qubit simulation limit even before move/height/count registers.
- **Late-game states** (6-7 cells filled, 2-3 empty) may fit within 17 qubits for actual quantum simulation of the walk at those levels.

#### 4.3.3 Predicates

- **Cell empty**: `board[i] == EMPTY`
- **Correct turn**: inferred from count of X's and O's
- **Game not over**: no three-in-a-row exists for either player
- **Win detection**: three-in-a-row check for X or O
- **Legal move count**: number of empty cells (in non-terminal state)

### 4.4 Chess (End Goal)

- Board: 8x8 with piece type encoding (exact encoding TBD).
- Up to ~218 legal moves per position (theoretical maximum).
- Requires classical verification mode — far beyond simulation limits.
- Piece movement rules, check detection, castling, en passant, etc.

### 4.5 Walk Operator Design

**Build from scratch.** The existing `QWalkTree` implementation is not trusted for correctness.

#### 4.5.1 Key Design Decision: Diffusion Over Valid Children Only

The walk operator diffuses only over valid (legal) children, not over all possible children with invalid ones marked. This is the principled approach for correct quantum speedup.

#### 4.5.2 Walk Operator Steps

For a single application of the walk operator at a given tree node:

1. **Enumerate candidate moves**: Generate all possible moves (up to a fixed maximum) in a move register.
2. **Evaluate legality**: Apply the legality predicate to each candidate move, producing a validity flag (qbool) per move.
3. **Count valid moves**: Sum the validity flags into a quantum integer `k`.
4. **Conditional diffusion**: For each possible value of `k`, apply a rotation parameterized by `k`:
   ```python
   for k_val in range(1, max_moves + 1):
       with (count == k_val):
           # Apply Ry(angle(k_val)) — uniform diffusion over k_val children
   ```
5. **Uncompute**: Reverse steps 3, 2, 1 (uncompute count, validity flags, move enumeration).

#### 4.5.3 Complexity Notes

- The conditional diffusion (step 4) iterates over all possible values of `k`. For tic-tac-toe, k ranges 0-9. For chess, k ranges 0-218.
- This does not change asymptotic complexity: move enumeration (step 1-2) is already O(d_max), so O(218) iterations in step 4 are the same order.
- **Focus on correctness first, optimization later.**

#### 4.5.4 Conditional Rotation Construction

The diffusion for `k` valid children requires a rotation angle that depends on `k`:

- Uniform diffusion over `k` states: the reflection operator has angle `2/k` (or `2*arcsin(1/sqrt(k))` depending on formulation).
- Implemented as a sequence of `with (count == k_val): Ry(angle(k_val))` blocks.
- Each block is a controlled rotation — controlled on the count register equaling a specific value.

### 4.6 Full Pipeline Components

| Component | Description | Needed For |
|-----------|-------------|------------|
| Game state encoding | Board as qarrays, piece positions | Both games |
| Move generation | Legal move enumeration as predicates | Both games |
| Move legality predicate | "Is this cell empty / is this piece move legal" | Both games |
| Valid move counting | Sum of legality flags → quantum integer k | Walk operator |
| Conditional diffusion | Rotation parameterized by k | Walk operator |
| Walk operator (R_A, R_B) | Composing enumeration + legality + counting + diffusion | Tree search |
| Evaluation predicate | Terminal node detection (win/checkmate/draw) | Minimax |
| Min/max alternation | Track whose turn it is, propagate values up the tree | Minimax |
| Height/depth tracking | Current depth in the game tree | Walk structure |

### 4.7 Verification Strategy for Walk Operators

Two complementary approaches:

1. **Classical verification mode** (Task 3): Run walk operator code classically on sampled inputs, check properties (no illegal moves, correct counts, correct board states after branching).
2. **Small-instance quantum simulation**: For late-game tic-tac-toe or tiny custom graphs, verify walk operators via standard Qiskit simulation within the 17-qubit limit.

---

## 5. Task 1: Code Maintenance

Improve the existing codebase in terms of:

- **Speed**: Optimize hot paths, reduce overhead in gate emission and circuit construction.
- **Memory**: Reduce memory footprint of circuit representation.
- **Stability**: Fix edge cases, improve error handling at system boundaries.
- **File sizes**: Reduce bloat (e.g., the 100+ generated sequence files in `c_backend/src/sequences/`).
- **Functionality**: Extend supported operations where gaps exist.

No detailed specification yet — to be refined based on profiling and usage patterns.

---

## 6. Task 2: Codebase Cleanup

General cleanup of the codebase:

- Remove dead code and unused modules.
- Consolidate duplicated logic.
- Improve module organization.
- Clean up the `.planning/` directory (large number of deleted planning files in git status).
- Standardize naming conventions across Python and C layers.

No detailed specification yet.

---

## 7. Task 4: Circuit Compilation Improvements

### 7.1 Problem

Currently, full quantum circuits are stored in memory. For large algorithms, this is prohibitive. Gates are meant to be applied to a QPU, not stored.

### 7.2 Direction

- Use **call graphs** to store instructions at a higher level of abstraction, rather than storing every individual gate.
- The `@ql.compile` decorator and `CallGraphDAG` are partial implementations of this idea but are not complete or fully working.
- Optimization flags (`opt=0`, `opt=1`, `opt=2`) exist but are incomplete.

### 7.3 Independence

This task is independent of classical verification (Task 3). Call graphs may incidentally help classical verification, but this is not a design goal.

No detailed specification yet — to be refined based on Task 5 requirements.

---

## 8. Open Questions

To be revisited during implementation:

1. **Move register encoding**: Fixed-size slot list per candidate move, or a more compact representation?
2. **Exact diffusion operator**: Precise rotation angles and construction for uniform diffusion over k states.
3. **Game termination in the walk tree**: How terminal nodes (wins, draws) interact with the walk operator.
4. **Min/max alternation**: How to track and propagate player turns and evaluations in the quantum walk.
5. **Tic-tac-toe cell encoding**: Optimal encoding for 3-state cells (empty/X/O) — 2 qubits with one unused state, or alternative.
6. **Chess piece encoding**: Bit-width and layout for encoding piece types and positions.
7. **Classical verification API**: Exact public API design (`ql.verify()`, context manager, or assertion-based).
8. **Performance of classical mode**: Whether the flag check at the top of every `qint` operation introduces measurable overhead in quantum mode.
