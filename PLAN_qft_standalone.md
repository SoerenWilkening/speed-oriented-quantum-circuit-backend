# PLAN: Standalone QFT / Inverse-QFT Sequence Builders

## Status

**COMPLETE** — 2026-04-06. All five steps below have shipped and all
requirements were met. Work was completed by beads issues:
refactor-623, refactor-2ed, refactor-q0f, refactor-6s9, refactor-z1c.

Companion to `PRD_qft_standalone.md`. Five independently testable
steps; each step is small (well under the 500-line module cap).

Total expected new code: ~250–350 lines in `src/qft.c`, ~10 lines
in `include/quantum_circuit.h`, ~250 lines of new tests, no edits
to existing source files except the public header and CMakeLists.

## Step 1 — Public header declarations  [x] Status: DONE (refactor-623)

**Goal:** introduce the four new public symbols so downstream
consumers can `cimport` them, before any implementation lands.
Implementation in step 2 will satisfy the linker.

**Files:**
- modify `include/quantum_circuit.h`

**Changes:**
- Add a new section "Bare QFT sequence builders" in the `*_seq`
  area near line 1029, declaring:
  ```c
  QC_API qc_sequence_t *qc_qft_seq(int n);
  QC_API qc_sequence_t *qc_iqft_seq(int n);
  ```
- Add a new section "Bare QFT" near the other `qc_arith_*`
  wrappers, declaring:
  ```c
  QC_API qc_error_t qc_qft (circuit_ctx_t *ctx, const uint32_t *qubits, uint32_t n);
  QC_API qc_error_t qc_iqft(circuit_ctx_t *ctx, const uint32_t *qubits, uint32_t n);
  ```
- Add doxygen comments documenting the no-final-swaps convention,
  edge cases (`n==0` no-op, `n==1` single H), and ownership
  (caller frees the sequence returned by the builders).

**Test plan:**
- `cmake --build build` must still link the existing test binaries
  even though the new symbols are unimplemented (they are not yet
  referenced anywhere). If the build breaks, header is malformed.
- `grep` confirms the four declarations are present.

**Dependencies:** none.

## Step 2 — Implement `qc_qft_seq` / `qc_iqft_seq`  [x] Status: DONE (refactor-2ed)

**Goal:** produce the two new builder functions in a brand-new
source file, fully independent of `qft_addition.c`.

**Files:**
- create `src/qft.c`
- modify `CMakeLists.txt` to add `src/qft.c` to the library target
  source list

**Changes:**
- File-local helpers `qft_seq_alloc(num_layers, gates_cap)`,
  `seq_h(...)`, `seq_cp(...)` — duplicated from the static
  helpers in `qft_addition.c` (intentional, see PRD §8).
- **Allocator note:** use a LOCAL two-arg sequence-alloc helper
  analogous to the static `qft_sequence_alloc` in
  `qft_addition.c`. Do NOT use the public single-arg
  `qc_sequence_alloc` — it does not allocate per-layer gate
  slots, so the new builder would need to backfill them manually.
  The local helper takes `(num_layers, gates_per_layer_cap)` and
  pre-sizes each layer's gate array, matching the existing
  pattern.
- `qc_qft_seq(int n)`:
  - if `n <= 0`: allocate empty sequence (`num_layers = 0`),
    set `total_qubits = 0`, return.
  - if `n == 1`: 1 layer, 1 H gate on qubit 0.
  - general: `2*n - 1` layers, follow the reference loop in
    PRD §6, set `seq->used_layer = 2*n - 1`.
  - call `qc_sequence_compute_total_gate_count(seq)`.
  - set `seq->total_qubits = (uint32_t)n`.
- `qc_iqft_seq(int n)`: copy the inverse loop from
  `qft_addition.c` lines 166–179 VERBATIM into the new `qft.c`,
  preserving the layer index expression
  `2*n - 2 - (2*j + i + 1)` exactly as written (do not
  re-derive it). The result must be a sequence of length
  `2*n - 1` whose layer at index `2*n - 2 - k` corresponds to the
  forward layer at index `k` with phase angles negated.
- Top-of-file comment explicitly stating: "Duplication of the
  rotation pattern in qft_addition.c is intentional and required
  to keep the QFT-arithmetic builders bit-for-bit stable. Do not
  refactor to share code."

**Test plan:**
- New `tests/test_qft_standalone.c` (skeleton; full content lands
  in step 4) calls `qc_qft_seq(n)` for `n = 0..8`, asserts:
  - `n==0`: returns non-NULL, `used_layer==0`,
    `total_gate_count==0`.
  - `n==1`: `used_layer==1`, total gates == 1.
  - `n==4`: `used_layer==7`, total gates == 1+2+3+4 = 10
    (n Hadamards + n*(n-1)/2 controlled phases).
  - Per-layer gate counts match the closed form derived from
    PRD §6.
- `qc_iqft_seq(n)` produces the same gate count as
  `qc_qft_seq(n)` and the same per-layer multiset (in reversed
  order).
- Free with `qc_sequence_free`, run under valgrind (if available)
  for the small cases.

**Dependencies:** Step 1.

## Step 3 — Implement `qc_qft` / `qc_iqft` wrappers  [x] Status: DONE (refactor-q0f)

**Goal:** thin public wrappers that build, run, free.

**Files:**
- modify `src/qft.c` (append wrappers)

**Changes:**
- `qc_qft(ctx, qubits, n)`:
  - `ctx == NULL`            → `QC_ERR_NULL`
  - `n > 64`                 → `QC_ERR_WIDTH`
  - `n > 0 && qubits == NULL`→ `QC_ERR_NULL`
  - `n == 0`                 → return `QC_OK` immediately
  - else: `seq = qc_qft_seq((int)n)`; if NULL → `QC_ERR_ALLOC`;
    `qc_run_instruction(ctx, seq, qubits, 0)`;
    `qc_sequence_free(seq)`; return `QC_OK`.
- `qc_iqft` analogous, calling `qc_iqft_seq`.

**Test plan:**
- Test that `qc_qft(ctx, NULL, 0)` returns `QC_OK` and emits zero
  gates (`qc_circuit_gate_count(ctx) == 0`).
- Test that `qc_qft(NULL, ...)` returns `QC_ERR_NULL`.
- Test that `qc_qft(ctx, qubits, 65)` returns `QC_ERR_WIDTH`.
- Test that `qc_qft(ctx, NULL, 4)` returns `QC_ERR_NULL`.
- Test that `qc_qft(ctx, qubits, 4)` on freshly-allocated qubits
  emits exactly the expected number of gates (10 for n=4).

**Dependencies:** Step 2.

## Step 4 — Functional / round-trip tests  [x] Status: DONE (refactor-6s9)

**Goal:** verify correctness end-to-end against a simulator.

**Files:**
- create `tests/test_qft_standalone.c` (structural / API tests
  from steps 2 and 3, plus non-contiguous qubit-array test)
- create `tests/test_qasm_sim_qft_standalone.py` (Python
  simulator round-trip test, modeled on the existing
  `tests/test_qasm_sim_qft.py`)
- modify `CMakeLists.txt` / `tests/CMakeLists.txt` to register
  the new C test executable

**Changes:**
- C test cases:
  - structural per-layer comparison for `n ∈ {1, 2, 3, 4, 5, 8}`
  - **literal-tuple structural test**: for at least `n=3` and
    `n=4`, assert the exact list of `(target, control, angle)`
    tuples emitted, layer by layer, against a hand-derived
    expected table. Per-layer counts alone can miss a sign flip
    on a single rotation that another rotation cancels, so the
    literal-tuple check is required, not optional.
  - **explicit `n=1` inverse assertion**: `qc_iqft_seq(1)` must
    return a sequence with exactly one layer containing exactly
    one H gate on qubit 0 and zero CP rotations.
  - non-contiguous qubit array, e.g. `{3, 7, 1, 5}`, asserts the
    emitted gates target exactly those hardware qubits and never
    a qubit outside the array
  - error path coverage for the wrappers
- Python test cases (using whatever simulator the existing
  `test_qasm_sim_qft.py` already imports):
  - for `n ∈ {1, 2, 3, 4, 5}` and each computational basis state
    `|x⟩`: build a circuit that allocates `n` qubits, prepares
    `|x⟩` with `X` gates, applies `qc_qft` then `qc_iqft`,
    exports to QASM, simulates, and asserts the final state is
    still `|x⟩` (within `1e-9`)
  - same test on non-contiguous qubit indices to confirm the
    qmap is honored end-to-end

**Test plan:**
- `cd build && ctest --output-on-failure` — new C test passes.
- `pytest tests/test_qasm_sim_qft_standalone.py -v` — passes.

**Dependencies:** Step 3.

## Step 5 — Regression check on existing QFT-arithmetic path  [x] Status: DONE (refactor-z1c)

**Goal:** prove the existing `qc_arith_*` builders are unaffected.

**Files:** none modified.

**Changes:** none.

**Test plan:**
- Re-run the full pre-existing suite:
  - `ctest --output-on-failure` (must include
    `test_qft_arithmetic`)
  - `pytest tests/test_qasm_sim_qft.py -v`
  - `pytest tests/ -v`
- All previously-passing tests must still pass with zero diff in
  output.
- Spot-check: dump the gate sequence of `qc_arith_qq_add_seq(4)`
  before and after this PR; confirm byte-for-byte identical.

**Dependencies:** Steps 1–4.

## Out-of-package follow-up (NOT in this plan)

`quantum-cython-types` will need its own PRD/PLAN to:
1. add Python-level IR opcodes for QFT / IQFT,
2. add Cython declarations for `qc_qft` / `qc_iqft` in
   `_c_backend.pxd`,
3. expose `qint.qft()` / `qint.iqft()` methods that emit those IR
   nodes and lower them to the new C wrappers.

Mention this in the orchestrator handoff so the sibling package
gets its own planner spawn after this PRD lands.
