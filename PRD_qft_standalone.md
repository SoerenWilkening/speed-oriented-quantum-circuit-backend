# PRD: Standalone QFT / Inverse-QFT Sequence Builders

## 1. Goal & Motivation

Expose **bare** Quantum Fourier Transform and inverse QFT as first-class
public sequence builders in `circuit-c-backend`, addressed by an explicit
array of qubit indices.

These will be consumed by `quantum-cython-types` to expose
`qint.qft()` / `qint.iqft()` to Python users. A standalone QFT is a
fundamental building block for Shor's algorithm, quantum phase
estimation, and similar primitives that the higher-level packages need
to express directly — independent of the existing QFT-based
arithmetic builders.

Today there is **no public way** to obtain a bare QFT from this
package. The only QFT code lives as `static` helpers
(`qc_seq_qft`, `qc_seq_qft_inv`) inside `src/qft_addition.c`, and they
are file-private and only used by the internal addition / multiplication
sequence builders. They also assume an implicit "contiguous register
starting at 0" layout, which is not what a public API should look like.

## 2. Background

- `src/qft_addition.c` lines 145–179 contain the textbook QFT and
  inverse-QFT generators used by `qc_arith_qq_add_seq` etc. They:
  - take only a width `n`, no qubit array;
  - target qubits `[0 .. n-1]` of the sequence;
  - emit Hadamards + controlled phase rotations in `2n-1` layers;
  - use the **no-final-swaps** convention (matches what the higher
    Python layer expects).
- The public sequence-builder pattern in this package is well
  established: every builder returns a `qc_sequence_t *` and is
  declared in `include/quantum_circuit.h` with the `QC_API`
  macro. Callers compose these via `qc_run_instruction(ctx, seq,
  qmap, invert)` which already handles qubit remapping and inversion.
- `qc_arith_mode_t::QC_ARITH_QFT` is a *mode selector* for arithmetic
  dispatch, **not** an IR opcode. The C backend has no per-sequence
  opcode enum; sequences are opaque `qc_sequence_t` objects. So "new
  IR opcode" lives only in `quantum-cython-types`; on the C side this
  PRD only adds new public sequence builders + thin convenience
  wrappers.

## 3. Requirements

### 3.1 Functional

1. New public sequence builder `qc_qft_seq(int n)` returning a
   `qc_sequence_t *` containing a textbook QFT on virtual qubits
   `[0 .. n-1]`, no final swaps, MSB-first processing — bit-for-bit
   identical to the rotation pattern of the existing static helper.
2. New public sequence builder `qc_iqft_seq(int n)` with the
   inverse pattern (matching the existing static `qc_seq_qft_inv`).
3. New public convenience wrappers:
   - `qc_error_t qc_qft (circuit_ctx_t *ctx, const uint32_t *qubits, uint32_t n);`
   - `qc_error_t qc_iqft(circuit_ctx_t *ctx, const uint32_t *qubits, uint32_t n);`
   These build the sequence, run it via `qc_run_instruction` with
   `qubits` as the qmap, and free the sequence. They are the entry
   points the Cython layer will bind.
4. The wrappers accept an **arbitrary** array of qubit indices — they
   need not be contiguous. The mapping from sequence-local indices
   `[0..n-1]` to hardware qubits is handled by `qc_run_instruction`'s
   existing qubit-array remapping; the builder does not need new logic.
5. Edge cases:
   - `n == 1` → emits a single Hadamard on `qubits[0]` (sequence has
     one layer with one gate).
   - `n == 0` → silent no-op: builder returns a valid empty
     `qc_sequence_t *` (zero layers, zero gates); the wrapper returns
     `QC_OK` without calling `qc_run_instruction`. Passing `n==0` is
     not an error.
   - `qubits == NULL` with `n > 0` → `QC_ERR_NULL`.
   - `n > 64` (matches existing width cap used elsewhere in this
     package) → `QC_ERR_WIDTH`.
6. Conventions: **no final swaps**. MSB-first traversal, matching
   the existing static helpers exactly (see section 6).

### 3.2 Non-functional

- `qc_qft_seq` / `qc_iqft_seq` must be **independent** of
  `qft_addition.c`. They live in a new file (see section 5) and
  duplicate the rotation pattern. The static helpers in
  `qft_addition.c` must remain bit-for-bit unchanged.
- Both new builders must produce sequences whose `total_qubits` field
  is set to `n` and `total_gate_count` is computed via
  `qc_sequence_compute_total_gate_count`.
- Round-trip identity: applying `qc_qft` followed by `qc_iqft` on the
  same qubit array must reduce to identity (validated via simulation
  against basis states).
- Existing `qc_arith_*` paths must continue to pass their tests
  unchanged.
- Memory: builders use the existing public `qc_sequence_alloc` /
  `qc_sequence_free` lifecycle (or, if needed, a local fixed-capacity
  helper analogous to `qft_sequence_alloc`).

## 4. Success Criteria

1. `include/quantum_circuit.h` declares the four new public symbols
   (`qc_qft_seq`, `qc_iqft_seq`, `qc_qft`, `qc_iqft`).
2. New unit tests pass:
   - structural test: gate count and per-layer shape match what the
     existing static helpers would produce for `n = 1..8`;
   - round-trip identity test on random basis states for `n = 1..6`
     using the in-tree simulator (`test_qasm_sim_qft.py`-style);
   - non-contiguous qubit-array test (e.g. `qubits = {3, 7, 1, 5}`)
     showing the qmap is honored;
   - edge cases `n = 0` and `n = 1`;
   - error cases (`NULL`, oversized `n`).
3. The full pre-existing test suite (`ctest`, `pytest tests/`) is
   green — in particular `test_qft_arithmetic.c` and
   `test_qasm_sim_qft.py` must be unchanged.
4. `quantum-cython-types` can `cimport` `qc_qft` / `qc_iqft` and back
   `qint.qft()` / `qint.iqft()` with them. (Out of scope for this PRD;
   tracked as a separate issue in that package.)

## 5. Design Decisions

### 5.1 File layout

Create **new files** rather than modifying `qft_addition.c`:

- `src/qft.c` — new public sequence builders + wrappers.
- The public declarations live in the existing
  `include/quantum_circuit.h` (under a new "Bare QFT" section near
  the other `*_seq` declarations around line 1029); no new public
  header file. This matches the convention used for every other
  builder family in the package.

`qft.c` may use a small file-local allocator helper analogous to
`qft_sequence_alloc` in `qft_addition.c`, OR may use the public
`qc_sequence_alloc` followed by manual `gates_per_layer` setup —
whichever is simpler. The implementation step will pick one and
justify it. Either way, `qft.c` must not `#include` or call into
`qft_addition.c`.

### 5.2 Opcode question

**Decision: no new C-side opcode is required.** The C backend does
not have a per-sequence opcode enum. The `qc_arith_mode_t::QC_ARITH_QFT`
constant is a *mode selector* for arithmetic dispatch and is unrelated
to a "bare QFT" instruction. New behavior is exposed as new public
functions, which is consistent with every other builder family
(`qc_cmp_*`, `qc_split_*`, `qc_toffoli_*_seq`, etc.).

The "new IR opcode" the user mentioned belongs to
`quantum-cython-types`' Python-level IR, not to this package. See
section 7.

### 5.3 Signature shape

Pick `(circuit_ctx_t *ctx, const uint32_t *qubits, uint32_t n)` to
match every other public function in this package (`qc_circuit_mcx`,
`qc_arith_qq_add`, etc.) — first arg `ctx`, then qubit array, then
count. The sequence builder uses `int n` for symmetry with the other
`*_seq` builders that already take `int bits`.

### 5.4 Inverse via two builders, not a flag

The package convention is two distinct builder functions for
forward/inverse pairs (e.g. `qc_split_cq_add_seq` /
`qc_split_cq_sub_seq`). We keep the same pattern: `qc_qft_seq` and
`qc_iqft_seq` are two separate functions. (`qc_run_instruction` does
have an `invert` flag, but that is an *application*-time concern, not
a builder concern, and using it would obscure the no-swap convention.)

### 5.5 No-swap convention

Matches the existing static helpers exactly. Documented in the
header docstring of each new function so consumers cannot
accidentally compose it with a swap-terminated QFT.

## 6. Reference rotation pattern

For `qc_qft_seq(n)` (verbatim with what the static helper does, just
re-derived against a 0-based qubit array of size `n`):

```
for j in 0..n-1:
    q = n - 1 - j           # MSB-first
    layer = 2*j
    H(q)                    # at layer
    for i in 0..n-2-j:
        CP(q, q-i-1, pi / 2^(i+1))   # at layer 2*j + i + 1
total layers = 2*n - 1
```

`qc_iqft_seq(n)` mirrors this with negated phases and reversed layer
ordering, exactly as the existing `qc_seq_qft_inv` does. Concretely,
the inverse builder must produce a sequence of length `2*n - 1`
whose layer at index `2*n - 2 - k` corresponds to the forward layer
at index `k` with phase angles negated. The implementation copies
the inverse loop from `qft_addition.c` lines 166–179 verbatim,
preserving the layer index expression `2*n - 2 - (2*j + i + 1)`.

## 7. Out of Scope

- **Modifying `src/qft_addition.c`** in any way. The static
  `qc_seq_qft` / `qc_seq_qft_inv` helpers stay file-private and
  bit-for-bit unchanged. No promotion to public, no refactor, no
  call-through.
- Adding final swaps or a swap-toggle option.
- Approximate / banded QFT (truncated rotation tail).
- Controlled-QFT (`c_qft_seq`). Possible future extension; not in
  this PRD.
- Adding any new C-side opcode enum.
- Changes to `quantum-cython-types`. That package needs a separate
  PRD/PLAN to:
  - add Python-level IR opcodes for QFT/IQFT,
  - bind `qc_qft`/`qc_iqft` via Cython,
  - expose `qint.qft()` / `qint.iqft()`.
  Note this dependency in the orchestrator handoff but do **not**
  plan it here.

## 8. Risks

- **Risk:** subtle off-by-one between the new builder and the static
  helper would silently break the round-trip test or cause Shor's
  algorithm callers to get wrong results.
  **Mitigation:** structural test that compares the new builder's
  per-layer gate list to a literal expected pattern derived from the
  static helper for `n = 1..8`.
- **Risk:** future maintainer "deduplicates" by routing the static
  helper through the new public one.
  **Mitigation:** explicit comment block in both `qft.c` and
  `qft_addition.c` stating that the duplication is intentional and
  load-bearing for build stability.
