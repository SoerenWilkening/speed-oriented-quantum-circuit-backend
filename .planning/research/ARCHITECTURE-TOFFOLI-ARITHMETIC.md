# Architecture Patterns: Toffoli-Based Arithmetic

**Domain:** Quantum arithmetic circuits (Toffoli gate basis)
**Researched:** 2026-02-14

## Recommended Architecture

### Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| `ToffoliAddition.c` | Generate RCA and CLA addition `sequence_t` | `gate.c` (for x/cx/ccx calls) |
| `ToffoliMultiplication.c` | Generate schoolbook multiplication `sequence_t` | `ToffoliAddition.c` (calls adder internally) |
| `ToffoliDivision.c` | Generate restoring division `sequence_t` | `ToffoliAddition.c` (calls adder for subtract/restore) |
| `toffoli_arithmetic_ops.h` | Public API for all Toffoli operations | Consumed by hot_path_*.c and Cython bindings |
| `hot_path_toffoli_add.c` | Qubit layout + run_instruction dispatch | `execution.c`, `toffoli_arithmetic_ops.h` |
| Cython `qint_arithmetic.pxi` | Mode-aware operator dispatch | `hot_path_toffoli_add.h`, `hot_path_add.h` |

### Data Flow

```
Python: a += b
    |
    v
Cython: addition_inplace(self, other)
    |
    +-- Check arithmetic_mode
    |
    +-- mode=='qft':  hot_path_add_qq()    [existing]
    +-- mode=='toffoli': hot_path_toffoli_add_qq()  [new]
    |
    v
C hot path: Build qubit_array, allocate ancilla
    |
    v
C sequence generator: QQ_add_toffoli(bits) -> sequence_t*
    |-- Calls ccx(), cx(), x() to build gate sequence
    |-- Returns cached sequence (precompiled pattern)
    |
    v
C execution: run_instruction(seq, qubit_array, invert, circ)
    |-- Maps abstract qubit indices to physical qubit indices
    |-- Calls add_gate() for each gate in sequence
    |
    v
Circuit: Gates stored in circuit_t layers
```

### Parallel Structure with QFT Operations

```
QFT Path (existing):                    Toffoli Path (new):
IntegerAddition.c                       ToffoliAddition.c
  QQ_add(bits) -> sequence_t*             QQ_add_toffoli(bits) -> sequence_t*
  CQ_add(bits, val) -> sequence_t*        CQ_add_toffoli(bits, val) -> sequence_t*
  cQQ_add(bits) -> sequence_t*            cQQ_add_toffoli(bits) -> sequence_t*
  cCQ_add(bits, val) -> sequence_t*       cCQ_add_toffoli(bits, val) -> sequence_t*

IntegerMultiplication.c                 ToffoliMultiplication.c
  QQ_mul(bits) -> sequence_t*             QQ_mul_toffoli(bits) -> sequence_t*
  ...                                     ...

(Python-level division)                 ToffoliDivision.c
  qint_division.pxi                       QQ_divmod_toffoli(bits) -> sequence_t*
                                          ...

hot_path_add.c                          hot_path_toffoli_add.c
  hot_path_add_qq()                       hot_path_toffoli_add_qq()
  hot_path_add_cq()                       hot_path_toffoli_add_cq()
```

## Patterns to Follow

### Pattern 1: MAJ-UMA Chain (CDKM Ripple-Carry)

**What:** The CDKM adder is built from two primitive sub-circuits: MAJ (majority) and UMA (un-majority-and-add), chained together in a ripple pattern.

**When:** For all basic Toffoli-based addition.

**Gate-level decomposition:**

```c
// MAJ gate: computes carry into c, modifies a and b
// Before: a=a_i, b=b_i, c=c_i (carry-in)
// After:  a=a_i XOR c_i, b=a_i XOR b_i, c=MAJ(a_i,b_i,c_i) = carry-out
static void maj_gate(sequence_t *seq, int *layer, qubit_t a, qubit_t b, qubit_t c) {
    // Gate 1: CNOT(target=b, control=c)  -- b ^= c
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], b, c);
    (*layer)++;
    // Gate 2: CNOT(target=a, control=c)  -- a ^= c
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], a, c);
    (*layer)++;
    // Gate 3: Toffoli(target=c, control1=a, control2=b)  -- c ^= (a AND b)
    // After gates 1-2: a = a_orig XOR c_orig, b = b_orig XOR c_orig
    // So Toffoli computes: c ^= (a_orig XOR c_orig) AND (b_orig XOR c_orig)
    // Which equals: c_new = MAJ(a_orig, b_orig, c_orig) = carry_out
    ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], c, a, b);
    (*layer)++;
}

// UMA gate: uncomputes carry and writes sum bit
// Before: a=a_i XOR c_i, b=a_i XOR b_i, c=carry_out
// After:  a=a_i, b=sum_i = a_i XOR b_i XOR c_i, c=c_i (restored)
static void uma_gate(sequence_t *seq, int *layer, qubit_t a, qubit_t b, qubit_t c) {
    // Gate 1: Toffoli(target=c, control1=a, control2=b) -- undo MAJ's Toffoli
    ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], c, a, b);
    (*layer)++;
    // Gate 2: CNOT(target=a, control=c) -- restore a = a_orig
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], a, c);
    (*layer)++;
    // Gate 3: CNOT(target=b, control=a) -- b = sum = a_orig XOR b_orig XOR c_orig
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], b, a);
    (*layer)++;
}

// Full n-bit ripple-carry addition (CDKM):
// Qubit layout: a[0..n-1], b[0..n-1], anc (single ancilla = carry_0)
//   a[i] at qubit index: 2*i + 1
//   b[i] at qubit index: 2*i + 2
//   c[i] at qubit index: 2*i (where c[0] = ancilla at index 0)
// This interleaved layout minimizes wire crossings
sequence_t *QQ_add_toffoli(int bits) {
    // Forward sweep: MAJ chain
    // MAJ(c_0, b_0, a_0), MAJ(c_1, b_1, a_1), ..., MAJ(c_{n-2}, b_{n-2}, a_{n-2})
    // After: c_{n-1} = carry-out, all intermediate carries computed

    // Reverse sweep: UMA chain
    // UMA(c_{n-2}, b_{n-2}, a_{n-2}), ..., UMA(c_0, b_0, a_0)
    // After: b[i] = sum[i], all carries uncomputed, ancilla restored to |0>
}
```

### Pattern 2: Precompiled Sequence Cache

**What:** Cache computed `sequence_t*` for each width, returning the same pointer on subsequent calls.

**When:** For all Toffoli-based operations (same pattern as QFT operations).

**Example:**

```c
// Width-parameterized precompiled caches (same pattern as QFT)
static sequence_t *precompiled_QQ_add_toffoli[65] = {NULL};

sequence_t *QQ_add_toffoli(int bits) {
    if (bits < 1 || bits > 64) return NULL;

    // Check cache
    if (precompiled_QQ_add_toffoli[bits] != NULL)
        return precompiled_QQ_add_toffoli[bits];

    // Build sequence...
    sequence_t *add = malloc(sizeof(sequence_t));
    // ... populate with MAJ/UMA gates ...

    // Cache and return
    precompiled_QQ_add_toffoli[bits] = add;
    return add;
}
```

### Pattern 3: Hot Path Qubit Array Assembly

**What:** Build the qubit_array mapping on the C stack, then call `run_instruction()` once.

**When:** For all hot path functions.

**Example:**

```c
void hot_path_toffoli_add_qq(circuit_t *circ,
                              const unsigned int *self_qubits, int self_bits,
                              const unsigned int *other_qubits, int other_bits,
                              int invert, int controlled, unsigned int control_qubit) {
    unsigned int qa[256];
    int pos = 0;
    int result_bits = self_bits > other_bits ? self_bits : other_bits;

    // Map sequence indices to physical qubits:
    // Sequence index 0 = ancilla (carry_0) -> allocate from circuit
    qubit_t ancilla = allocator_alloc(circ->allocator, 1, true);
    qa[0] = ancilla;

    // Interleaved layout: a[i] at 2*i+1, b[i] at 2*i+2
    for (int i = 0; i < result_bits; i++) {
        qa[2*i + 1] = self_qubits[i];   // a[i]
        qa[2*i + 2] = other_qubits[i];  // b[i]
    }

    sequence_t *seq = QQ_add_toffoli(result_bits);
    if (seq == NULL) return;

    run_instruction(seq, qa, invert, circ);

    // Return ancilla
    allocator_free(circ->allocator, ancilla, 1);
}
```

### Pattern 4: Subtraction via Complement

**What:** Implement subtraction as `a -= b` = `a += (~b + 1)`, but more efficiently as: flip all b bits (NOT), add with carry-in = 1, flip b bits back.

**When:** For all subtraction operations (needed by division).

**Example:**

```c
// Subtraction: a -= b
// Step 1: NOT all b qubits (b -> ~b)
// Step 2: Run adder with carry-in initialized to 1 (X gate on ancilla before MAJ chain)
// Step 3: NOT all b qubits back (~b -> b, restoring b)
// Result: a = a + ~b + 1 = a - b (two's complement subtraction)
```

This is cleaner than the `invert` flag approach because the carry-in = 1 is explicit.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Allocating Ancilla in Python

**What:** Allocating Toffoli-operation ancilla qubits via `_get_ancilla()` in Python and passing them through Cython.

**Why bad:** The ancilla count depends on the algorithm (1 for RCA, n-1 for CLA), which is a C-level implementation detail. Exposing this to Python creates a leaky abstraction and requires Python-level changes when the adder algorithm changes.

**Instead:** Allocate ancilla within the C hot path function using `allocator_alloc(circ->allocator, count, true)`. The hot path knows exactly how many ancilla are needed and can clean them up deterministically.

### Anti-Pattern 2: Separate Subtraction Implementation

**What:** Implementing a separate `QQ_sub_toffoli()` that generates its own gate sequence independently of the adder.

**Why bad:** Subtraction IS addition with complement. A separate implementation means two codepaths that must be kept in sync, doubling the bug surface.

**Instead:** Implement subtraction as NOT(b) + adder-with-carry-in + NOT(b). Or use the existing `run_instruction()` `invert` parameter to run the adder in reverse. For Toffoli-based adders (where all gates are self-inverse), inversion means running the gates in reverse order, which is exactly what `run_instruction(seq, qa, /*invert=*/1, circ)` does.

### Anti-Pattern 3: Mixed Qubit Layout Between Sequence and Hot Path

**What:** Having the `sequence_t` use one qubit ordering (e.g., interleaved a/b/carry) while the hot path builds the qubit_array with a different ordering (e.g., contiguous blocks).

**Why bad:** This is the exact bug class (BUG-CQQ-ARITH) that caused weeks of debugging in the QFT arithmetic. The sequence's qubit indices are abstract (0, 1, 2, ...) and the qubit_array maps them to physical qubits. If the mapping is wrong, the circuit silently produces incorrect results.

**Instead:** Document the qubit layout contract at the top of each `sequence_t`-returning function AND at the top of each hot path function. Use the same documentation format as existing functions (e.g., `// Qubit layout for QQ_add_toffoli(bits): ...`). Write tests that verify the mapping.

### Anti-Pattern 4: Dynamic Layer Allocation (MAXLAYERINSEQUENCE)

**What:** Using `MAXLAYERINSEQUENCE` (10000) for the `num_layer` field and allocating that many layers upfront, like the existing `QQ_mul()` does.

**Why bad:** Toffoli-based circuits have precisely known layer counts. For an n-bit CDKM adder, the layer count is exactly `6n - 3` (3 gates per MAJ * (n-1) + 3 gates per UMA * (n-1) + final CNOT). Over-allocating wastes memory and makes it harder to detect off-by-one errors.

**Instead:** Compute the exact layer count from `bits` before allocation. The QFT addition code (`IntegerAddition.c`) already does this correctly for its operations (e.g., `add->num_layer = 5 * bits - 2`).

## Scalability Considerations

| Concern | Width 4 | Width 8 | Width 16 | Width 64 |
|---------|---------|---------|----------|----------|
| RCA qubits | 9 (4+4+1) | 17 (8+8+1) | 33 (16+16+1) | 129 (64+64+1) |
| CLA qubits | 11 (4+4+3) | 23 (8+8+7) | 47 (16+16+15) | 191 (64+64+63) |
| RCA Toffoli count | 7 | 15 | 31 | 127 |
| RCA depth | 22 | 46 | 94 | 382 |
| CLA depth | ~6 | ~9 | ~12 | ~18 |
| Multiply qubits (RCA) | ~13 | ~25 | ~49 | ~193 |
| Multiply Toffoli count | ~28 | ~120 | ~496 | ~8128 |
| Division iterations | 4 | 8 | 16 | 64 |
| qa[256] limit | OK | OK | OK | OK (191 max) |

**Key insight:** The existing `qa[256]` stack array in hot path functions is sufficient for all operations up to 64-bit width. The worst case (64-bit CLA addition) uses 191 qubits, well within the 256 limit.

**Circuit size limit:** The `MAXLAYERINSEQUENCE` constant (10000) may need to be increased for large-width Toffoli multiplication. A 64-bit schoolbook multiply generates ~8000 Toffoli gates across ~24000 layers. This constant should be replaced with dynamic allocation for Toffoli operations (compute exact layer count from the algorithm parameters).

## Sources

- Cuccaro et al. (2004), MAJ/UMA decomposition -- [arXiv:quant-ph/0410184](https://arxiv.org/abs/quant-ph/0410184)
- Draper et al. (2004), CLA adder -- [arXiv:quant-ph/0406142](https://arxiv.org/abs/quant-ph/0406142)
- Existing codebase analysis: `gate.c` (gate primitives), `execution.c` (run_instruction), `IntegerAddition.c` (QFT adder pattern), `hot_path_add.c` (hot path pattern), `qubit_allocator.c` (ancilla management)
