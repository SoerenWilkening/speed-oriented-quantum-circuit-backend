# Feature Landscape: Toffoli-Based Fault-Tolerant Arithmetic

**Domain:** Quantum arithmetic circuit generation (Toffoli/Clifford+T gate set)
**Researched:** 2026-02-14 (updated with comprehensive codebase analysis)
**Focus:** New features for Toffoli-based arithmetic alongside existing QFT-based arithmetic

## Table Stakes

Features that are mandatory for a usable Toffoli-based arithmetic backend. Without these, the `ql.option('fault_tolerant', True)` switch is incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Ripple-carry adder (in-place, QQ) | Foundation for all other operations; simplest correct Toffoli adder | Medium | Cuccaro/CDKM adder: 2n-1 Toffoli, 5n-3 CNOT, 1 ancilla. O(n) depth. Must match existing `QQ_add` interface: target[0..n-1], other[n..2n-1]. |
| Ripple-carry adder (in-place, CQ) | Classical+quantum add used constantly (e.g., `a += 5`) | Medium | Conditional NOT cascade based on classical bits + carry propagation. Fewer gates since one operand is classical (known bits skip Toffoli gates). |
| Controlled ripple-carry adder (cQQ) | Required for `with cond:` blocks and multiplication subroutines | High | cQQ_add needs extra control qubit. Each Toffoli becomes a 3-controlled gate (decomposed via ancilla or directly via mcx). Layout: target[0..n-1], other[n..2n-1], control[2n]. |
| Controlled CQ adder (cCQ) | Required for conditional classical addition inside `with` blocks | Medium | Same as CQ but every gate conditioned on control qubit at [n]. |
| Subtraction via inverse adder | Users write `a -= b`; subtraction = run adder circuit in reverse | Low | Run the adder sequence inverted (already supported via `run_instruction(seq, qa, invert=1, circ)`). For Toffoli gates (self-inverse) and CNOT (self-inverse), reversal of gate order suffices. |
| `ql.option('fault_tolerant', True/False)` dispatch | User expects same API, different backend | Medium | Extend existing `option()` in `_core.pyx`. Hot-path functions (`hot_path_add_qq`, etc.) check flag and dispatch to Toffoli or QFT sequence generators. |
| Schoolbook multiplication (QQ) | Users write `c = a * b`; must produce correct product | High | n^2 + 4n + 3 Toffoli gates for n-bit multiply (Litinski 2024). Uses controlled add-subtract as core subroutine. Layout: result[0..n-1], a[n..2n-1], b[2n..3n-1]. |
| Classical-quantum multiplication (CQ) | Users write `c = a * 5`; common pattern | Medium | Shift-and-add: for each set bit in classical value, add shifted copy to result. Simpler than QQ since no controlled-add-subtract needed. |
| Restoring division (classical divisor) | Users write `q = a // 5` and `r = a % 5` | High | Already implemented at Python level using comparisons and conditional subtraction. Toffoli version: same Python-level algorithm, Toffoli adders/subtractors underneath. T-count: 35n^2 - 28n per Thapliyal. |
| Restoring division (quantum divisor) | Users write `q = a // b` | Very High | Already works via Python loop calling `>=` and conditional subtract. With Toffoli adders, same algorithm produces Toffoli division circuits automatically. |
| Ancilla management for Toffoli circuits | Toffoli circuits need ancilla qubits (ripple-carry: 1, CLA: 2n-2) | Medium | Existing `qubit_allocator` supports `allocator_alloc(circ->allocator, width, is_ancilla=true)`. Must allocate ancilla at sequence generation time and free after uncomputation. |
| Comparison operations (>=, ==, <) via Toffoli | Comparisons used in division, conditionals | Medium | Subtraction-based: compute a-b via Toffoli subtractor, check MSB for sign. `CQ_equal_width` already uses Toffoli/mcx gates. |
| Verification against QFT results | Correctness guarantee for all operations | Medium | Compare Toffoli circuit output vs QFT circuit output for all widths 1-8, all operation types. Existing test infrastructure in `tests/python/`. |

## Differentiators

Features that set the implementation apart. Not strictly required for correctness but provide significant value.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Carry look-ahead adder (QCLA) | O(log n) depth vs O(n) for ripple-carry; significant speedup for large widths | Very High | Draper et al. 2004: 5n-6 Toffoli, 3n CNOT, 2n-2 ancilla. Generate/propagate tree via Brent-Kung structure. Complex but transformative for depth-critical applications. |
| Controlled add-subtract circuit | Core innovation from Litinski 2024; n-1 Toffoli instead of 2n-1 for controlled adder | High | Adds when control=1, subtracts when control=0. Saves ~50% Toffolis in multiplication. Achieved by wrapping adder with multi-target CNOT gates controlled by the add/subtract selector. |
| Automatic depth/ancilla tradeoff | System chooses ripple-carry vs CLA based on register width | Medium | For n <= 8, ripple-carry is competitive. For n >= 16, CLA depth advantage matters. Could expose via `ql.option('adder_strategy', 'auto'/'ripple'/'cla')`. |
| Controlled multiplication (cQQ, cCQ) | Needed for nested conditionals: `with cond: c = a * b` | Very High | Current QFT version exists (`cQQ_mul`, `cCQ_mul`). Toffoli version: wrap schoolbook multiply with control qubit on all gates. |
| Hardcoded Toffoli sequences (widths 1-8) | Eliminate generation overhead for common cases | Medium | Follow existing pattern from `c_backend/src/sequences/add_seq_*.c`. Pre-generate via script, compile into binary. |
| T-count reporting in circuit statistics | For fault-tolerant QC, T-gate count is THE cost metric | Low | `circuit_stats.h` already tracks `ccx_gates`. Each Toffoli = 7 T gates (standard decomposition). Add computed `t_count` field. |
| Hot path C implementation | Performance parity with QFT hot paths | Medium | Follow Phase 60 pattern exactly (`hot_path_add.c`, `hot_path_mul.c`). Check `fault_tolerant` flag and dispatch. |
| Mixed-width Toffoli arithmetic | Handle `a(4-bit) + b(8-bit)` correctly | Medium | Zero-extend shorter operand. More explicit than QFT (no Fourier domain). Existing QFT code handles width mismatch; Toffoli must match. |
| Comparative gate count reporting | Users compare QFT vs Toffoli resource costs | Low | Show both gate counts when mode switches. Useful for papers and benchmarking. |

## Anti-Features

Features to explicitly NOT build in this milestone.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Karatsuba/Toom-Cook multiplication | Asymptotically better (O(n^1.58) vs O(n^2)) but worse for practical sizes n <= 64. Litinski 2024 confirms schoolbook wins for small n. | Use schoolbook multiplication. Karatsuba only helps for n > ~100, far beyond the 1-64 bit range this framework targets. |
| Non-restoring division circuit | More complex circuit design with marginal T-count improvement (~15% per Thapliyal). | Use restoring division. Simpler to implement, already matches existing Python-level algorithm structure exactly. |
| QFT-Toffoli hybrid circuits | Mixing QFT and Toffoli gates in the same arithmetic operation creates debugging and optimization nightmares. | Keep QFT and Toffoli as separate, switchable backends. One flag controls which backend all arithmetic uses. |
| Measurement-based uncomputation (Gidney's logical-AND trick) | Requires mid-circuit measurement + classical feedback. Current circuit model has no measurement gate in the execution path. | Defer to future milestone. The T-count halving (4n vs 8n for adder) is significant but requires infrastructure changes. |
| Modular arithmetic (mod 2^n - 1) | Specialized adder variant from Draper paper for modular exponentiation. | Not needed for general-purpose integer arithmetic. Add later if Shor's algorithm becomes a goal. |
| Automatic Toffoli decomposition of arbitrary multi-controlled gates | Decomposing n-controlled gates into Toffoli + ancilla automatically. | Use explicit circuit constructions. `mcx()` in `gate.h` handles multi-controlled X at the gate level. Decomposition is transpiler concern. |
| Floating-point Toffoli arithmetic | IEEE 754 quantum floating point. | Extreme complexity for marginal user value. Integer arithmetic covers target use cases. |
| Out-of-place addition | More qubits, different interface than existing API. | All operations modify target register in-place, matching existing QFT conventions. |
| Toffoli decomposition into Clifford+T | Backend compiler concern, not circuit generation concern. | Export Toffoli gates as-is. Hardware compilers (Qiskit transpiler etc.) handle T-gate decomposition. |

## Feature Dependencies

```
Ripple-carry adder (QQ_add_toffoli)
  |
  +---> Subtraction (inverse of adder)
  |       |
  |       +---> Comparison (>=, <) via subtraction + MSB check
  |       |       |
  |       |       +---> Restoring division (comparison + conditional subtract loop)
  |       |
  |       +---> Controlled add-subtract (Litinski technique) [DIFFERENTIATOR]
  |               |
  |               +---> Schoolbook multiplication (iterates add-subtract over b's bits)
  |
  +---> Controlled adder (cQQ_add_toffoli)
  |       |
  |       +---> Controlled multiplication (cQQ_mul_toffoli) [DIFFERENTIATOR]
  |
  +---> CQ adder (CQ_add_toffoli)
  |       |
  |       +---> CQ multiplication (shift-and-add with classical value)
  |       |
  |       +---> Classical divisor path (restoring div with CQ operations)
  |
  +---> cCQ adder (cCQ_add_toffoli)
          |
          +---> Controlled CQ multiplication

ql.option('fault_tolerant') dispatch (independent, can be built early)
  |
  +---> hot_path_add.c modification (check flag, dispatch to Toffoli or QFT)
  +---> hot_path_mul.c modification (check flag, dispatch to Toffoli or QFT)
  +---> Division/Modulo: automatic (uses adder/comparator primitives from Python)

Carry look-ahead adder [DIFFERENTIATOR] (independent upgrade, same API)
  |
  +---> Can replace ripple-carry at dispatch level
  +---> Requires more ancilla (2n-2 vs 1) -- allocator handles this

Verification tests (built alongside each algorithm)
  |
  +---> Compare Toffoli output vs QFT output for widths 1-8
```

## MVP Recommendation

Prioritize (in order of implementation):

1. **Ripple-carry adder (all 4 variants: QQ, CQ, cQQ, cCQ)** -- Foundation for everything else. Cuccaro adder is well-understood (2n-1 Toffoli, 5n-3 CNOT, 1 ancilla). All four variants mirror existing QFT variants exactly in qubit layout. Start with QQ, then CQ, then cQQ, then cCQ.

2. **`ql.option('fault_tolerant')` dispatch mechanism** -- Extend existing `option()` in `_core.pyx`. Add flag check in `hot_path_add.c` and `hot_path_mul.c` to call `QQ_add_toffoli()` vs `QQ_add()`. Minimal infrastructure change, enables user-facing switching.

3. **Subtraction** -- Already works via `run_instruction(seq, qa, invert=1, circ)`. Verify that reversing gate order on a Toffoli-based adder produces correct subtraction. Should work because Toffoli and CNOT are self-inverse (only gate order matters).

4. **Schoolbook multiplication (QQ, CQ, cQQ, cCQ)** -- Uses adder as subroutine. With controlled add-subtract (Litinski), achieves n^2 + 4n + 3 Toffoli count for QQ. Must match existing qubit layouts.

5. **Restoring division and modulo** -- Already implemented at Python level in `qint_division.pxi`. With Toffoli adders underneath, the same Python code produces Toffoli circuits without algorithmic changes.

Defer:
- **CLA adder**: Complex (generate/propagate tree, 2n-2 ancilla). Ripple-carry is sufficient for 1-64 bit range.
- **T-count optimization (Gidney's trick)**: Requires mid-circuit measurement infrastructure.
- **Hardcoded sequences**: Add after dynamic generation is working, following `scripts/generate_seq_*.py` pattern.
- **Hot path C implementation**: Follow Phase 60 pattern after algorithms proven correct.

## Detailed Feature Specifications

### Ripple-Carry Adder (Cuccaro/CDKM, 2004)

**Algorithm**: In-place addition |a> + |b> -> |a+b> using carry propagation via MAJ (majority) and UMA (unmajority-and-add) gate sequences.

**Steps** (for n-bit addition):
1. **MAJ chain (forward carry propagation)**: For i = 0 to n-1, compute majority of (carry_in, a[i], b[i]). This propagates carries upward. Each MAJ gate = 1 Toffoli + 2 CNOT.
2. **High carry extraction**: The final carry is in the ancilla qubit (overflow bit).
3. **UMA chain (reverse, compute sum)**: For i = n-1 down to 0, compute sum bit and undo carry computation. Each UMA gate = 1 Toffoli + 2 CNOT.

**Resources (n-bit)**:
- Toffoli gates: 2n - 1
- CNOT gates: 5n - 3
- NOT gates: 2n - 4
- Ancilla qubits: 1 (single carry bit)
- Depth: O(n) -- linear, sequential carry propagation

**Qubit layout** (must match existing QQ_add):
- [0, n-1]: Target register a (modified in-place to a+b)
- [n, 2n-1]: Source register b (unchanged after operation)
- [2n]: Ancilla carry qubit (initialized to |0>, returned to |0>)

**Subtraction**: Run the same circuit in reverse gate order. Since Toffoli and CNOT are self-inverse, reversing the sequence computes |a-b>. The existing `run_instruction` with `invert=1` handles this.

**Confidence**: HIGH -- Cuccaro paper is well-cited (1000+ citations), algorithm is standard in quantum computing textbooks.

### CQ Adder (Classical + Quantum, Toffoli-based)

**Algorithm**: Add classical value to quantum register using conditional gates.

**Key optimization**: When one operand is classical, Toffoli gates can be simplified. If classical bit b[i] = 0, the Toffoli gate for that position reduces to a CNOT (carry propagation only, no new carry generation). If b[i] = 1, the full MAJ/UMA logic is needed.

**Resources (n-bit, for classical value with k set bits)**:
- Toffoli gates: 2k - 1 (only at positions where classical bit = 1)
- CNOT gates: 5n - 3 (carry still propagates through all positions)
- Ancilla: 1

**Qubit layout** (must match existing CQ_add):
- [0, n-1]: Target register (modified in-place)
- Classical value passed as parameter (no quantum register)

### Controlled Variants (cQQ, cCQ)

**Challenge**: Each Toffoli gate in the base adder becomes a doubly-controlled Toffoli (CCT or C3X) when an external control qubit is added. This requires decomposition:

**Option A**: Use `mcx()` directly (3-controlled X). The existing `mcx()` in `gate.h` supports arbitrary numbers of controls. The optimizer or hardware compiler handles decomposition.

**Option B**: Decompose each controlled-Toffoli into 2 Toffoli + 1 ancilla using the standard decomposition: CCT(a,b,c,target) = Toff(a,b,anc) + Toff(anc,c,target) + Toff(a,b,anc)^dag.

**Recommendation**: Option A for simplicity, matching the existing pattern where `cQQ_add` in QFT uses CCP gates that decompose to CP + CNOT combinations.

**Qubit layout cQQ** (must match existing):
- [0, n-1]: Target register
- [n, 2n-1]: Source register
- [2n]: Control qubit

### Carry Look-Ahead Adder (Draper et al. 2004) [DIFFERENTIATOR]

**Algorithm**: Parallel carry computation using generate/propagate prefix tree.
- Generate: G_i = a_i AND b_i (carry generated at position i)
- Propagate: P_i = a_i XOR b_i (carry propagated through position i)
- Group generate: G_{i:j} = G_i OR (P_i AND G_{j})
- Group propagate: P_{i:j} = P_i AND P_j
- Carry c_i computed via Brent-Kung parallel prefix tree in O(log n) rounds

**Phases of circuit**:
1. Compute single-bit generate and propagate (Toffoli + CNOT, 1 round)
2. P-round (parallel prefix tree, O(log n) rounds of Toffoli gates)
3. G-round (compute group generates using prefix tree results)
4. Compute sum bits from carries (CNOT, 1 round)
5. Uncompute ancilla (reverse of phases 1-3)

**Resources (n-bit)**:
- Toffoli gates: 5n - 6
- CNOT gates: ~3n
- Ancilla qubits: 2n - 2 (n-2 for propagate bits + n for carry string)
- Depth: O(log n)

**Confidence**: HIGH on Toffoli count (from paper abstract), MEDIUM on exact CNOT count.

### Schoolbook Multiplication (Litinski 2024)

**Algorithm**: Standard long multiplication using controlled add-subtract circuits.

For each bit b_j of operand b (j = 0 to n-1):
1. Left-shift operand a by j positions (implicit via qubit addressing)
2. Perform controlled-add-subtract of shifted a into result, controlled by b_j

**Controlled add-subtract** (core innovation):
- When control = |1>: adds a to result
- When control = |0>: subtracts a from result
- Implementation: Multi-target CNOT gates (controlled by selector qubit) before and after a standard adder flip the input bits, effectively negating the addend
- Cost: n - 1 Toffoli gates per controlled add-subtract (vs 2n - 1 for standard controlled adder)

**Resources (n-bit x n-bit -> n-bit product)**:
- Toffoli gates: n^2 + 4n + 3 (with add-subtract optimization)
- Without optimization: 2n^2 + n Toffoli (standard controlled adders)
- Savings: ~50% Toffoli reduction for large n
- Ancilla qubits: O(n) for adder carry bits

**Qubit layout** (must match existing QQ_mul):
- [0, n-1]: Result register (accumulates partial products)
- [n, 2n-1]: Operand a
- [2n, 3n-1]: Operand b (each bit used as control)

**Confidence**: HIGH -- 2024 paper, verified gate counts from HTML version of paper.

### Restoring Division

**Algorithm**: Classical restoring division mapped to quantum circuits.

For each bit position i from MSB to LSB:
1. Left-shift remainder (implicit via qubit addressing)
2. Trial subtraction: remainder -= divisor << i
3. Check sign (MSB of result = borrow out of subtractor)
4. If negative (MSB = 1): restore by adding back, quotient bit = 0
5. If non-negative (MSB = 0): keep result, quotient bit = 1

**Resources (n-bit, classical divisor)**:
- T-count: 35n^2 - 28n (Thapliyal)
- Uses n iterations, each containing one subtractor + conditional adder
- Ancilla: n qubits for remainder register

**Existing implementation compatibility**: The Python-level code in `qint_division.pxi` already implements restoring division using `>=` comparison and conditional subtraction via `with can_subtract:`. With Toffoli-based adders/subtractors underneath, this Python code produces Toffoli-based division circuits without modification. This is a critical advantage -- no new division algorithm code is needed, only new adder primitives.

**Confidence**: MEDIUM on exact T-count (from Thapliyal paper), HIGH on algorithm structure matching existing implementation.

## Resource Comparison: QFT vs Toffoli

### Addition (n-bit)

| Metric | QFT-based (current) | Toffoli Ripple-Carry | Toffoli CLA |
|--------|---------------------|---------------------|-------------|
| Abstract gate count | ~5n (H + CP + P) | ~7n (Toffoli + CNOT) | ~8n (Toffoli + CNOT) |
| Depth | O(n) | O(n) | O(log n) |
| Ancilla qubits | 0 | 1 | 2n - 2 |
| Fault-tolerant T-count | O(n^2 log n) * | O(14n) ** | O(35n) ** |
| Precision | Exact | Exact | Exact |

### Multiplication (n-bit)

| Metric | QFT-based (current) | Toffoli Schoolbook | Toffoli Schoolbook + CAS |
|--------|---------------------|-------------------|--------------------------|
| Gate count (abstract) | O(n^2) CP + H | 2n^2 + n Toffoli | n^2 + 4n + 3 Toffoli |
| Depth | O(n^2) | O(n^2) | O(n^2) |
| FT T-count | O(n^3 log n) * | O(14n^2 + 7n) | O(7n^2 + 28n + 21) |

\* Each small-angle CP(pi/2^k) requires O(3k) T-gates via synthesis (Ross-Selinger). For n-bit QFT, there are O(n^2) rotation gates with angles up to pi/2^n.

\** Each Toffoli = 7 T-gates in standard Clifford+T decomposition.

**The critical insight**: QFT adders appear cheaper in abstract gate count, but each small-angle rotation requires expensive synthesis in fault-tolerant architectures. For n = 8 bit addition, QFT costs approximately 400+ T-gates (from rotation synthesis) while Toffoli ripple-carry costs ~105 T-gates (15 Toffoli x 7 T). This is why Toffoli-based circuits dominate in fault-tolerant quantum computing.

## Infrastructure Dependencies on Existing Codebase

| Existing Component | How Toffoli Arithmetic Uses It | Modification Needed |
|--------------------|-------------------------------|---------------------|
| `gate.h`: `ccx()`, `cx()`, `x()` | Core gate primitives for Toffoli circuits | None -- already exists |
| `gate.h`: `mcx()` | Multi-controlled X for controlled variants (cQQ) | None -- already exists |
| `types.h`: `sequence_t`, `gate_t` | Sequence representation for generated circuits | None -- same structure |
| `execution.c`: `run_instruction()` | Apply sequence with qubit mapping + inversion | None -- inversion works for self-inverse gates |
| `hot_path_add.c` / `hot_path_mul.c` | Dispatch to correct sequence generator | Add `fault_tolerant` flag check |
| `qubit_allocator.c` | Allocate ancilla qubits for carry bits | None -- `allocator_alloc(circ, width, is_ancilla)` exists |
| `_core.pyx`: `option()` | User-facing option for switching backends | Add `'fault_tolerant'` key |
| `qint_arithmetic.pxi` | Operator overloading (`+`, `-`, `*`) | None -- dispatches through hot_path |
| `qint_division.pxi` | Division/modulo algorithms | None -- uses `+=`, `-=`, `>=` which route through adder |
| `circuit_stats.h` | Gate counting | Add T-count field |
| `sequences/` directory | Hardcoded sequences | Add `toffoli_sequences/` (later phase) |

## Expected Behavior Specifications

### Addition (`a += b` or `a + b`)

**User code** (unchanged):
```python
ql.option('fault_tolerant', True)
a = ql.qint(5, width=4)
b = ql.qint(3, width=4)
c = a + b  # Uses Toffoli-based addition now
```

**Circuit behavior**:
- Out-of-place `a + b`: allocates result register, copies `a` via XOR, adds `b` in-place
- In-place `a += b`: adds `b` directly into `a`'s register
- Subtraction `a -= b`: same circuit, run in reverse
- With classical: `a += 5` uses CQ variant (optimized, fewer Toffolis)
- Inside `with cond:` block: uses cQQ or cCQ variant automatically

### Multiplication (`a * b`)

**User code** (unchanged):
```python
ql.option('fault_tolerant', True)
a = ql.qint(3, width=4)
b = ql.qint(4, width=4)
c = a * b  # Uses Toffoli-based schoolbook multiplication
```

**Circuit behavior**:
- Allocates fresh result register (all zeros)
- For each bit of `b`, performs controlled-add-subtract of `a` into result
- Result contains product mod 2^n (same modular semantics as QFT version)

### Division (`a // b`, `a % b`)

**User code** (unchanged):
```python
ql.option('fault_tolerant', True)
a = ql.qint(17, width=8)
q = a // 5   # quotient
r = a % 5    # remainder
```

**Circuit behavior**:
- Uses the existing Python-level restoring division loop
- Each `>=` comparison produces a Toffoli-based comparator circuit
- Each conditional subtraction produces a controlled Toffoli subtractor
- Net effect: same algorithm, Toffoli gates instead of QFT rotations

## Sources

### HIGH Confidence (Academic Papers, Multiple Corroborating Sources)

- [Draper et al. 2004 - A logarithmic-depth quantum carry-lookahead adder](https://arxiv.org/abs/quant-ph/0406142) -- QCLA: 5n-6 Toffoli, O(log n) depth, 2n-2 ancilla
- [Cuccaro et al. 2004 - A new quantum ripple-carry addition circuit](https://arxiv.org/abs/quant-ph/0410184) -- Ripple-carry: 2n-1 Toffoli, 5n-3 CNOT, 1 ancilla, O(n) depth
- [Litinski 2024 - Quantum schoolbook multiplication with fewer Toffoli gates](https://arxiv.org/abs/2410.00899) -- n^2+4n+3 Toffoli, controlled add-subtract technique
- [Gidney 2018 - Halving the cost of quantum addition](https://arxiv.org/abs/1709.06648) -- 4n+O(1) T-gates via temporary logical-AND (deferred feature)

### MEDIUM Confidence (Single Academic Source, Verified Claims)

- [Thapliyal et al. - Quantum Circuit Design of Integer Division](https://arxiv.org/abs/1609.01241) -- Restoring/non-restoring division, 35n^2-28n T-count
- [Comprehensive Study of Quantum Arithmetic Circuits (2024)](https://arxiv.org/html/2406.03867v1) -- Survey comparing adder/multiplier/divider designs

### Project-Internal Sources (Verified via Codebase)

- `c_backend/src/IntegerAddition.c` -- Existing QFT adder variants (QQ, CQ, cQQ, cCQ) with qubit layouts
- `c_backend/src/IntegerMultiplication.c` -- Existing QFT multiplication (QQ, CQ, cQQ, cCQ)
- `src/quantum_language/qint_division.pxi` -- Existing restoring division at Python level
- `c_backend/src/hot_path_add.c` -- Hot path dispatch for addition (QQ and CQ paths)
- `c_backend/src/hot_path_mul.c` -- Hot path dispatch for multiplication
- `c_backend/src/gate.c` -- Gate primitives including `ccx()`, `mcx()`, `cx()`, `x()`
- `c_backend/src/qubit_allocator.c` -- Ancilla allocation via `allocator_alloc()`
- `c_backend/src/execution.c` -- `run_instruction()` with inversion support
- `src/quantum_language/_core.pyx` -- `option()` function for runtime configuration
- `c_backend/include/types.h` -- `sequence_t`, `gate_t` structures
