# PLAN: Modular Arithmetic C Primitives for `qint_mod`

Beads tag: `refactor-qint-mod-primitives` (placeholder)

Companion to `PRD_qint_mod_primitives.md`. Each step is independently
buildable, testable, and ≤ 500 lines of net change.

The plan is split into **Phase A** (the Shor-critical path — Steps 0–2,
ready to execute on approval) and **Phase B** (everything else, with
explicit dependencies). At the end of Phase A the Layer 2 (Cython) team
can begin binding `cmod_add_cq` and `cmod_mul_cq` in parallel with the
remainder of Phase B.

All new code goes in either:
- `src/toffoli_mod_reduce.c` (existing — keeps modular family together,
  but it is already close to its 500-line limit), or
- `src/toffoli_mod_extras.c` (new sibling file, created in Step 0).

Public declarations all land in `include/quantum_circuit.h` immediately
after line 695 (the existing `qc_toffoli_mod_mul_cq` declaration).

---

## Phase A — Shor unblock (ready to execute immediately on approval)

### Step 0 — File-split prep, header reservations, CMake wiring  (~50 LOC)
**Unblocks:** every later step in both phases.

**Why the file split now (not in Step 5):** Step 5 (`cmod_add_qq`) is
expected to add ~250 LOC to the modular family. `toffoli_mod_reduce.c`
is already ~310 LOC, and Steps 1, 2, 3a alone push it past the 500-line
cap. Creating `toffoli_mod_extras.c` in Step 0 is therefore **motivated
preparation**, not premature scaffolding: it lets every later step pick
the correct file without having to do a mid-stream relocation. The
rationale must be referenced in the Step 0 commit message so reviewers
do not perceive the split as unmotivated.

**Files:**
- `include/quantum_circuit.h` — add doxygen-stubbed declarations for all
  nine new functions (forward declarations only, no bodies), inserted
  after line 695.
- `src/toffoli_mod_extras.c` — **new file**, **must `#include
  "internal.h"`** (required for access to `qc_dynamic_qq_add`,
  `qc_dynamic_qq_sub`, `qc_dynamic_cqq_add`, `qc_dynamic_cqq_sub`,
  `qc_dynamic_cq_add`, `qc_dynamic_ccq_add` — see PRD §4.4 and §8) in
  addition to `quantum_circuit.h`. Contains a file-banner comment plus
  an empty translation unit (no symbols yet).
- `CMakeLists.txt` — **add `src/toffoli_mod_extras.c` to the `quantum`
  library `add_library(...)` source list.** (Not just create the file —
  the file must be wired into the build.)

**Test:** `cmake --build build` succeeds; `nm libquantum.so` does **not**
yet show the new symbols (linker happy because there are no callers).

---

### Step 1 — `qc_toffoli_cmod_add_cq` public wrapper  (~25 LOC)  [SHOR BLOCKER]
**Unblocks:** Step 2 (`cmod_mul_cq`) and Layer 2 binding for
`qint_mod.__add__` controlled forms.
**Depends on:** Step 0.

**Files:**
- `src/toffoli_mod_reduce.c` — add public wrapper that forwards to the
  existing static `toffoli_cmod_add_cq_internal`. (The internal helper
  stays static; the wrapper is the cleaner choice and preserves the
  existing call site in `mod_mul_cq`.)

**Test:** `tests/test_toffoli_cmod_add_cq.c` — n=4, N=7, addend=3,
controls in {0, 1}, three input values; assert gate count > 0, ancilla
balance preserved (`current_in_use` strict `==`), QASM simulation matches
`(x + 3) % 7` when `ctrl=1` and `x` unchanged when `ctrl=0`.

---

### Step 2 — `qc_toffoli_cmod_mul_cq` Beauregard ladder  (~80 LOC)  [SHOR CRITICAL]
**Unblocks:** entire Shor `__rpow__` path. **At the end of this step the
Layer 2 team can begin binding.**
**Depends on:** Step 1.

**Files:**
- `src/toffoli_mod_reduce.c` (or `toffoli_mod_extras.c` if length budget
  blown) — implement per PRD §4.6 using per-iteration AND-ancilla.

**Critical correctness requirement:** the `c == 1` special case **must**
emit `ccx(ext_ctrl, value[i], result[i])` for `i in 0..min(n,
result_bits) - 1`. **Not** `cx(value[i], result[i])`. The bare `cx` is
correct in the uncontrolled `mod_mul_cq` but would silently break the
controlled contract here. The `c == 0` case is a no-op independent of
`ext_ctrl`.

**Test:** `tests/test_toffoli_cmod_mul_cq.c`
- n=3, N=5, multiplier=3; iterate over input value ∈ {0,1,2,3,4} ×
  ctrl ∈ {0,1}; assert result encodes `(value * 3) mod 5` when ctrl=1
  and `0` when ctrl=0; ancilla balance strict-`==`.
- **Dedicated `c == 1` test (ctrl=0):** `multiplier = 1` and a second
  case with `multiplier = N + 1` so `multiplier mod N == 1`
  non-trivially; sweep input values; assert `result` is **unchanged**
  in every case. Fails loudly if the implementer regresses to bare
  `cx`.
- Dedicated `c == 1` test with ctrl=1: assert `result := value`.
- **`multiplier > N` test:** e.g., `N = 5`, `multiplier = 8`; both
  ctrl polarities; assert ctrl=1 yields `(value * (8 mod 5)) mod 5 =
  (value * 3) mod 5` and ctrl=0 is a no-op. Exercises the internal
  classical reduction `multiplier mod N`.
- **`value = 0` test:** prepare the input register in `|0>`; with
  ctrl=1 assert `result == 0` (multiplicative identity from the value
  side); with ctrl=0 assert `result` unchanged from its initial
  contents. Both polarities asserted.

---

### Phase A exit criteria
At the close of Step 2: Layer 2 (`quantum-cython-types`) is unblocked
and can begin binding `qc_toffoli_cmod_add_cq` and
`qc_toffoli_cmod_mul_cq` in parallel with Phase B below.

---

## Phase B — Remaining primitives (sequenced by dependency)

### Step 3a — `qc_toffoli_mod_sub_cq`  (~40 LOC)
**Depends on:** Step 0.

This is the **only** sub variant that is genuinely a thin classical
adapter. Because the subtrahend is classical, the substitution
`s' = (N - (s mod N)) mod N` is computed at compile time and is exact
for every modulus, not just powers of two.

**Files:**
- `src/toffoli_mod_reduce.c` — one-line adapter: reduce `s` mod N,
  compute `s' = (N - s) mod N`, call `qc_toffoli_mod_add_cq` with `s'`.

**Test:** `tests/test_toffoli_mod_sub_cq.c` — n=4, N=7 and N=15, several
input values, ancilla balance strict-`==`.

---

### Step 3b — `qc_toffoli_mod_sub_qq`  (~110 LOC)
**Depends on:** Step 0. (Independent of Step 3a in code, but ordered
after it because the QQ construction requires the dagger discussion in
PRD §4.2 which the planner / executor must internalise.)

**Critical correctness requirement (see PRD §4.2):** the implementation
**must** be the unitary inverse (dagger) of `qc_toffoli_mod_add_qq`.
**Forbidden:** any construction that two's-complements `other` into an
ancilla and then calls `mod_add_qq`. That strategy computes
`(value + 2^n - other) mod N`, which equals `(value - other) mod N`
**only when N is a power of two** — it is silently wrong for every
prime modulus.

The implementer may either (a) hand-write the daggered 8-step block, or
(b) refactor `qc_toffoli_mod_add_qq` to take a private `bool inverse`
flag and dispatch each step to its `qq_add` / `qq_sub` form.

**Files:**
- `src/toffoli_mod_reduce.c` — implement the inverse 8-step block, or
  the `inverse`-flagged shared helper (implementer's choice).

**Test:** `tests/test_toffoli_mod_sub_qq.c`
- **Non-power-of-two regression test:** `N = 15`, n = 4, with at least
  one input pair where `value < other` (e.g., `value = 3, other = 10`),
  asserting the result is `(3 - 10) mod 15 = 8`. This is the
  load-bearing test that catches the two's-complement fallback.
- A second non-power-of-two case (`N = 7`).
- Ancilla balance strict-`==`.

---

### Step 4 — `qc_toffoli_cmod_add_qq`  (~250 LOC)
**Depends on:** Step 0. (Logically independent of Phase A and Steps 3a,
3b, but placed here because it is the dependency for Steps 5 and 6.)

**Files:**
- `src/toffoli_mod_extras.c` (which already `#include`s `internal.h`
  per Step 0) — port `qc_toffoli_mod_add_qq` to a controlled
  Beauregard 8-step block per PRD §4.4. All add/sub primitives are
  the internal `qc_dynamic_*` helpers (`qc_dynamic_cqq_add`,
  `qc_dynamic_cqq_sub`, `qc_dynamic_ccq_add` with negated constants
  for subtract). **Do not introduce public `qc_toffoli_*sub`
  symbols** — they do not exist and the existing `qc_toffoli_mod_add_qq`
  uses the same `qc_dynamic_*` convention.

**Tightened ancilla budget (see PRD §4.4):** allocate **exactly one**
AND-ancilla, used only in Step 5 of the inner Beauregard sequence (the
conditional `N`-add controlled on `cmp_anc`). All other steps are
**singly controlled** on `ext_ctrl` directly via `cqq_add` / `cqq_sub`
/ `ccq_add` / `ccq_sub` / `ccx`. Implementer **must not** allocate
spurious AND-ancillae for Steps 1, 2, 3, 7, 8.

**Test:** `tests/test_toffoli_cmod_add_qq.c` — n=4, N=7 and N=15, both
ctrl polarities, ancilla balance strict-`==`.

---

### Step 5 — `qc_toffoli_cmod_sub_cq` and `qc_toffoli_cmod_sub_qq`  (~70 LOC)
**Depends on:** Steps 1, 3b, 4 (all three). The `cq` variant only needs
Step 1; the `qq` variant needs both Step 4 (`cmod_add_qq`) and Step 3b
(`mod_sub_qq`) because it is the dagger of `cmod_add_qq` and shares the
construction strategy. The dependency is **unconditional**: this step
cannot land until 1, 3b, **and** 4 have all merged.

**Files:**
- `src/toffoli_mod_extras.c` —
  - `cmod_sub_cq`: adapter — call `cmod_add_cq` with
    `(N - (s mod N)) mod N`.
  - `cmod_sub_qq`: dagger of `cmod_add_qq`, mirroring the strategy used
    in Step 3b for `mod_sub_qq` (either hand-daggered or via an
    `inverse` flag in a shared helper).

**Test:** `tests/test_toffoli_cmod_sub.c` — both variants, both ctrl
polarities, non-power-of-two `N`, ancilla balance strict-`==`.

---

### Step 6 — `qc_toffoli_mod_mul_qq`  (~420 LOC)
**Depends on:** Step 4 (`cmod_add_qq`).

**Construction:** per PRD §4.7. Allocate an `(n+1)`-bit `b_shift`
register, CNOT-copy `b` into it, then for each bit `j` of `a` perform a
`cmod_add_qq(result, b_shift, modulus)` controlled on `a[j]`, followed
by a **modular doubling** of `b_shift` in place. Uncompute `b_shift` at
the end of the ladder.

**Modular doubling helper:** factor out a **file-local `static`**
helper with the exact signature

```c
static qc_error_t toffoli_mod_double_inplace(circuit_ctx_t *ctx,
                                              const uint32_t *reg,
                                              uint32_t n,
                                              int64_t modulus);
```

declared and defined in `src/toffoli_mod_extras.c` (no entry in
`include/quantum_circuit.h`, no entry in `src/internal.h`). The
`static` keyword is mandatory: this symbol must NOT be `nm`-visible.
The helper allocates an `(n+1)`-bit `tmp`, CNOT-copies `reg` into
`tmp`, calls `qc_toffoli_mod_add_qq(reg, tmp, modulus)`, then
uncomputes `tmp`. It is shared with Step 7 (which lives in the same
translation unit so the `static` is visible) and is the natural unit
to test in isolation via a dedicated test driver that pokes at it
through a sibling test-only `extern` declaration in
`tests/test_toffoli_mod_double.c`.

**Forbidden:** any "shift the index list" / qubit-relabelling strategy.
Modular reduction is not a permutation of basis states; relabelling is
silently wrong as soon as `2^j ≥ N`.

**Size budget:** the corrected construction is materially larger than
the previous (incorrect) "index shift" estimate of ~260 LOC. Estimated
~380–450 LOC including the doubling helper, the ladder, the uncompute,
and the test. **If the step threatens to exceed 500 LOC, split into:**
- **Step 6a** — modular doubling helper + dedicated unit test
  (~120 LOC).
- **Step 6b** — `mod_mul_qq` ladder using the helper (~280 LOC).

The split is recommended; it makes the doubling helper independently
reviewable and keeps both halves comfortably under the cap.

**Files:**
- `src/toffoli_mod_extras.c` — doubling helper + ladder.

**Tests:**
- `tests/test_toffoli_mod_double.c` (if split): exercise modular
  doubling on N=5, N=7, N=15 over all input values; ancilla balance
  strict-`==`.
- `tests/test_toffoli_mod_mul_qq.c` — n=3, N=5; exhaustive over small
  `a, b`; n=4, N=15 (non-power-of-two) for at least three input pairs;
  ancilla balance strict-`==`.

---

### Step 7 — `qc_toffoli_cmod_mul_qq`  (~120 LOC)
**Depends on:** Step 6 (or 6a + 6b).

**Files:**
- `src/toffoli_mod_extras.c` — wrap the Step 6 ladder, replacing each
  inner `cmod_add_qq(... ctrl=a[j])` with the doubly-controlled form
  via per-iteration AND-ancilla, mirroring Step 2.

**Test:** `tests/test_toffoli_cmod_mul_qq.c` — both ctrl polarities,
non-power-of-two `N`, ancilla balance strict-`==`.

---

### Step 8 — Integration & cross-check  (~30 LOC, no new src)
**Depends on:** all prior steps.

**Files:**
- `tests/test_toffoli_mod_primitives_integration.c` — sanity sweep:
  for each primitive, run a 2- or 3-input case under
  `qc_circuit_optimize` and confirm gate count is non-zero post-opt
  and simulation result still matches.

---

## Ordered Step Summary (size in net LOC of change including tests)

| Phase | # | Step                              | LOC | Depends on | Notes |
|-------|---|-----------------------------------|-----|-----------|-------|
| A | 0  | File split + header + CMake       |  50 | —         | wires `toffoli_mod_extras.c` into build |
| A | 1  | `cmod_add_cq` wrapper             |  90 | 0         | ★ Shor blocker |
| A | 2  | `cmod_mul_cq` ladder              | 220 | 1         | ★ Shor critical; `c==1` uses `ccx` |
| B | 3a | `mod_sub_cq` (CQ adapter)         |  90 | 0         | safe for any N (classical) |
| B | 3b | `mod_sub_qq` (dagger of add)      | 220 | 0         | non-power-of-two test required |
| B | 4  | `cmod_add_qq`                     | 350 | 0         | exactly one AND-ancilla |
| B | 5  | `cmod_sub_cq` + `cmod_sub_qq`     | 130 | 1, 3b, 4  | unconditional triple dep |
| B | 6  | `mod_mul_qq` (split-eligible 6a/6b) | 450 | 4   | modular doubling helper, no index relabel |
| B | 7  | `cmod_mul_qq`                     | 150 | 6         |       |
| B | 8  | Integration sweep                 |  60 | all       |       |

Total ≈ 1810 LOC across ≤ 10 steps; no single step > 450 LOC, with
Step 6 split-eligible to keep both halves well under the 500 cap.
**At the end of Phase A (Step 2), the Cython team can begin Layer 2
binding work in parallel with Phase B.**

---

## Deferred Questions for Orchestrator

1. Phase A ships in two C steps plus Step 0; should the Layer 2 binding
   work for `cmod_add_cq` and `cmod_mul_cq` be triaged to
   `quantum-cython-types` immediately on Phase A close, or batched with
   the rest of the sub/mul/qq family at Phase B close?

## Resolved Questions

- **Modular doubling helper visibility (Step 6 / 6a):** Resolved by
  orchestrator — KEEP PRIVATE. The helper is `static` in
  `src/toffoli_mod_extras.c` under the name
  `toffoli_mod_double_inplace` and is not exported via either
  `include/quantum_circuit.h` or `src/internal.h`.
- **Persistent `cmp_anc` leak in `qc_toffoli_mod_reduce`:** OUT OF
  SCOPE for this beads issue. Will be filed as a separate issue
  against this package.
