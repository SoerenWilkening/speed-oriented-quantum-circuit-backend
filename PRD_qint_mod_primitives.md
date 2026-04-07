# PRD: Modular Arithmetic C Primitives for `qint_mod`

Beads tag: `refactor-qint-mod-primitives` (placeholder, not yet registered)

## 1. Goal & Motivation

The C backend currently exposes only four modular primitives
(`qc_toffoli_mod_reduce`, `qc_toffoli_mod_add_cq`, `qc_toffoli_mod_add_qq`,
`qc_toffoli_mod_mul_cq`). The Python layer `qint_mod` (in sibling
`quantum-cython-types`) advertises 12 operations and silently no-ops at
import time when the missing nine symbols cannot be found in `libquantum`.
This blocks every modular workflow downstream — most importantly Shor's
algorithm, whose `__rpow__` (controlled modular exponentiation) requires a
ladder of controlled classical modular multiplications (`cmod_mul_cq`).

This PRD defines the missing nine primitives, their public signatures, and
their construction at a level sufficient for the implementation planner.
**Top priority:** the Shor-critical chain `cmod_add_cq → cmod_mul_cq`
(Phase A in the companion PLAN).

## 2. Pre-existing Work Discovered

Reading `src/toffoli_mod_reduce.c` reveals that a static helper
`toffoli_cmod_add_cq_internal` already implements the controlled Beauregard
8-step block (lines 163-229) and is used internally by `qc_toffoli_mod_mul_cq`.
**This helper just needs to be exported with a public wrapper** —
no fresh construction required for `cmod_add_cq`. This significantly
reduces the work for the Shor-critical step.

The existing `qc_toffoli_mod_mul_cq` (lines 317-353) contains a `c == 1`
special case that emits `qc_circuit_cx(value[i], result[i])`. This bare
CNOT is **not** gated by any external control, which is correct for the
uncontrolled variant but is a pitfall for the controlled variant — see
§4.5 below.

No other primitives in the missing list appear to already exist.

## 3. Required Public Signatures

All functions follow existing conventions: `circuit_ctx_t*` first,
qubit-list pointers as `const uint32_t*`, widths as `uint32_t`,
classical operands as `int64_t`, modulus as `int64_t`, controls as
`uint32_t`. All return `qc_error_t`. All declared `QC_API` in
`include/quantum_circuit.h` directly after the existing mod block
(insert after line 695).

```c
/* --- Modular subtraction --- */
QC_API qc_error_t qc_toffoli_mod_sub_cq(circuit_ctx_t *ctx,
        const uint32_t *value, uint32_t value_bits,
        int64_t subtrahend, int64_t modulus);

QC_API qc_error_t qc_toffoli_mod_sub_qq(circuit_ctx_t *ctx,
        const uint32_t *value, uint32_t value_bits,
        const uint32_t *other, uint32_t other_bits,
        int64_t modulus);

/* --- Controlled modular addition --- */
QC_API qc_error_t qc_toffoli_cmod_add_cq(circuit_ctx_t *ctx,
        const uint32_t *value, uint32_t value_bits,
        int64_t addend, int64_t modulus, uint32_t ext_ctrl);

QC_API qc_error_t qc_toffoli_cmod_add_qq(circuit_ctx_t *ctx,
        const uint32_t *value, uint32_t value_bits,
        const uint32_t *other, uint32_t other_bits,
        int64_t modulus, uint32_t ext_ctrl);

/* --- Controlled modular subtraction --- */
QC_API qc_error_t qc_toffoli_cmod_sub_cq(circuit_ctx_t *ctx,
        const uint32_t *value, uint32_t value_bits,
        int64_t subtrahend, int64_t modulus, uint32_t ext_ctrl);

QC_API qc_error_t qc_toffoli_cmod_sub_qq(circuit_ctx_t *ctx,
        const uint32_t *value, uint32_t value_bits,
        const uint32_t *other, uint32_t other_bits,
        int64_t modulus, uint32_t ext_ctrl);

/* --- Controlled modular multiplication (Shor critical) --- */
QC_API qc_error_t qc_toffoli_cmod_mul_cq(circuit_ctx_t *ctx,
        const uint32_t *value, uint32_t value_bits,
        const uint32_t *result, uint32_t result_bits,
        int64_t multiplier, int64_t modulus, uint32_t ext_ctrl);

/* --- Quantum-quantum modular multiplication --- */
QC_API qc_error_t qc_toffoli_mod_mul_qq(circuit_ctx_t *ctx,
        const uint32_t *a, uint32_t a_bits,
        const uint32_t *b, uint32_t b_bits,
        const uint32_t *result, uint32_t result_bits,
        int64_t modulus);

QC_API qc_error_t qc_toffoli_cmod_mul_qq(circuit_ctx_t *ctx,
        const uint32_t *a, uint32_t a_bits,
        const uint32_t *b, uint32_t b_bits,
        const uint32_t *result, uint32_t result_bits,
        int64_t modulus, uint32_t ext_ctrl);
```

Naming/parameter ordering rationale:
- `value`/`other` mirrors the existing `qc_toffoli_mod_add_qq`.
- `result`/`value` ordering on `cmod_mul_cq` mirrors the existing
  `qc_toffoli_mod_mul_cq` exactly so the Cython binding can be
  generated mechanically.
- `ext_ctrl` is always the trailing parameter, matching
  `qc_toffoli_cqq_add`, `qc_toffoli_cdivmod_*`, etc.

## 4. Beauregard Construction Sketches

All constructions use an `(n+1)`-bit working register (one high/sign bit),
**not** `2n` bits. Every ancilla is uncomputed before return — no
persistent leaks (the planner must verify in code review).

### 4.1 `mod_sub_cq` (CQ subtract — trivial classical adapter)
`value -= s mod N` is equivalent to `value += ((N - (s mod N)) mod N)`.
Because `s` is classical, the substitution is computed at compile time
and is exact for **any** `N`. Implementation: reduce `s` into `[0, N)`,
compute `s' = (N - s) mod N`, then call `qc_toffoli_mod_add_cq` with
`s'`. No ancilla, no construction risk.

### 4.2 `mod_sub_qq` (QQ subtract — DAGGER OF `mod_add_qq`)
**Critical correctness note:** the naïve "two's-complement `other` into
an ancilla then `mod_add_qq`" strategy is **WRONG** for non-power-of-two
`N`. Two's complement on an `n`-bit register computes `2^n - other`, not
`N - other`. Feeding that into `mod_add_qq` yields
`(value + 2^n - other) mod N`, which equals `(value - other) mod N`
**only when `2^n ≡ 0 (mod N)`** — i.e., only when `N` is a power of two.
For every prime modulus (and every modulus of cryptographic interest)
this would silently produce the wrong answer.

**Correct construction (chosen): inverse-of-`mod_add_qq`.**
`mod_add_qq` is a unitary: running its 8-step Beauregard block
**backwards** (each step replaced by its dagger, in reverse order)
implements `value := (value - other) mod N` exactly. Concretely, the
existing 8-step structure of `qc_toffoli_mod_add_qq` (see
`src/toffoli_mod_reduce.c:231-311`) is:

1. Pad `other` to `n+1` bits (CNOT copies into a wide register).
2. `wide_reg += value` via `qc_dynamic_qq_add`.
3. Subtract `N` (constant) from `wide_reg`.
4. Comparison: load `cmp_anc` from the high bit of `wide_reg`.
5. Conditional add of `N` to `wide_reg`, controlled on `cmp_anc`.
6. Subtract `value` from `wide_reg` (uncompute step 2 partially) and
   uncompute `cmp_anc`.
7. Add `value` back.
8. `value += other` via the wide register.

The dagger of this block (`mod_sub_qq`) runs the same 8 steps in
reverse with each gate replaced by its inverse. **All primitive names
below are internal helpers from `src/internal.h`; none of these are
public `qc_toffoli_*` symbols and the implementer must not introduce
new public `qc_toffoli_*sub` wrappers.**

- Step 8 dagger: `wide_reg -= other` via the **internal**
  `qc_dynamic_qq_sub(ctx, src, wide_reg, n+1)` (NOT a public symbol).
- Step 7 dagger: `cx(wide_reg[n], cmp_anc)` (self-inverse).
- Step 6 dagger: `x(cmp_anc)` (self-inverse).
- Step 5 dagger: `qc_dynamic_qq_add(ctx, src, wide_reg, n+1)`
  (inverse of step-5's `qq_sub`).
- Step 4 dagger: conditional **subtract** of `N` controlled on
  `cmp_anc`, expressed as `qc_dynamic_ccq_add(ctx, wide_reg, n+1,
  -modulus, cmp_anc)` (the only `ccq_sub` available is "add the
  negated constant"; there is no `qc_dynamic_ccq_sub` symbol).
- Step 3 dagger: `cx(wide_reg[n], cmp_anc)` (self-inverse).
- Step 2 dagger: **add** `N` (constant) back to `wide_reg` via
  `qc_dynamic_cq_add(ctx, wide_reg, n+1, modulus)` (inverse of the
  original `cq_add(..., -modulus)`; again no `cq_sub` exists).
- Step 1 dagger: `qc_dynamic_qq_sub(ctx, src, wide_reg, n+1)` to
  uncompute the original `qq_add` of step 1; followed by uncompute of
  the wide-register padding.

**Comparison-ancilla ownership under the dagger.** `cmp_anc` is
allocated at the **top** of `mod_add_qq` (before step 1) and freed at
the **bottom** (after step 8). Its alloc/free positions are invariant
under daggering: `mod_sub_qq` allocates `cmp_anc` at primitive entry
and frees it at primitive exit, exactly like `mod_add_qq`. The
intermediate steps recompute and uncompute the qubit's `|0>↔|1>`
state, but no step in the daggered sequence performs the alloc or
free. The implementer **must** assert
`qc_circuit_alloc_stats(ctx).current_in_use` is identical at primitive
entry and exit (strict `==`); see §6.3.

Implementation guidance: rather than hand-writing daggers gate-by-gate,
the implementer **may** factor the existing `qc_toffoli_mod_add_qq` into
a private helper that takes a `bool inverse` flag. The flag swaps each
step's add↔sub form (i.e., `qc_dynamic_qq_add` ↔ `qc_dynamic_qq_sub`,
and `qc_dynamic_cq_add(..., +c)` ↔ `qc_dynamic_cq_add(..., -c)`,
likewise for `ccq_add`) **but preserves the alloc/free positions of
`cmp_anc` and the wide-register padding**. Ancilla bookkeeping is
invariant under the dagger. Either approach is acceptable so
long as the resulting circuit is the unitary inverse and the test in §6
on a non-power-of-two modulus passes.

**Forbidden:** any construction that materialises `2^n - other` in an
ancilla and feeds it to `mod_add_qq`. Any construction that assumes
`2^n mod N == 0`. Any construction that does not pass the `N=15`
underflow test.

### 4.3 `cmod_add_cq` (Shor blocker)
Already exists as `toffoli_cmod_add_cq_internal`. Add a thin public
wrapper that just forwards arguments and returns its result. **No new
gate construction.**

### 4.4 `cmod_add_qq`
Same 8-step structure as `qc_toffoli_mod_add_qq`, but the operand-folding
steps are gated on `ext_ctrl`. **All add/sub primitives invoked here
are internal helpers from `src/internal.h`. None of `qc_toffoli_cqq_sub`,
`qc_toffoli_ccq_sub`, `qc_toffoli_cq_sub`, or `qc_toffoli_qq_sub`
exist** in `include/quantum_circuit.h` — only `qc_toffoli_cqq_add`
and `qc_toffoli_ccq_add` are public. The existing `qc_toffoli_mod_add_qq`
deliberately uses `qc_dynamic_*` internal helpers for exactly this
reason; this primitive follows the same convention. **The
implementation file `src/toffoli_mod_extras.c` MUST `#include
"internal.h"` to access these symbols. Do NOT introduce new public
`qc_toffoli_*sub` wrappers — that is scope creep.**

Tightened wording (avoid spurious ancillae):

- Step 1 (the `qq_add(src, wide_reg, n+1)` against the wide register)
  becomes directly **singly controlled on `ext_ctrl`** via the
  internal `qc_dynamic_cqq_add(ctx, src, wide_reg, n+1, ext_ctrl)`.
  The CNOT-copy that pads `other` into `src` becomes `ccx(ext_ctrl,
  other[i], src[i])` (an extra-control CNOT-copy gated on `ext_ctrl`).
  **No AND-ancilla.**
- Step 2 (constant subtract of `N`): expressed as a controlled
  constant add of `-modulus` via the internal
  `qc_dynamic_ccq_add(ctx, wide_reg, n+1, -modulus, ext_ctrl)`. There
  is no `qc_dynamic_ccq_sub` symbol; the constant is negated instead.
  **No AND-ancilla.**
- Step 3 (`cx(wide_reg[n], cmp_anc)` sign copy) becomes
  `ccx(ext_ctrl, wide_reg[n], cmp_anc)`. **No AND-ancilla.**
- Step 4 (conditional add of `N` controlled on `cmp_anc`) is the
  **only** step that needs an AND of `(ext_ctrl, cmp_anc)`. Allocate
  one AND-ancilla here, AND `ext_ctrl ∧ cmp_anc` into it via `ccx`,
  then call the internal `qc_dynamic_ccq_add(ctx, wide_reg, n+1,
  modulus, and_anc)`, then uncompute the AND-ancilla with the same
  `ccx` and free.
- Step 5 (`qq_sub` of `src` from `wide_reg`) becomes
  `qc_dynamic_cqq_sub(ctx, src, wide_reg, n+1, ext_ctrl)` (internal,
  declared in `src/internal.h`). **No AND-ancilla.**
- Step 6 (`x(cmp_anc)`) becomes `cx(ext_ctrl, cmp_anc)`.
  **No AND-ancilla.**
- Step 7 (`cx(wide_reg[n], cmp_anc)`) becomes
  `ccx(ext_ctrl, wide_reg[n], cmp_anc)`. **No AND-ancilla.**
- Step 8 (`qq_add` of `src` into `wide_reg`) becomes
  `qc_dynamic_cqq_add(ctx, src, wide_reg, n+1, ext_ctrl)`.
  **No AND-ancilla.**

Net additional ancilla cost over `mod_add_qq`: **exactly one** (the
single AND-ancilla in step 4).

### 4.5 `cmod_sub_cq` and `cmod_sub_qq`
- `cmod_sub_cq`: trivial — call `cmod_add_cq` with `(N - (s mod N)) mod N`.
- `cmod_sub_qq`: dagger of `cmod_add_qq`, exactly as `mod_sub_qq` is the
  dagger of `mod_add_qq`. Cannot land until both `cmod_add_qq` (Phase B)
  and the corrected `mod_sub_qq` exist.

### 4.6 `cmod_mul_cq` (Shor critical path — controlled Beauregard ladder)
Mirrors `qc_toffoli_mod_mul_cq` but every inner `cmod_add_cq` is now
**doubly controlled** on `(ext_ctrl, value[j])`. Realize the double
control by allocating a single AND-ancilla per loop iteration:

```
for j in 0..n-1:
    if shifted != 0:
        and_anc = alloc()
        ccx(ext_ctrl, value[j], and_anc)
        cmod_add_cq(result, result_bits, shifted, modulus, and_anc)
        ccx(ext_ctrl, value[j], and_anc)
        free(and_anc)
    shifted = (shifted * 2) % modulus
```

**`c == 1` special case (CRITICAL FIX over the uncontrolled variant):**
the existing `qc_toffoli_mod_mul_cq` emits a bare
`qc_circuit_cx(value[i], result[i])`. In the controlled wrapper this
**must become**

```
ccx(ext_ctrl, value[i], result[i])
```

for `i in 0..min(n, result_bits)-1`. A bare `cx` would copy `value` into
`result` regardless of `ext_ctrl`, silently breaking the controlled
contract whenever the multiplier reduces to 1 mod N.

**`c == 0` special case:** no-op (independent of `ext_ctrl`).

### 4.7 `mod_mul_qq` and `cmod_mul_qq`
Structurally a Beauregard ladder over the bits of `a`: for each bit `j`
of `a`, accumulate `(b * 2^j) mod N` into `result` using `cmod_add_qq`
controlled on `a[j]` (and on `ext_ctrl` for the controlled variant).

**Critical correctness note (replaces previous "shift the index list"
proposal):** the previous draft proposed encoding `(b * 2^j) mod N` as a
re-indexed view of `b`'s qubits (an LSB shift on the qubit list). **This
is wrong.** It only works when `2^j mod N == 2^j`, which fails the
moment `2^j ≥ N`. Modular reduction is **not** a permutation of basis
states; you cannot relabel qubits to mean a modularly-shifted value.

**Correct construction: explicit modular doubling helper.**
Allocate an `(n+1)`-bit "shifted-`b`" ancilla register `b_shift`. The
ladder is:

1. Initialise `b_shift := b` via CNOT-copy (bit-by-bit `cx(b[i],
   b_shift[i])`).
2. For `j = 0, 1, ..., n - 1`:
   a. `cmod_add_qq(result, b_shift, modulus)` controlled on `a[j]`
      (and on `ext_ctrl` for the controlled variant; gate via the
      same single-AND-ancilla pattern as Step 4.4 / 4.6).
   b. If `j < n - 1`: apply **modular doubling** to `b_shift` in place:
      `b_shift := (2 * b_shift) mod N`.

   **`j = 0` boundary (executor must not double on iteration 0).** On
   iteration `j = 0`, `b_shift` already equals `b` from the CNOT-copy
   in step 1; the `cmod_add_qq(result, b_shift, modulus)` call
   accumulates `b * a[0]` directly. **Doubling happens AFTER the add,
   only when `j < n - 1`,** to prepare `b_shift = 2b mod N` for
   iteration `j = 1`. The executor must not pre-double `b_shift`
   before entering the loop.
3. Uncompute `b_shift` at the end of the ladder by running steps 2b and
   1 in reverse (i.e., undouble `n - 1` times, then uncompute the
   CNOT-copy from step 1).

**Modular doubling in place** (`reg := (2 * reg) mod N` for an
`(n+1)`-bit register `reg`) is implemented as:
- Allocate an `(n+1)`-bit temporary `tmp`, CNOT-copy `reg` into `tmp`.
- Call `qc_toffoli_mod_add_qq(reg, tmp, modulus)` so that
  `reg += tmp = reg`, i.e., `reg := (reg + reg) mod N`.
- Uncompute `tmp` (CNOT-copy back, then free).

The CNOT-copy intermediate is required because `mod_add_qq` cannot
self-add a register in place (the operand and target must be distinct
qubit lists). This modular doubling helper is the natural unit to test
in isolation; the planner extracts it as a private helper used by both
`mod_mul_qq` and `cmod_mul_qq`.

Ancilla budget for the QQ multiplication ladder: `(n+1)` qubits for
`b_shift`, plus `(n+1)` qubits for the doubling `tmp` (allocated and
freed inside each doubling step), plus the per-iteration AND-ancilla
for the controlled variant. All freed before return.

## 5. Out of Scope

- **No changes to `arithmetic_dispatch.c`.** These primitives are
  invoked directly by the Cython layer; the dispatcher does not need
  new entries unless the qint API later asks for mode switching.
- **No Cython work.** The sibling `quantum-cython-types` will bind
  the new symbols in a separate planner/executor pass.
- **No QFT-mode equivalents.** Toffoli-mode only for now (matches the
  existing four primitives).
- **The persistent `cmp_anc` leak in `qc_toffoli_mod_reduce` is OUT OF
  SCOPE.** Confirmed by orchestrator; will be filed as a separate beads
  issue against this package and is not addressed here.
- **No 2n-bit "multiply then reduce" fallback.** Forbidden.
- **No in-place QQ multiplication.** `mod_mul_qq` is out-of-place,
  mirroring `mod_mul_cq`.

## 6. Success Criteria

1. All nine new symbols are exported from `libquantum.so` and visible
   via `nm`.
2. Each primitive has a unit test (C, in `tests/`) that:
   a. Builds a circuit invoking the primitive on a small register
      (n=3 or n=4, small modulus).
   b. Asserts `qc_circuit_gate_count() > 0` (no silent no-op).
   c. Exports to QASM and runs through the existing
      `test_qasm_sim_toffoli.py` simulation harness, asserting the
      output register encodes the correct modular result for at least
      three input values, and for controlled variants both
      `ext_ctrl=|0>` (no-op) and `ext_ctrl=|1>` (op applied) cases.
3. **Ancilla balance (strict equality):** for every primitive,
   ```
   qc_circuit_alloc_stats(ctx).current_in_use   /* after */
       == qc_circuit_alloc_stats(ctx).current_in_use   /* before */
   ```
   The assertion **must** use `==`, not `<=`. Asserted in every unit
   test, both ctrl polarities where applicable.
4. **Non-power-of-two modulus regression test for `mod_sub_qq`:**
   exercise `N = 15` (or `N = 21`) with at least one input pair where
   `value < other`, so the underflow path is hit. The test fails
   loudly if any implementer falls back to the two's-complement
   strategy.
5. **`cmod_mul_cq` controlled regression cases.** All three of the
   following must be present:
   a. **`multiplier mod N == 1`, `ext_ctrl = |0>`:** invoke with
      `multiplier = 1` (and a second case with `multiplier = N + 1`
      so that `multiplier mod N == 1` non-trivially), `ext_ctrl = 0`;
      assert `result` is **unchanged** for every input value. Fails
      loudly if the `c == 1` branch uses bare `cx` instead of
      `ccx(ext_ctrl, value[i], result[i])`.
   b. **`multiplier > N`:** invoke with `multiplier = N + k` for some
      small `k > 1` (e.g., `N = 5`, `multiplier = 8`), both
      `ext_ctrl` polarities; assert the result matches `(value * (k))
      mod N` when `ctrl = 1`, exercising the internal classical
      reduction `multiplier mod N`.
   c. **`value = 0`:** invoke with the input `value` register
      prepared in `|0>`. With `ctrl = 1` the result must be `0`
      (multiplicative identity from the value side); with `ctrl = 0`
      it must be a no-op (`result` unchanged from its initial
      contents). Both polarities asserted.
6. No regression in existing `test_toffoli_*` tests.
7. The Shor-critical chain `cmod_add_cq → cmod_mul_cq` lands in Phase A
   (steps 1 + 2) so the Layer 2 (Cython) team can begin binding
   immediately, in parallel with Phase B.

## 7. Risks

- The QQ controlled variants need careful AND-ancilla management; the
  existing `toffoli_cmod_add_cq_internal` is the canonical pattern and
  must be followed. **Only the conditional-`N`-add step needs an
  AND-ancilla** (see §4.4); all other steps are singly controlled on
  `ext_ctrl`.
- `mod_sub_qq` correctness hinges on faithfully running `mod_add_qq`
  backwards. The non-power-of-two regression test (§6.4) is the
  load-bearing guard.
- `mod_mul_qq` ancilla bookkeeping for the `b_shift` register and
  in-loop modular doubling is the largest construction; the planner
  may split it across two steps if it threatens the 500-LOC cap.
- Negative-classical operand handling for `mod_sub_cq` must reduce
  `subtrahend mod N` first (matches existing `mod_add_cq` behaviour).

## 8. Symbol Provenance

Every C symbol referenced in §4 has been verified against the headers
in the repo. Reviewer spot-check table:

| Symbol                          | Header                       | Visibility | Notes |
|---------------------------------|------------------------------|------------|-------|
| `qc_toffoli_mod_add_cq`         | `include/quantum_circuit.h`  | public     | existing |
| `qc_toffoli_mod_add_qq`         | `include/quantum_circuit.h`  | public     | existing |
| `qc_toffoli_mod_mul_cq`         | `include/quantum_circuit.h`  | public     | existing |
| `qc_toffoli_mod_reduce`         | `include/quantum_circuit.h`  | public     | existing |
| `qc_toffoli_cqq_add`            | `include/quantum_circuit.h` (line 579) | public | existing |
| `qc_toffoli_ccq_add`            | `include/quantum_circuit.h` (line 586) | public | existing |
| `qc_dynamic_qq_add`             | `src/internal.h` (line 340)  | internal   | existing |
| `qc_dynamic_qq_sub`             | `src/internal.h` (line 344)  | internal   | existing |
| `qc_dynamic_cqq_add`            | `src/internal.h` (line 348)  | internal   | existing |
| `qc_dynamic_cqq_sub`            | `src/internal.h` (line 353)  | internal   | existing |
| `qc_dynamic_cq_add`             | `src/internal.h` (line 358)  | internal   | existing; constant subtract is `qc_dynamic_cq_add(..., -c)` |
| `qc_dynamic_ccq_add`            | `src/internal.h` (line 362)  | internal   | existing; controlled constant subtract is `qc_dynamic_ccq_add(..., -c, ctrl)` |
| `qc_circuit_cx`, `_ccx`, `_x`   | `include/quantum_circuit.h`  | public     | existing |
| `qc_qubit_alloc`, `_alloc_n`, `_free`, `_free_n` | `include/quantum_circuit.h` | public | existing |
| `toffoli_cmod_add_cq_internal`  | `src/toffoli_mod_reduce.c`   | static     | existing — Step 1 wraps it |

**Symbols intentionally NOT used (do not exist; do not introduce):**
`qc_toffoli_cqq_sub`, `qc_toffoli_ccq_sub`, `qc_toffoli_cq_sub`,
`qc_toffoli_qq_sub`, `qc_dynamic_cq_sub`, `qc_dynamic_ccq_sub`. Any
constant subtraction is expressed as adding the negated constant via
the corresponding `*_add` helper. Any controlled QQ subtraction uses
`qc_dynamic_cqq_sub`. Any uncontrolled QQ subtraction uses
`qc_dynamic_qq_sub`.

The new public symbols introduced by this PRD (§3) are the only
additions to `include/quantum_circuit.h`.
