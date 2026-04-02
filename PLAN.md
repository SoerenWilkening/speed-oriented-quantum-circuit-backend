# Implementation Plan: Fix CQ Comparison Wrapper Qubit Mapping

## Overview

The public API wrappers `qc_cmp_cq_less()`, `qc_cmp_cq_greater()`, and
`qc_cmp_cq_equal()` in `integer_comparison.c` fail to allocate and map ancilla
qubits that their underlying sequences require. This causes hangs, memory
corruption, or incorrect results for certain width/value combinations.

See `PRD.md` for full root cause analysis and evidence.

## Step 1: Fix `qc_cmp_cq_greater` and `qc_cmp_cq_less` borrow ancilla

**Priority note:** `qc_cmp_cq_greater` is the PRIMARY fix target because it
is the actual downstream call path used by quantum-cython-types
(`_qint_compare.py` calls `cmp_cq_greater`). `qc_cmp_cq_less` is fixed for
API completeness but is not currently called by downstream consumers.

**Files modified:** `circuit-c-backend/src/integer_comparison.c`
**Estimated lines changed:** ~40

### Changes

In `qc_cmp_cq_greater()` (line 718) -- fix FIRST as it is the downstream
call path:

1. After building `qubit_array[0..width]`, allocate a borrow ancilla qubit
   using `qc_qubit_alloc(ctx, &borrow)`.
2. Set `qubit_array[width + 1] = borrow`.
3. **Empty-sequence edge case:** `qc_cmp_cq_greater_seq()` returns an empty
   sequence when `value >= max_val` (i.e., `value >= (1 << width) - 1`),
   because "a > max_val" is trivially false. When the sequence is empty
   (gate count == 0), skip `qc_run_instruction` entirely and do NOT allocate
   the ancilla qubit. The result qubit remains unchanged (stays |0>, meaning
   "not greater"), which is the correct behavior.
4. When the sequence is non-empty, call `qc_run_instruction(ctx, seq,
   qubit_array, 0)` as before, then free the borrow ancilla with
   `qc_qubit_free(ctx, borrow)`.
5. Handle allocation failure by returning `QC_ERR_ALLOC`.

In `qc_cmp_cq_less()` (line 688) -- same pattern:

1. Same ancilla allocation and mapping as above.
2. **Empty-sequence edge case:** `qc_cmp_cq_less_seq()` can return an empty
   sequence for certain inputs (e.g., `value == 0` means "a < 0" is trivially
   false for unsigned). When the sequence is empty, skip `qc_run_instruction`
   and do not allocate the ancilla.
3. Otherwise, run the sequence and free the ancilla.

### Verification

```bash
# Build
cd circuit-c-backend && cmake -B build && cmake --build build

# Quick smoke test: compile and run a C test that calls qc_cmp_cq_less
# at widths 1-4 with values that previously hung (w=1,v=2; w=2,v=0; etc.)
```

### Test criteria

- `qc_cmp_cq_greater(ctx, a, 2, 0, result)` returns QC_OK (primary path)
- `qc_cmp_cq_greater(ctx, a, 2, 3, result)` returns QC_OK with 0 gates
  (trivially-false case: value == max_val)
- `qc_cmp_cq_less(ctx, a, 2, 0, result)` returns QC_OK (previously hung)
- `qc_cmp_cq_less(ctx, a, 1, 2, result)` returns QC_OK (previously hung)
- Gate counts match `qc_sequence_gate_count(qc_cmp_cq_less_seq(w, v))`
  for all tested width/value pairs

---

## Step 2: Fix `qc_cmp_cq_equal` AND-ancilla for width >= 3

**Files modified:** `circuit-c-backend/src/integer_comparison.c`
**Estimated lines changed:** ~25

### Changes

In `qc_cmp_cq_equal()` (line 657):

1. For width >= 3, the equality sequence uses `width - 2` AND-ancilla at
   abstract indices `[width+1 .. width+width-2]`.
2. After building `qubit_array[0..width]`, allocate `width - 2` ancilla
   using `qc_qubit_alloc_n(ctx, width - 2, &anc_start)`.
3. Map: `qubit_array[width + 1 + i] = anc_start + i` for `i` in
   `[0, width - 3]`.
4. After `qc_run_instruction`, free with `qc_qubit_free_n(ctx, anc_start,
   width - 2)`.
5. For width <= 2, no ancilla are needed (MCX with 1-2 controls doesn't
   decompose). No change to this path.

### Verification

```bash
cmake --build build
# Test qc_cmp_cq_equal at width=3,4,5 with various values
```

### Test criteria

- `qc_cmp_cq_equal(ctx, a, 4, 5, result)` returns QC_OK (previously
  accessed uninitialized memory)
- Gate count matches sequence-level output
- Width 1 and 2 continue to work (no regression)

---

## Step 3: Add C-level regression tests

**Files modified:** `circuit-c-backend/tests/test_integration.c` (or new file
`circuit-c-backend/tests/test_comparison_fix.c`)
**Estimated lines changed:** ~120

### Test cases

1. **`test_cq_greater_all_widths`** (primary -- downstream call path):
   For widths 1-8, test `qc_cmp_cq_greater` with explicit boundary values:
   - `value = 0` (typical case)
   - `value = 1` (near-minimum)
   - `value = max_val - 1` (where `max_val = (1 << width) - 1`)
   - `value = max_val` (trivially-false case: empty sequence, 0 gates)
   Verify for each:
   - Returns QC_OK
   - Gate count matches `qc_sequence_gate_count(qc_cmp_cq_greater_seq(w, v))`
   - For `value = max_val`, gate count is 0

2. **`test_cq_less_all_widths`**: For widths 1-8, test `qc_cmp_cq_less` with
   explicit boundary values:
   - `value = 0` (trivially-false case: empty sequence, 0 gates)
   - `value = 1` (near-minimum)
   - `value = max_val / 2` (mid-range)
   - `value = max_val` (maximum)
   Verify:
   - Returns QC_OK
   - Gate count matches `qc_sequence_gate_count(qc_cmp_cq_less_seq(w, v))`
   - For `value = 0`, gate count is 0

3. **`test_cq_equal_all_widths`**: For widths 1-8, test `qc_cmp_cq_equal`
   with values 0, 1, and max_unsigned. Verify same criteria.

4. **`test_cq_less_previously_hanging`**: Specifically test the known-bad
   combinations: (w=1, v=2), (w=2, v=0), (w=2, v=1), (w=2, v=3). All must
   return QC_OK within bounded time.

5. **`test_cq_greater_simulate_mode`**: For width=2, set simulate=true, run
   `qc_cmp_cq_greater`, extract gates, verify gate count matches.

### Build integration

If creating a new test file, add it to `CMakeLists.txt` test targets.

### Test criteria

- All test cases pass
- No memory leaks (valgrind clean if available)
- Tests complete in < 5 seconds total

---

## Step 4: Verify and fix controlled CQ wrappers and `qc_cmp_qq_less`

**Files modified:** `circuit-c-backend/src/integer_comparison.c` (if needed)
**Estimated lines changed:** ~40 (if changes needed), 0 (if not exposed)

### Analysis

1. **Controlled CQ variants:** The controlled variants `qc_c_cmp_cq_less_seq`
   and `qc_c_cmp_cq_equal_seq` exist as sequence builders. Check whether
   there are public API wrappers for them (like `qc_c_cmp_cq_less(ctx, ...)`)
   that have the same missing-ancilla pattern. If so, apply the same fix.
   Currently, the controlled variants are used by quantum-cython-types through
   the sequence API (the Python layer handles qubit mapping). If there are no
   C-level wrappers, this step is a no-op -- just document that the controlled
   `_seq()` functions are correct and the Python layer must handle ancilla
   mapping.

2. **`qc_cmp_qq_less` wrapper:** Also verify whether the QQ less-than wrapper
   `qc_cmp_qq_less()` has similar ancilla allocation issues. The QQ less-than
   sequence may require a borrow ancilla similar to the CQ variants. If the
   wrapper does not map the ancilla, apply the same fix pattern. If it already
   handles ancilla correctly, document why.

### Test criteria

- If wrappers exist with bugs: same test pattern as Steps 1-3
- If no wrappers or wrappers are correct: document in code comments that
  callers are responsible for ancilla mapping when using `_seq()` functions
  directly
- `qc_cmp_qq_less` at widths 1-4 returns QC_OK and produces correct gate
  counts

---

## Summary

| Step | Description                                 | Files | Est. lines |
|------|---------------------------------------------|-------|------------|
| 1    | Fix borrow ancilla in greater/less (+ edge) | 1     | ~40        |
| 2    | Fix AND-ancilla in equal (width>=3)         | 1     | ~25        |
| 3    | C-level regression tests                    | 1-2   | ~150       |
| 4    | Verify controlled wrappers + qq_less        | 0-1   | ~0-40      |

**Total estimated:** ~215-255 lines changed across 3-4 steps.

## Dependencies

- Steps 1 and 2 are independent and can be done in parallel.
- Step 3 depends on Steps 1 and 2.
- Step 4 is independent of all other steps.

## Sibling Package Follow-Up (not implemented here)

After this fix lands:

1. **quantum-cython-types** should:
   - Remove `_MAX_CMP_CQ_WIDTH = 2` from `_qint_compare.py`
   - Un-skip CQ ordering comparison tests
   - This is a separate issue to be filed against quantum-cython-types

2. **integration-tests** should:
   - Add or update Python-level simulation tests that verify CQ less-than
     and greater-than produce correct results when simulated via OpenQASM
   - This corresponds to PRD R5, which lives in integration-tests/, not in
     circuit-c-backend
