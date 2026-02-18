# Domain Pitfalls: Toffoli-Based Fault-Tolerant Arithmetic

**Domain:** Adding Toffoli-based arithmetic (CLA adder, schoolbook multiplier, restoring divider) to an existing QFT-based quantum programming framework
**Researched:** 2026-02-14
**Confidence:** HIGH for codebase-specific pitfalls (source code inspected), MEDIUM for algorithmic pitfalls (literature-verified)

## Executive Summary

Adding Toffoli-based arithmetic to this codebase is a fundamentally different kind of work than the QFT arithmetic that already exists. QFT arithmetic uses no ancilla qubits, operates entirely through phase rotations (CP gates), and has a qubit layout that is fixed at compile time. Toffoli-based arithmetic uses O(n) ancilla qubits for carry-lookahead adders, operates through CCX/CNOT/X gates, and requires dynamic ancilla allocation and uncomputation. The transition surfaces six categories of pitfalls: (1) ancilla lifecycle management bugs, (2) carry-lookahead generate/propagate logic errors, (3) schoolbook multiplication add-subtract subtleties, (4) dual-backend operator overloading collisions, (5) qubit count surprises, and (6) optimizer incompatibility. The most dangerous pitfalls are those that silently produce wrong results rather than crashes -- particularly ancilla qubits left dirty after uncomputation and generate/propagate index off-by-one errors.

---

## Critical Pitfalls

Mistakes that cause wrong computational results, memory corruption, or require architecture rewrites.

### Pitfall 1: Ancilla Qubits Left Dirty After Uncomputation

**What goes wrong:** Toffoli-based adders (ripple-carry and carry-lookahead) use ancilla qubits to store intermediate carry values. These ancilla MUST be returned to |0> state after the computation completes, or they create unwanted entanglement with the data registers. If uncomputation is incomplete or occurs in wrong order, the ancilla remain dirty -- the circuit produces wrong results for *all subsequent operations* that reuse those qubits, but the error is silent (no crash, no assertion failure).

**Why it happens:** The existing QFT arithmetic uses zero ancilla qubits. Every `QQ_add`, `CQ_add`, `cQQ_add`, `cCQ_add` function in `IntegerAddition.c` returns a sequence of CP/H gates that operate only on data qubits. The entire ancilla management infrastructure (`allocator_alloc` with `is_ancilla=true`, `allocator_free`) has been built (see `qubit_allocator.c`) but has never been exercised by arithmetic operations -- only by comparison operations (`CQ_equal_width` in `IntegerComparison.c`) and bitwise operations (`qint_bitwise.pxi`). The Toffoli adder will be the first arithmetic operation to allocate, use, and free ancilla qubits.

**Consequences:**
- Silent wrong results on all computations after the dirty ancilla is reused
- Ancilla qubit is entangled with data register, corrupting quantum state
- Debugging requires full statevector simulation, which is infeasible for widths > ~15

**Warning signs:**
- Test passes for single operations but fails for two sequential operations
- Result depends on whether ancilla was previously used (order-dependent bugs)
- Freed ancilla stack in `qubit_allocator.c` returns qubits that are not in |0>

**Prevention:**
- Every Toffoli-based adder sequence MUST include explicit uncomputation of all ancilla to |0>
- Add a debug mode assertion: after uncomputation, simulate ancilla measurement (must give 0 with probability 1)
- The `allocator_free()` function (line 155 of `qubit_allocator.c`) should flag qubits freed without uncomputation in debug builds
- Unit test pattern: run operation A, then run operation B using same-width ancilla, verify B's result is independent of A's inputs

**Phase to address:** First phase (adder implementation). MUST be correct from day one -- retrofitting uncomputation is extremely painful.

**Codebase-specific risk:** The `allocator_alloc()` function only reuses freed qubits for `count == 1` allocations (line 94 of `qubit_allocator.c`). CLA adders need contiguous blocks of O(n) ancilla qubits. If these are allocated fresh each time instead of reused, qubit counts will explode. But if they ARE reused (after fixing the count>1 reuse limitation), the dirty-ancilla bug becomes catastrophic because the reused qubits may not be in |0>.

---

### Pitfall 2: Carry-Lookahead Generate/Propagate Index Off-By-One Errors

**What goes wrong:** The Draper-Kutin-Rains-Svore (DKRS) carry-lookahead adder computes carry bits through a prefix tree using generate (G) and propagate (P) signals. The tree has O(log n) levels with specific index arithmetic for combining G/P pairs. Off-by-one errors in the tree indexing produce circuits that compute correct carries for small widths (n <= 4) but wrong carries for larger widths.

**Why it happens:** The DKRS CLA adder has three phases:
1. **P-round (upward sweep):** Compute G[i:j] = G[i:k] OR (P[i:k] AND G[k:j]) for tree node pairs. Indices follow a binary tree structure where parent (i,j) combines children (i,k) and (k,j).
2. **G-round (downward sweep):** Propagate final carry values back down the tree.
3. **Cleanup:** Uncompute the P/G ancilla.

Each phase has different index patterns. The upward sweep processes pairs at distance 1, 2, 4, 8... while the downward sweep processes at distance n/2, n/4, n/2... The index calculations involve `floor(i/2^k)`, `ceil()`, and boundary checks. Getting any of these wrong by one position produces a circuit that "almost works" -- correct for powers of 2, wrong for other widths.

**Consequences:**
- Correct results for n=1,2,4 (trivial cases), wrong for n=3,5,6,7 (non-power-of-2)
- Or: correct results for all widths up to n=8, wrong for n=9+ (larger tree depth)
- Extremely difficult to debug because the circuit "mostly works"

**Prevention:**
- Implement the simplest correct version first: Cuccaro ripple-carry adder (1 ancilla, O(n) depth, 2n-1 Toffoli gates). Only after this works, implement CLA.
- Cuccaro ripple-carry is well-understood: qubit layout is `|a_0, a_1, ..., a_{n-1}, b_0, b_1, ..., b_{n-1}, c>` where c is the single ancilla. The MAJ and UMA gate sequences are simple and hard to get wrong.
- For CLA: exhaustively test all inputs for widths 1-6 before testing larger widths
- Compare CLA output against ripple-carry output for random inputs at each width
- Reference implementation test: generate expected carry bits classically, compare against quantum circuit output

**Phase to address:** Adder implementation phase. Start with ripple-carry, then CLA as optimization.

---

### Pitfall 3: `reverse_circuit_range()` Cannot Uncompute Toffoli Circuits Correctly

**What goes wrong:** The existing uncomputation mechanism in `execution.c` (`reverse_circuit_range()`, line 62) inverts gates by negating `GateValue`. This works perfectly for QFT circuits (where `P(theta)` is inverted to `P(-theta)` and `H` is self-inverse). But Toffoli gates (CCX) are self-inverse -- they have `GateValue = 1` and their inverse is also a CCX with `GateValue = 1`. Negating `GateValue` to -1 produces a gate that does NOT exist in the gate set and will behave unpredictably.

**Why it happens:** The `reverse_circuit_range()` function was written for QFT circuits where `GateValue` encodes phase angles. Line 84: `g.GateValue = -g.GateValue;`. For P gates with angle theta, this correctly produces P(-theta). For X gates (including CX and CCX), `GateValue` is 1 (set in `x()`, `cx()`, `ccx()` in `gate.c`). Negating gives -1, which has no defined semantics for X-type gates.

**Consequences:**
- Uncomputation of Toffoli-based arithmetic produces garbage circuits
- Layer tracking (`_start_layer` / `_end_layer` in `qint_arithmetic.pxi`) records the wrong range
- The `with can_subtract:` pattern in division (line 79 of `qint_division.pxi`) implicitly relies on uncomputation working correctly

**Warning signs:**
- Uncomputation tests pass for QFT operations but fail for Toffoli operations
- `GateValue` of -1 appears in circuit dumps for X-type gates
- Division/modulo operations using Toffoli adder produce wrong results even when addition is correct

**Prevention:**
- Fix `reverse_circuit_range()` to check gate type: for self-inverse gates (X, H, CX, CCX), do NOT negate `GateValue` -- just replay the gate
- Add gate type classification: `gate_is_self_inverse()` function that returns true for X, H, and their controlled variants
- Test: `reverse_circuit_range()` on a Toffoli circuit should produce the exact inverse (verify by composing original + inverse and checking identity)

**Phase to address:** Must be fixed BEFORE any Toffoli arithmetic is integrated into the operator overloading. This is a prerequisite, not a follow-up.

**Code locations:**
- `c_backend/src/execution.c:84` -- the negation line
- `c_backend/src/gate.c:445-463` -- `gates_are_inverse()` already handles this correctly for the optimizer (X gates compare `GateValue` directly, not negated), but `reverse_circuit_range()` does not use `gates_are_inverse()`

---

### Pitfall 4: `qubit_array` Layout Collision Between QFT and Toffoli Backends

**What goes wrong:** The `qubit_array` in `hot_path_add.c` (line 33) is a stack-allocated `unsigned int qa[256]` that maps sequence-local qubit indices to physical qubit indices. QFT addition (`QQ_add`) uses layout `[target_0..target_{n-1}, other_0..other_{n-1}]` with no ancilla. Toffoli addition (CLA or ripple-carry) uses layout `[a_0..a_{n-1}, b_0..b_{n-1}, ancilla_0..ancilla_{k}]`. If the backend selection happens at the wrong level (inside vs outside the hot path), the `qubit_array` layout will be assembled for QFT but executed with a Toffoli sequence, or vice versa.

**Why it happens:** The qubit layout is hard-coded in `hot_path_add_qq()` (lines 22-63 of `hot_path_add.c`). The layout differs between QFT and Toffoli:

| Backend | Uncontrolled Layout | Controlled Layout |
|---------|---------------------|-------------------|
| QFT `QQ_add` | `[target, other]` (2n qubits) | `[target, other, control]` (2n+1 qubits) |
| Toffoli RCA | `[a, b, carry_ancilla]` (2n+1 qubits) | `[a, b, carry_ancilla, control]` (2n+2 qubits) |
| Toffoli CLA | `[a, b, g_ancilla, p_ancilla]` (~3n qubits) | `[a, b, g_ancilla, p_ancilla, control]` (~3n+1 qubits) |

The Cython layer (`qint_arithmetic.pxi:7-64`) assembles the qubit array based on the CURRENT layout expectation. If a runtime flag selects the backend but the Cython code is compiled with only QFT layout logic, the ancilla positions will be wrong.

**Consequences:**
- Ancilla qubits overlap with data qubits, producing garbage results
- Or: ancilla indices point to unallocated qubits, causing segfault
- Or: control qubit position is wrong, producing unconditional operations when conditional is expected

**Warning signs:**
- Known existing issue: "qubit_array is global and may contain stale values" (from MEMORY.md)
- Tests pass for uncontrolled QQ_add but fail for controlled cQQ_add with Toffoli backend
- Circuit qubit count does not match expected count

**Prevention:**
- Each backend must have its own hot path function (e.g., `hot_path_add_qq_toffoli()`) with correct layout
- OR: The hot path function must accept a layout descriptor that specifies how many ancilla and where they go
- Never share a single `qubit_array` assembly between QFT and Toffoli paths
- Add assertions: `assert(qa_length == seq->expected_qubits)` before `run_instruction()`

**Phase to address:** Backend switching infrastructure phase. Must be designed before any Toffoli sequences are wired into the hot paths.

---

### Pitfall 5: Optimizer Gate Cancellation Rules Wrong for Toffoli Circuits

**What goes wrong:** The optimizer in `optimizer.c` (line 163, `add_gate()`) checks if consecutive gates are inverse pairs and cancels them. The `gates_are_inverse()` function in `gate.c` (line 445) correctly identifies that two CCX gates on the same qubits are inverses (since CCX is self-inverse). However, the optimizer only checks the *immediately preceding gate on the same target qubit* (line 144: `gate_index_of_layer_and_qubits[min_possible_layer - 1][g->Target]`). In Toffoli circuits, cancellation opportunities often span multiple layers (CCX, CNOT, CCX pattern). The current optimizer will:
1. See CCX on qubit X at layer L
2. See CNOT on qubit X at layer L+1 (different gate, no cancellation)
3. See CCX on qubit X at layer L+2 -- but this is NOT an inverse of the L+1 CNOT, and the L CCX is no longer the "previous gate"

This means the optimizer is *less effective* for Toffoli circuits than for QFT circuits, where consecutive CP gates with opposite angles are common and easily caught.

More dangerously: the optimizer might INCORRECTLY cancel gates. Two CCX gates on the same target but different controls are NOT inverses, but `gates_are_inverse()` correctly checks controls (line 459-460). However, if the optimizer *parallelizes* gates (reordering to minimize depth), it might move a CCX past a CNOT that shares a control qubit, creating a new adjacency where cancellation is attempted. The commutation check `gates_commute()` (line 466) returns `true` for X gates on the same target (line 481-482), which is correct for bare X but NOT for CCX with different controls.

**Why it happens:** The optimizer was designed for QFT circuits where:
- CP gates commute with each other (line 472-473: P with P returns true)
- Inverse pairs are always CP(theta) followed by CP(-theta)
- No multi-controlled gates need special handling

Toffoli circuits have fundamentally different cancellation patterns:
- CCX does NOT commute with CNOT sharing a control (not covered in `gates_commute()`)
- Cancellation requires matching ALL controls, not just target
- The MAJ/UMA pattern in ripple-carry adders has gates that look cancellable but are separated by essential intermediate gates

**Consequences:**
- Best case: optimizer is ineffective, Toffoli circuits have more gates than necessary (performance issue, not correctness)
- Worst case: optimizer incorrectly cancels or reorders gates, producing wrong results
- Subtle: optimizer might cancel the uncomputation Toffoli gates with the computation Toffoli gates if they happen to be adjacent after layer optimization

**Warning signs:**
- Toffoli circuit gate count is lower than expected after optimization
- Self-inverse gates (CCX) being unexpectedly removed
- Circuit works without optimizer but fails with optimizer enabled

**Prevention:**
- Disable optimizer for Toffoli circuits initially (set a flag, bypass `add_gate()` inverse check for X-type gates with NumControls > 0)
- Add Toffoli-specific commutation rules to `gates_commute()`: CCX(t,c1,c2) does NOT commute with CNOT(c1, c3) or CNOT(c2, c3)
- Extensive test: compare optimized vs unoptimized circuit outputs for all n-bit adder inputs where n <= 6
- Consider a separate Toffoli-aware optimization pass (gate cancellation based on Toffoli identity: CCX^2 = I)

**Phase to address:** After adder is working but before performance optimization. Run with optimizer disabled initially.

---

## Moderate Pitfalls

Issues that cause incorrect results in specific cases or significant performance problems.

### Pitfall 6: Schoolbook Multiplication Add-Subtract Sign Logic Error

**What goes wrong:** The schoolbook multiplication with controlled add-subtract circuits (per Gidney, arXiv:2410.00899) uses a clever trick: instead of n controlled additions, it uses n-1 controlled add-subtract operations where the control qubit selects addition or subtraction. This halves the Toffoli count (n-1 Toffoli gates per add-subtract vs 2n-1 per controlled adder). However, the sign convention is counterintuitive: when the control bit is |1>, the operation ADDS; when |0>, it SUBTRACTS. Getting this backwards produces a multiplication circuit that computes `a * (2^n - 1 - b)` instead of `a * b`.

**Why it happens:** The controlled add-subtract is equivalent to: `if ctrl: result += partial` else `result -= partial`. This works because the final uncomputation phase reverses the subtractions. But the partial product bit weights must be correctly shifted: bit j of b has weight 2^j, so the partial product `a * b_j * 2^j` must be added to the correct position in the result register. Getting the shift wrong by even one position produces a multiplication that is off by a factor of 2 for one bit.

**Consequences:**
- Multiplication returns wrong results for specific input pairs
- The error is width-dependent: works for n=2, fails for n=3+
- Hard to debug because the error pattern is not obvious (not just off-by-one in the result)

**Prevention:**
- Implement and verify schoolbook multiplication with plain controlled additions first (known-correct, just less efficient)
- Only switch to add-subtract variant after the plain version passes exhaustive testing for n=1..6
- Test: `a * b == b * a` for all (a,b) pairs at each width (commutativity check catches sign errors)
- Test: `a * 1 == a` and `a * 0 == 0` for all a (identity checks)

**Phase to address:** Multiplication implementation phase, after adder is verified.

---

### Pitfall 7: Qubit Count Explosion When Composing Toffoli Operations

**What goes wrong:** QFT multiplication (`QQ_mul` in `IntegerMultiplication.c`) uses 3n qubits (result, a, b) with no ancilla. Toffoli schoolbook multiplication uses at minimum 3n + O(n) qubits (result, a, b, plus ancilla for each addition). If the adder is CLA (O(n) ancilla per addition) and there are n additions in the multiplication, the total ancilla is O(n^2) unless ancilla are properly reused between additions. Even with ripple-carry (1 ancilla per addition), the ancilla reuse must be explicit.

The existing `__mul__` operator in `qint_arithmetic.pxi` (line 556) allocates a `result = qint(width=result_width)`. This allocates `result_width` fresh qubits. For QFT, that is the only allocation. For Toffoli multiplication, each internal addition also needs ancilla. If the ancilla are allocated via `allocator_alloc()` for each partial product addition and freed after, the peak qubit count is 3n + ancilla_per_add. But if the allocator does not reuse freed ancilla (current limitation for count > 1, line 94 of `qubit_allocator.c`), each addition allocates fresh ancilla and the total is 3n + n * ancilla_per_add.

**Why it happens:** The allocator reuses freed qubits only for `count == 1`. CLA adders need contiguous blocks. Ripple-carry needs 1 ancilla (works with reuse). But even with ripple-carry, the sequence-level ancilla allocation pattern (allocate in C sequence, free after sequence completes) differs from the operation-level pattern (allocate before multiplication, reuse across all partial products).

**Consequences:**
- 8-bit Toffoli multiplication: expected ~25 qubits (3*8 + 1 ancilla), actual could be 3*8 + 8*1 = 32 if ancilla not reused, or 3*8 + 8*O(8) = 88+ if CLA ancilla not reused
- ALLOCATOR_MAX_QUBITS (8192) limit hit much sooner than with QFT
- Memory usage increases proportionally

**Warning signs:**
- `allocator_get_stats()` shows `peak_allocated` much higher than expected
- `ancilla_allocations` stat grows linearly with problem size
- 32-bit multiplication segfault (existing known bug) may be exacerbated

**Prevention:**
- Fix `allocator_alloc()` to support contiguous block reuse from freed stack
- Implement ancilla pool pattern: pre-allocate ancilla block at multiplication start, pass to each internal addition, free once at end
- Document expected qubit counts per operation per width in a table
- Add assertion: `peak_allocated <= 4*width + constant` for multiplication

**Phase to address:** Ancilla management infrastructure phase, before multiplication implementation.

---

### Pitfall 8: Cached Sequence Mutation With Backend-Specific Sequences

**What goes wrong:** The existing QFT arithmetic caches sequences globally: `precompiled_QQ_add_width[bits]` (line 15 of `IntegerAddition.c`). The CQ_add function even MUTATES the cached sequence to update rotation angles (line 82: `add->seq[start_layer + i][...].GateValue = rotations[i]`). If Toffoli sequences are cached in the same global arrays, or if the backend selection is per-operation rather than per-program, the cache will serve wrong sequences.

**Why it happens:** QFT sequences are width-parameterized but backend-independent. Adding a Toffoli backend means the same width (e.g., 8-bit addition) has two different sequences. If both are cached under `precompiled_QQ_add_width[8]`, the first backend to be called wins and subsequent calls with the other backend get the wrong sequence.

**Consequences:**
- Switching backend mid-program silently uses cached sequence from wrong backend
- QFT sequence executed with Toffoli qubit layout, or vice versa
- Data corruption without crash

**Prevention:**
- Separate cache arrays: `precompiled_QQ_add_qft_width[65]` and `precompiled_QQ_add_toffoli_width[65]`
- OR: Add backend tag to sequence_t struct so cache lookup checks backend match
- Never allow backend switching mid-program in v1 -- set once at circuit initialization

**Phase to address:** Backend infrastructure phase.

---

### Pitfall 9: Division Algorithm Assumes In-Place Subtraction

**What goes wrong:** The existing division algorithm in `qint_division.pxi` (line 71-81) implements restoring division using `remainder -= trial_value` inside a `with can_subtract:` context. This in-place subtraction currently uses QFT `CQ_add` with inversion. With the Toffoli backend, in-place subtraction requires the adder to support inversion (running the sequence backwards). The `run_instruction()` function handles inversion by iterating layers backwards and negating `GateValue` (line 23 of `execution.c`). As noted in Pitfall 3, negating `GateValue` for Toffoli gates is incorrect.

Additionally, the restoring division algorithm requires *conditional* subtraction (controlled by the comparison result qubit). The controlled Toffoli adder (`cQQ_add_toffoli`) requires an additional control qubit position in the qubit layout. If the division assumes the QFT layout for controlled addition, the control qubit position will be wrong.

**Consequences:**
- Division and modulo operations produce wrong results with Toffoli backend
- The existing deferred bugs (BUG-MOD-REDUCE, BUG-DIV-02) may interact with Toffoli backend in unpredictable ways
- Division is used by right-shift (`__rshift__`, line 464 of `qint_arithmetic.pxi`), so shift operations also break

**Prevention:**
- Fix `reverse_circuit_range()` and `run_instruction()` inversion logic before enabling Toffoli division
- Test division separately from addition: ensure `a // b` works for all small cases
- Keep QFT as fallback for division until Toffoli division is verified

**Phase to address:** After adder and subtraction are verified, before multiplication/division.

---

### Pitfall 10: Layer Floor Mechanism Breaks With Toffoli Circuit Depth

**What goes wrong:** The `layer_floor` mechanism (line 75 of `circuit.h`) prevents the optimizer from scheduling gates before a certain layer. This is set by the Python layer before each operation (`(<circuit_s*>_circ).layer_floor = start_layer` in `qint_arithmetic.pxi` line 99). QFT addition has depth O(n^2) (QFT + CP rotations + IQFT). Toffoli ripple-carry has depth O(n). CLA has depth O(log n). The layer floor correctly prevents interleaving of operations in the QFT case. But with Toffoli circuits, the much shallower depth means more gates are packed into fewer layers. If two operations are performed sequentially (e.g., `a += b; a += c`), the optimizer might schedule the second operation's gates into the same layers as the first, despite the layer floor, because the Toffoli gates can be parallelized by the `minimum_layer()` function in `optimizer.c`.

More specifically: the layer floor is set to `start_layer` before the operation. But `minimum_layer()` (line 40) uses `max(layer_floor, last_occupied_layer)`. For QFT circuits, the last occupied layer is always >= layer_floor because the QFT uses many layers. For Toffoli circuits, ancilla qubits that are freshly allocated have `used_occupation_indices_per_qubit[ancilla] == 0`, meaning `smallest_layer_below_comp()` returns 0. The gate will be scheduled at `layer_floor` even if it should be after the previous operation's uncomputation.

**Consequences:**
- Gates from uncomputation phase interleave with gates from next operation
- Ancilla qubits used before they are uncomputed
- Silent wrong results

**Prevention:**
- After each Toffoli operation (including uncomputation), update `layer_floor` to `circ->used_layer`
- OR: Use barrier gates (no-op gates on all used qubits) between operations to prevent interleaving
- Test: dump circuit layer by layer, verify no Toffoli from operation B appears before uncomputation of operation A completes

**Phase to address:** Integration phase when Toffoli operations are wired into operator overloading.

---

## Minor Pitfalls

Issues that cause confusion, suboptimal performance, or require workarounds.

### Pitfall 11: Toffoli Decomposition Flag (`toff_decomp`) Is Vestigial

**What goes wrong:** The circuit_t structure has a `toff_decomp` field (line 71 of `circuit.h`) set to `DONTDECOMPOSETOFFOLI` (line 17 of `circuit_allocations.c`). The optimizer code mentions "check if multicontrolled gate needs to be decomposed" (line 164 of `optimizer.c`) but this is a comment with no implementation. The `DECOMPOSETOFFOLI` and `DONTDECOMPOSETOFFOLI` constants are defined but unused. When adding Toffoli-based arithmetic, developers may assume this flag controls whether CCX gates are decomposed into Clifford+T, but it does not.

**Prevention:**
- Document that `toff_decomp` is vestigial and does nothing
- Implement Toffoli decomposition as a separate post-processing pass if needed for fault-tolerance
- Do not rely on this flag for backend selection

---

### Pitfall 12: MAXCONTROLS=2 Limits Multi-Controlled Toffoli Decomposition

**What goes wrong:** The `MAXCONTROLS` constant is 2 (line 31 of `types.h`), and the `Control[]` array in `gate_t` is fixed at size 2. Multi-controlled X gates (MCX) with > 2 controls use `large_control` (a dynamically allocated array). The Toffoli adder itself uses at most CCX (2 controls), so this is not a problem for the adder. But controlled Toffoli addition (`cQQ_add_toffoli`) needs an additional control qubit, making some internal gates CCCX (3 controls). These must use the `large_control` path, which is slower and has different memory management.

**Prevention:**
- Avoid CCCX gates in controlled Toffoli adders: decompose `c-CCX` into CCX + ancilla using standard decomposition
- Test `large_control` path with Toffoli circuits: ensure `run_instruction()` correctly maps `large_control` indices (it does, per lines 31-37 of `execution.c`, but this path is rarely exercised)
- Consider: for controlled Toffoli adder, decompose the overall control into the adder structure rather than adding control to every Toffoli gate

---

### Pitfall 13: Hardcoded Sequence Width Limit (HARDCODED_MAX_WIDTH=16) May Need Separate Toffoli Sequences

**What goes wrong:** The current hardcoded sequences (`add_seq_1.c` through `add_seq_16.c`) are QFT-specific. If Toffoli sequences are also hardcoded for performance, they need separate files and a separate dispatch mechanism. The existing `get_hardcoded_QQ_add(bits)` function would need a Toffoli counterpart.

**Prevention:**
- Do NOT hardcode Toffoli sequences initially -- generate dynamically
- Hardcode only after the dynamic generation is verified correct for all widths 1-16
- Keep QFT and Toffoli hardcoded sequences in separate directories

---

### Pitfall 14: `gates_are_inverse()` Exact Floating-Point Comparison

**What goes wrong:** The `gates_are_inverse()` function (line 454 of `gate.c`) uses exact equality `G1->GateValue != -G2->GateValue` for P gates. This works for QFT circuits where gate values are computed from `M_PI / pow(2, k)` using the same formula. For Toffoli circuits, all gate values are 1 (for X/CX/CCX). The exact comparison `1 != -1` is `true`, so self-inverse detection fails via this branch. But the else branch (line 457) `G1->GateValue != G2->GateValue` correctly identifies `1 == 1` as matching for non-P gates. This is actually fine -- but the code structure is confusing and a future developer might "fix" the P branch without understanding it handles a different case.

**Prevention:**
- Add a comment explaining the two branches: P gates use additive inverse (theta vs -theta), all other gates use identity comparison
- Consider adding explicit `gate_is_self_inverse()` helper function

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Ripple-carry adder implementation | Off-by-one in MAJ/UMA gate sequences | Exhaustive test for n=1..6, compare with classical addition |
| CLA adder implementation | Generate/propagate tree index errors (Pitfall 2) | Start with ripple-carry, verify, then implement CLA |
| Ancilla allocation infrastructure | Contiguous block reuse not supported (Pitfall 7) | Fix `allocator_alloc()` for count>1 reuse before CLA |
| Backend selection | Cached sequence serves wrong backend (Pitfall 8) | Separate cache arrays per backend |
| Hot path integration | Qubit layout mismatch (Pitfall 4) | Separate hot path per backend |
| Uncomputation integration | `reverse_circuit_range()` negates Toffoli GateValue (Pitfall 3) | Fix BEFORE any Toffoli integration |
| Schoolbook multiplication | Add-subtract sign convention (Pitfall 6) | Plain controlled addition first, add-subtract later |
| Division/modulo | Inversion + controlled layout bugs (Pitfall 9) | Test separately, keep QFT fallback |
| Optimizer compatibility | Wrong cancellation/reordering (Pitfall 5) | Disable optimizer for Toffoli initially |
| Layer scheduling | Layer floor too permissive (Pitfall 10) | Barrier gates between operations |
| Interaction with BUG-DIV-02 | MSB comparison leak + Toffoli ancilla = compound bug | Fix BUG-DIV-02 before Toffoli division |
| Interaction with 32-bit mul segfault | Qubit count explosion with Toffoli ancilla | Fix allocator block reuse first |

## Interaction With Known Deferred Bugs

| Existing Bug | Interaction with Toffoli Arithmetic | Risk Level |
|-------------|--------------------------------------|------------|
| BUG-MOD-REDUCE | Modular reduction uses subtraction; Toffoli subtraction must work first | HIGH |
| BUG-COND-MUL-01 | Controlled multiplication layout already wrong; Toffoli adds more control complexity | HIGH |
| BUG-DIV-02 (MSB comparison leak) | MSB comparison uses Toffoli gates already (via `IntegerComparison.c`); may interact with new Toffoli adder ancilla | MEDIUM |
| BUG-WIDTH-ADD | Width mismatch between operands; Toffoli adder has strict width requirements for ancilla count | MEDIUM |
| 32-bit mul segfault | Likely caused by MAXLAYERINSEQUENCE (10000) overflow; Toffoli circuits have different depth profile | LOW (may actually improve) |

## Sources

- [Draper-Kutin-Rains-Svore CLA Adder](https://arxiv.org/abs/quant-ph/0406142) - O(log n) depth, O(n) ancilla
- [Cuccaro et al. Ripple-Carry Adder](https://arxiv.org/abs/quant-ph/0410184) - O(n) depth, 1 ancilla, 2n-1 Toffoli gates
- [Gidney: Schoolbook Multiplication with Fewer Toffoli Gates](https://arxiv.org/abs/2410.00899) - Controlled add-subtract trick
- [Comprehensive Study of Quantum Arithmetic Circuits](https://arxiv.org/html/2406.03867v1) - Survey of adder/multiplier designs
- [QFT Addition Simplified to Toffoli Addition](https://arxiv.org/abs/2209.15193) - Equivalence between QFT and Toffoli approaches
- [Optimal Toffoli-Depth Quantum Adder](https://arxiv.org/abs/2405.02523) - Recent optimizations
- Codebase inspection: `c_backend/src/IntegerAddition.c`, `c_backend/src/optimizer.c`, `c_backend/src/execution.c`, `c_backend/src/qubit_allocator.c`, `src/quantum_language/qint_arithmetic.pxi`, `c_backend/include/types.h`, `c_backend/include/circuit.h`
