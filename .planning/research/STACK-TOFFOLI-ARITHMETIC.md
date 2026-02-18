# Technology Stack: Toffoli-Based Arithmetic

**Project:** Quantum Assembly -- Toffoli-Based Arithmetic Milestone
**Researched:** 2026-02-14
**Overall Confidence:** HIGH (grounded in existing codebase analysis + published quantum circuit literature)

## Executive Summary

The existing Quantum Assembly framework already has all the gate-level infrastructure needed for Toffoli-based arithmetic. The `ccx()` (Toffoli), `cx()` (CNOT), and `x()` (NOT) functions in `gate.c` are the exact gate primitives used by carry look-ahead adders, schoolbook multipliers, and restoring dividers. No new gate types are required. The work is entirely about implementing new `sequence_t*`-returning C functions, managing ancilla qubits through the existing `allocator_alloc()` mechanism, and wiring them through the existing `run_instruction()` + `hot_path_*` pipeline.

## Recommended Stack Additions

### New C Source Files

| File | Purpose | Why |
|------|---------|-----|
| `c_backend/src/ToffoliAddition.c` | Carry look-ahead adder (CLA) and ripple-carry adder (RCA) | Core Toffoli-based addition -- separate from QFT-based `IntegerAddition.c` to keep concerns isolated |
| `c_backend/include/toffoli_arithmetic_ops.h` | Header declaring all new Toffoli-based arithmetic functions | Parallel to `arithmetic_ops.h` for QFT variants |
| `c_backend/src/ToffoliMultiplication.c` | Schoolbook multiplication using Toffoli-based adders | Builds on CLA/RCA adder, same pattern as `IntegerMultiplication.c` |
| `c_backend/src/ToffoliDivision.c` | Restoring division using Toffoli-based subtraction | Replaces Python-level restoring division with C-level gate generation |
| `c_backend/src/hot_path_toffoli_add.c` | Hot path for Toffoli addition (like `hot_path_add.c`) | Performance: avoid Python/C boundary crossings |

### No New Dependencies Required

The existing stack (C backend, Cython, Python) requires zero new external dependencies. All algorithms use the existing gate primitives (`x()`, `cx()`, `ccx()`) already defined in `gate.c`.

## Detailed Design: New C Functions

### 1. Carry Look-Ahead Adder (CLA)

Based on Draper-Kutin-Hoyer-Kutin (2004), adapted for the existing `sequence_t` infrastructure.

**Functions to implement:**

```c
// ============================================================================
// Toffoli-Based Addition (Carry Look-Ahead)
// ============================================================================

// Quantum-quantum addition: a += b using CLA circuit
// Qubit layout: [0:n-1] a (target, modified), [n:2n-1] b (unchanged), [2n:3n-2] ancilla (carry/generate)
// Depth: O(log n), Gates: O(n) Toffoli + O(n) CNOT
sequence_t *QQ_add_toffoli(int bits);

// Classical-quantum addition: a += classical_value using Toffoli gates
// Qubit layout: [0:n-1] a (target, modified), [n:2n-2] ancilla
// Optimization: classical bits reduce to CNOT/X where classically known
sequence_t *CQ_add_toffoli(int bits, int64_t value);

// Controlled quantum-quantum addition: if ctrl then a += b
// Qubit layout: [0:n-1] a, [n:2n-1] b, [2n] control, [2n+1:3n-1] ancilla
sequence_t *cQQ_add_toffoli(int bits);

// Controlled classical-quantum addition: if ctrl then a += value
// Qubit layout: [0:n-1] a, [n] control, [n+1:2n-1] ancilla
sequence_t *cCQ_add_toffoli(int bits, int64_t value);
```

**Internal helper functions (static, not exported):**

```c
// MAJ gate: majority of 3 bits -- used in ripple-carry variant
// Implements: c_out = MAJ(a, b, c_in) = a*b XOR a*c_in XOR b*c_in
// Gate decomposition: CNOT(c,b) + CNOT(c,a) + Toffoli(a,b,c)
static void maj_gate(sequence_t *seq, int *layer, qubit_t a, qubit_t b, qubit_t c);

// UMA gate: un-majority and add -- inverse of MAJ + sum computation
// Gate decomposition: Toffoli(a,b,c) + CNOT(c,a) + CNOT(a,b)
static void uma_gate(sequence_t *seq, int *layer, qubit_t a, qubit_t b, qubit_t c);

// Generate/Propagate computation for CLA
// g[i] = a[i] AND b[i]  (Toffoli: target=g[i], ctrl1=a[i], ctrl2=b[i])
// p[i] = a[i] XOR b[i]  (CNOT: target=p[i], ctrl=a[i] -- p stored in-place on b)
static void compute_generate_propagate(sequence_t *seq, int *layer, int bits, ...);

// Parallel prefix tree for carry computation (Brent-Kung style)
// Uses O(log n) rounds of Toffoli+CNOT operations
static void carry_prefix_tree(sequence_t *seq, int *layer, int bits, ...);
```

**Algorithm outline (CLA):**

1. **Compute generate (g) and propagate (p):** For each bit position i, compute `g[i] = a[i] AND b[i]` (Toffoli) and `p[i] = a[i] XOR b[i]` (CNOT, in-place on b).
2. **Parallel prefix carry computation:** Using a Brent-Kung-style tree, compute all carry bits in O(log n) depth. Each tree node uses: Toffoli(g_out, p_high, g_low) then CNOT(p_out, p_high, p_low).
3. **Compute sum bits:** For each position, `sum[i] = p[i] XOR c[i]` (CNOT).
4. **Uncompute ancilla:** Reverse the prefix tree and generate/propagate computation to clean up ancilla qubits.

**Qubit count analysis:**

| Width (n) | a-register | b-register | Ancilla (carry/gen) | Total |
|-----------|-----------|-----------|---------------------|-------|
| 4 | 4 | 4 | 3 | 11 |
| 8 | 8 | 8 | 7 | 23 |
| 16 | 16 | 16 | 15 | 47 |
| n | n | n | n-1 | 3n-1 |

**Gate counts:**

| Width (n) | Toffoli | CNOT | Depth |
|-----------|---------|------|-------|
| 4 | ~14 | ~18 | O(log 4) = ~6 |
| 8 | ~34 | ~42 | O(log 8) = ~9 |
| 16 | ~74 | ~90 | O(log 16) = ~12 |
| n | O(n) | O(n) | O(log n) |

### 2. Ripple-Carry Adder (CDKM/Cuccaro, simpler alternative)

Based on Cuccaro et al. (2004). Simpler to implement, uses only 1 ancilla qubit but has O(n) depth.

**Functions:**

```c
// CDKM ripple-carry adder: a += b with single ancilla
// Qubit layout: [0:n-1] a (target), [n:2n-1] b (unchanged), [2n] ancilla (single carry qubit)
// Depth: O(n), Gates: 2n-1 Toffoli + 5n-3 CNOT
// Simpler to implement and verify than CLA
sequence_t *QQ_add_ripple(int bits);
```

**Algorithm (CDKM):**

1. **Forward sweep (MAJ chain):** For i = 0 to n-2, apply MAJ(c_i, b_i, a_i) where c_0 is the ancilla qubit initialized to |0>. Each MAJ consists of: CNOT(a,b), CNOT(a,c), Toffoli(c,b,a).
2. **Extract carry-out:** CNOT from a[n-1] to carry-out position.
3. **Reverse sweep (UMA chain):** For i = n-2 down to 0, apply UMA(c_i, b_i, a_i). Each UMA consists of: Toffoli(c,b,a), CNOT(a,c), CNOT(c,b). This writes sum bits into b and uncomputes carry bits.

**Recommendation:** Implement CDKM ripple-carry FIRST because:
- Only 1 ancilla qubit (vs n-1 for CLA)
- Straightforward sequential gate sequence (easier to verify)
- Sufficient for schoolbook multiplication (which is already O(n^2) depth)
- Can upgrade to CLA later for depth-sensitive applications

### 3. Schoolbook Multiplication

**Functions:**

```c
// Toffoli-based schoolbook multiplication: result = a * b
// Uses n iterations of controlled-addition (shift-and-add)
// Qubit layout: [0:n-1] result, [n:2n-1] a, [2n:3n-1] b, [3n:...] ancilla for adder
// Depth: O(n^2) with ripple-carry, O(n log n) with CLA
// Gate count: n * (adder gates) + n Toffoli for partial products
sequence_t *QQ_mul_toffoli(int bits);

// Classical-quantum: result = a * classical_value
// Optimization: skip iterations where classical bit is 0
sequence_t *CQ_mul_toffoli(int bits, int64_t value);

// Controlled versions
sequence_t *cQQ_mul_toffoli(int bits);
sequence_t *cCQ_mul_toffoli(int bits, int64_t value);
```

**Algorithm (schoolbook with Toffoli-based addition):**

For each bit position k of multiplier b (k = 0 to n-1):
1. **Partial product:** For each bit j of multiplicand a, compute `pp[j] = a[j] AND b[k]` using Toffoli gate into a scratch register (or directly into a controlled-add).
2. **Shift and add:** Add partial product (shifted by k positions) to accumulator using the Toffoli-based adder.
3. **Uncompute partial product** (if using scratch register).

**Alternative (CAS-based optimization):** Use controlled add-subtract circuits per Babbush et al. to reduce Toffoli count from ~2n^2 to ~n^2. This is a Phase 2 optimization.

### 4. Restoring Division

**Functions:**

```c
// Toffoli-based restoring division: computes quotient and remainder
// dividend / divisor = quotient remainder
// Qubit layout: [0:n-1] quotient (output), [n:2n-1] remainder (output),
//               [2n:3n-1] divisor (unchanged), [3n:...] ancilla
// Algorithm: n iterations of subtract-test-restore
sequence_t *QQ_divmod_toffoli(int bits);

// Classical divisor version
sequence_t *CQ_divmod_toffoli(int bits, int64_t divisor_value);

// Controlled versions
sequence_t *cQQ_divmod_toffoli(int bits);
sequence_t *cCQ_divmod_toffoli(int bits, int64_t divisor_value);
```

**Algorithm (restoring division):**

For bit position k = n-1 down to 0:
1. **Left-shift remainder:** Shift remainder register left by 1, bringing in next dividend bit.
2. **Trial subtraction:** Subtract divisor from remainder using Toffoli-based subtractor (adder with two's complement).
3. **Test sign bit:** CNOT from MSB of result to quotient bit k.
4. **Conditional restore:** If sign bit indicates negative result, add divisor back (controlled addition controlled by the sign bit).

**Subtraction:** Implemented as a += (~b + 1), using NOT gates on b, Toffoli-based addition, then NOT gates to restore b. Alternatively, use the adder's inversion flag (the existing `run_instruction()` already supports `invert` parameter).

## Ancilla Management Strategy

### Current Infrastructure (Sufficient)

The existing `qubit_allocator_t` system handles ancilla allocation:

```c
// Allocate ancilla for Toffoli-based operations
qubit_t ancilla_start = allocator_alloc(circ->allocator, num_ancilla, /*is_ancilla=*/true);

// After operation, return ancilla to pool
allocator_free(circ->allocator, ancilla_start, num_ancilla);
```

### Ancilla Requirements by Operation

| Operation | Ancilla Count | Notes |
|-----------|--------------|-------|
| RCA add (CDKM) | 1 | Single carry qubit |
| CLA add (Draper) | n-1 | Generate/propagate/carry bits |
| Schoolbook multiply | 1 to n | Depends on adder used + partial product scratch |
| Restoring division | n (remainder) + adder ancilla | Remainder register + subtractor workspace |

### Ancilla Lifecycle in Hot Path

The hot path functions (`hot_path_toffoli_add.c`) will follow the existing pattern from `hot_path_add.c`:

1. Cython extracts qubit indices from Python `qint` objects
2. Builds qubit_array on stack (up to 256 entries, matching current `qa[256]`)
3. Calls C function with `nogil`
4. C function generates `sequence_t*`, calls `run_instruction(seq, qa, invert, circ)`

**Key constraint:** The `qa[256]` stack array in hot path functions limits total qubits per operation to 256. For a 64-bit CLA adder: 64 + 64 + 63 ancilla = 191 qubits -- fits comfortably. For 64-bit multiplication: 64 + 64 + 64 + ancilla = 192+ -- still fits.

### Ancilla Qubit Source

Currently, ancilla qubits come from the Python `_get_ancilla()` function which returns a pre-allocated pool. For Toffoli-based operations, the approach should be:

**Option A (recommended):** Allocate ancilla within the C hot path function using `allocator_alloc()` directly. This avoids round-tripping to Python for ancilla management.

**Option B:** Continue using the Python-managed ancilla pool. Requires ensuring pool size is sufficient for Toffoli operations (which need more ancilla than QFT operations).

Option A is preferred because Toffoli-based operations have deterministic ancilla counts (function of `bits`), and allocating in C eliminates a Python/C boundary crossing.

## Build System Changes

### setup.py Additions

```python
c_sources = [
    # ... existing sources ...
    # Toffoli-based arithmetic (new milestone)
    os.path.join(PROJECT_ROOT, "c_backend", "src", "ToffoliAddition.c"),
    os.path.join(PROJECT_ROOT, "c_backend", "src", "ToffoliMultiplication.c"),
    os.path.join(PROJECT_ROOT, "c_backend", "src", "ToffoliDivision.c"),
    os.path.join(PROJECT_ROOT, "c_backend", "src", "hot_path_toffoli_add.c"),
]
```

No new compiler flags needed. The existing `-O3 -pthread` flags are sufficient.

### Header Dependencies

```
toffoli_arithmetic_ops.h
  |-- includes: types.h (for sequence_t, gate_t, qubit_t)
  |-- uses: gate.h functions (x, cx, ccx) at implementation level
```

## Cython/Python Integration

### New .pxi Include (or additions to existing)

The Toffoli-based operations should be exposed through the existing operator overloading infrastructure. Two approaches:

**Approach A (recommended): Arithmetic mode selector**

Add an `arithmetic_mode` property to `qint` (or a global/context setting) that switches between QFT-based and Toffoli-based implementations:

```python
# Python API
import quantum_language as ql

ql.set_arithmetic_mode('toffoli')  # or 'qft' (default)
a = qint(5, width=8)
b = qint(3, width=8)
c = a + b  # Uses Toffoli-based adder
```

**Approach B: Explicit functions**

```python
a.toffoli_add(b)  # Explicit Toffoli-based addition
```

Approach A is cleaner because it preserves the natural operator syntax. The mode flag routes to different hot path functions internally.

### Cython Declarations Needed

```cython
# In _core.pxd or a new toffoli_ops.pxd
cdef extern from "toffoli_arithmetic_ops.h":
    sequence_t *QQ_add_toffoli(int bits)
    sequence_t *CQ_add_toffoli(int bits, int64_t value)
    sequence_t *cQQ_add_toffoli(int bits)
    sequence_t *cCQ_add_toffoli(int bits, int64_t value)
    sequence_t *QQ_mul_toffoli(int bits)
    # ... etc

cdef extern from "hot_path_toffoli_add.h":
    void hot_path_toffoli_add_qq(circuit_t *circ, ...) nogil
    void hot_path_toffoli_add_cq(circuit_t *circ, ...) nogil
```

## Gate Infrastructure Assessment

### Existing Gates (No Changes Needed)

| Gate | Function | Used By | Status |
|------|----------|---------|--------|
| X (NOT) | `x()` | Two's complement negation, bit flips | EXISTS |
| CNOT | `cx()` | XOR, propagate computation, sum bits | EXISTS |
| Toffoli (CCX) | `ccx()` | Generate computation, MAJ/UMA, carry logic | EXISTS |
| MCX (n-controlled X) | `mcx()` | Multi-controlled operations (for controlled division) | EXISTS |

### Gate Commutation Rules (May Need Extension)

The `gates_commute()` function in `gate.c` currently handles P-P, P-Z, X-X, H-H, Z-P, Z-Z commutation. For Toffoli-based circuits, the optimizer may benefit from:

- **Toffoli-Toffoli commutation:** Two Toffoli gates commute if they share no qubits (already handled by the generic check at line 469-470 of `gate.c`: "if both have controls and different targets, commute").
- **CNOT-Toffoli commutation:** CNOT and Toffoli commute when they share no qubits (already covered).

The existing commutation rules are sufficient for correctness. Circuit optimization for Toffoli circuits is a separate concern (handled by the existing inverse-cancellation pass, which already cancels Toffoli-Toffoli pairs via `gates_are_inverse()`).

### Inverse Detection for Toffoli Gates

The `gates_are_inverse()` function correctly identifies that X, CNOT, and Toffoli are self-inverse (since `GateValue` is set to 1 for all X-type gates, and 1 == 1). This means the existing circuit optimizer will automatically cancel adjacent identical Toffoli gates -- which is important for uncomputation cleanup.

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Adder type | CDKM ripple-carry first, then CLA | CLA-only | RCA is simpler to implement and debug; CLA can be added later for depth optimization |
| Multiplication | Schoolbook with RCA | Karatsuba, CAS-optimized | Schoolbook is simplest, matches existing QFT multiplication structure, optimizations are Phase 2 |
| Division | Restoring (bit-level) | Non-restoring, SRT | Restoring is already implemented at Python level -- just need to push gate generation to C |
| Ancilla management | C-level allocation | Python-level pool | Deterministic counts, fewer boundary crossings |
| API surface | Mode selector (`set_arithmetic_mode`) | Separate functions | Preserves natural operator syntax |
| File organization | Separate ToffoliAddition.c etc. | Add to existing IntegerAddition.c | Keeps QFT and Toffoli implementations cleanly separated; avoids 1000+ line files |

## Implementation Order

1. **CDKM Ripple-Carry Adder** -- Foundation for everything else. Simple, well-understood, easy to test against QFT adder.
2. **CLA Adder** -- Upgrade path for depth-sensitive applications. Can share test infrastructure with RCA.
3. **Schoolbook Multiplication** -- Built on top of Toffoli adder. Directly comparable to existing QFT multiplication.
4. **Restoring Division** -- Built on top of Toffoli subtraction (= adder with inversion). Replaces Python-level implementation.
5. **Hot path migration** -- Performance optimization once algorithms are verified.
6. **Hardcoded sequences** (optional) -- Same pattern as QFT hardcoded sequences, for common widths.

## Verification Strategy

### Test Against Existing QFT Implementations

The key advantage of this milestone: every Toffoli-based operation can be verified against the existing QFT-based operation. For each width and operand combination:

```python
# Verify Toffoli addition matches QFT addition
ql.set_arithmetic_mode('qft')
qft_result = simulate(a + b)

ql.set_arithmetic_mode('toffoli')
toff_result = simulate(a + b)

assert qft_result == toff_result
```

### Gate-Level Verification

Export to OpenQASM 3.0 (existing capability) and verify via Qiskit simulation (existing capability). Compare statevectors between QFT and Toffoli implementations.

## Sources

- Cuccaro et al., "A new quantum ripple-carry addition circuit" (2004) -- [arXiv:quant-ph/0410184](https://arxiv.org/abs/quant-ph/0410184)
- Draper et al., "A logarithmic-depth quantum carry-lookahead adder" (2004) -- [arXiv:quant-ph/0406142](https://arxiv.org/abs/quant-ph/0406142)
- Quantum schoolbook multiplication with fewer Toffoli gates (2024) -- [arXiv:2410.00899](https://arxiv.org/html/2410.00899v1)
- Quantum Circuit Design of Integer Division Optimizing Ancillary Qubits and T-Count -- [arXiv:1609.01241](https://ar5iv.labs.arxiv.org/html/1609.01241)
- Qiskit CDKMRippleCarryAdder reference -- [IBM Quantum Docs](https://quantum.cloud.ibm.com/docs/en/api/qiskit/qiskit.circuit.library.CDKMRippleCarryAdder)
- Qiskit implementation (source) -- [GitHub](https://github.com/Qiskit/qiskit/blob/main/qiskit/circuit/library/arithmetic/adders/cdkm_ripple_carry_adder.py)
- Higher radix CLA architecture -- [Nature Scientific Reports](https://www.nature.com/articles/s41598-023-41122-4)
- Factoring using 2n+2 qubits with Toffoli-based modular multiplication -- [arXiv:1611.07995](https://arxiv.org/pdf/1611.07995)

## Confidence Assessment

| Area | Confidence | Reason |
|------|------------|--------|
| Gate infrastructure compatibility | HIGH | Verified directly: `ccx()`, `cx()`, `x()` exist and work correctly in `gate.c` |
| Sequence/circuit pipeline compatibility | HIGH | Verified: `run_instruction()` handles arbitrary gate types, qubit remapping works |
| CDKM ripple-carry algorithm | HIGH | Well-established (2004), implemented in Qiskit, clear gate-level description |
| CLA algorithm | MEDIUM | Well-established paper but more complex to implement; prefix tree requires careful indexing |
| Schoolbook multiplication | HIGH | Direct extension of adder; existing multiplication pattern in codebase is directly analogous |
| Restoring division | HIGH | Already implemented at Python level; C-level is a direct translation |
| Ancilla management | HIGH | Existing `allocator_alloc/free` API is designed for exactly this use case |
| Build system changes | HIGH | Trivial additions to `setup.py` c_sources list |
| Hot path migration | MEDIUM | Pattern well-established from Phase 60, but Toffoli operations have different qubit layouts |
