# Implementation Plan: QQ Less-Than Comparison Sequence Builders

**Issue:** refactor-eap
**Package:** circuit-c-backend
**PRD:** `PRD_qq_comparison.md`

## Overview

Replace the NULL-returning stubs for `qc_cmp_qq_less_seq` and `qc_c_cmp_qq_less_seq` with capture-based implementations that use the existing QFT arithmetic infrastructure.

## Architecture Decision: Capture-Based Construction

**Chosen approach:** Build sequences by running operations on a temporary circuit and capturing the emitted gates (same pattern as `divmod_sequences.c` and `cmul_sequences.c`).

**Why not direct sequence construction (copy_remap_layers)?** The monolith uses `copy_remap_layers` / `copy_remap_layers_inverse` static helpers to remap and embed sub-sequences. The micro-package does not have these helpers, and reimplementing them adds complexity and risk. The capture-based approach:
- Reuses tested `qc_arith_qq_add` / `qc_arith_cqq_add` functions directly
- Uses `qc_circuit_reverse_range` for inversion (already correct for QFT rotation gates)
- Follows established codebase patterns
- Trades ~10% more ancilla overhead for much simpler, less error-prone code

## Steps

### Step 1: Add capture helpers to `comparison_internal.h`

**File:** `src/comparison_internal.h`
**Lines added:** ~8

Add forward declarations for two static-inline or extern capture helpers:
```c
extern qc_sequence_t *qc_cmp_capture_to_sequence(circuit_ctx_t *ctx);
extern circuit_ctx_t *qc_cmp_create_capture_ctx(uint32_t initial_qubits);
```

Alternatively, since both divmod and cmul duplicate these helpers as static functions, the comparison files can do the same. The choice depends on whether we want to reduce duplication (shared) or keep files self-contained (static copies).

**Decision:** Use static copies within `integer_comparison.c`, matching the established pattern. This avoids cross-file linkage changes and keeps each file self-contained. The helpers are ~100 lines total but identical to the divmod/cmul versions.

**Testability:** These are internal helpers; tested indirectly via steps 2-3.

---

### Step 2: Implement `qc_cmp_qq_less_seq` (uncontrolled)

**File:** `src/integer_comparison.c`
**Lines added:** ~70 (replacing ~5-line stub)
**Net change:** ~+65 lines (424 -> ~489 lines, under 500)

**Algorithm (capture-based):**

```
function qc_cmp_qq_less_seq(bits):
    validate: bits in [1, 63]

    # Qubit layout for capture circuit:
    #   [0]=result, [1..bits]=A, [bits+1..2*bits]=B,
    #   [2*bits+1]=borrow, [2*bits+2]=zero_ext
    n = bits
    total_reg = 2*n + 3
    ctx = create_capture_ctx(total_reg + headroom)
    alloc total_reg qubits

    # Define register arrays
    A[i] = i+1         for i in [0, bits)
    B[i] = bits+1+i    for i in [0, bits)
    A_ext[i] = A[i]    for i in [0, bits), A_ext[bits] = 2*bits+1 (borrow)
    B_ext[i] = B[i]    for i in [0, bits), B_ext[bits] = 2*bits+2 (zero_ext)

    # Step 1: A -= B (inverse QQ_add on n bits)
    qc_arith_qq_add(ctx, A, B, bits)   # forward add
    layer_after_step1 = ctx->used_layer
    # Now reverse step1 to get subtraction:
    # Actually, we need to use qc_run_instruction with invert=1
    # OR: emit forward add, record layers, then reverse that range

    # Better approach using run_instruction(invert=1):
    seq_add_n = qc_arith_qq_add_seq(bits)
    qc_run_instruction(ctx, seq_add_n, qubit_map_n, 1)  # invert=1 => A -= B

    # Step 2: [A,borrow] += [B,zero_ext] (forward QQ_add on n+1 bits)
    seq_add_ext = qc_arith_qq_add_seq(bits+1)
    qc_run_instruction(ctx, seq_add_ext, qubit_map_ext, 0)  # forward

    # Step 3: CX(borrow -> result)
    qc_circuit_cx(ctx, 2*bits+1, 0)  # control=borrow, target=result

    # Step 4: Undo step 2 (inverse extended add)
    qc_run_instruction(ctx, seq_add_ext, qubit_map_ext, 1)  # invert=1

    # Step 5: A += B (forward QQ_add on n bits, restore)
    qc_run_instruction(ctx, seq_add_n, qubit_map_n, 0)  # forward

    # Capture and cleanup
    seq = capture_circuit_to_sequence(ctx)
    seq->total_qubits = 2*bits + 3
    free sequences, destroy ctx
    return seq
```

**Key insight:** Using `qc_run_instruction(ctx, seq, map, invert=1)` lets us apply a sequence in reverse with inverted rotations, without needing `copy_remap_layers_inverse`. The qubit mapping arrays handle the register layout remapping.

**Qubit mapping arrays:**

`qubit_map_n` (for QQ_add on n bits, layout [0..n-1]=target, [n..2n-1]=source):
- map[i] = A[i] = i+1 for i in [0, bits)  (target -> A)
- map[bits+i] = B[i] = bits+1+i for i in [0, bits)  (source -> B)

`qubit_map_ext` (for QQ_add on n+1 bits, layout [0..n]=target, [n+1..2n+1]=source):
- map[i] = A[i] = i+1 for i in [0, bits)  (target LSBs -> A)
- map[bits] = 2*bits+1  (target MSB -> borrow)
- map[bits+1+i] = B[i] = bits+1+i for i in [0, bits)  (source LSBs -> B)
- map[2*bits+1] = 2*bits+2  (source MSB -> zero_ext)

**Testability:** Unit test with small bit widths (1-4 bits). Verify result qubit is set correctly for known A < B cases and unset for A >= B cases.

---

### Step 3: Implement `qc_c_cmp_qq_less_seq` (controlled)

**File:** `src/integer_comparison_ctrl.c`
**Lines added:** ~75
**Net change:** 402 -> ~477 lines, under 500

Move the stub from `integer_comparison.c` to `integer_comparison_ctrl.c` (where all controlled CQ variants already live). Replace with capture-based implementation.

**Algorithm (same as uncontrolled but with controlled addition):**

```
function qc_c_cmp_qq_less_seq(bits):
    validate: bits in [1, 63]

    # Qubit layout:
    #   [0]=result, [1..bits]=A, [bits+1..2*bits]=B,
    #   [2*bits+1]=borrow, [2*bits+2]=zero_ext, [2*bits+3]=control
    total_reg = 2*bits + 4
    ctx = create_capture_ctx(total_reg + headroom)
    alloc total_reg qubits

    ctrl = 2*bits + 3

    # Use controlled QQ_add sequences (cQQ_add)
    seq_cadd_n = qc_arith_cqq_add_seq(bits)
    seq_cadd_ext = qc_arith_cqq_add_seq(bits+1)

    # qubit_map_n: cQQ_add layout [0..n-1]=target, [n..2n-1]=source, [2n]=control
    # qubit_map_ext: cQQ_add layout [0..n]=target, [n+1..2n+1]=source, [2n+2]=control

    # Step 1: Controlled A -= B
    qc_run_instruction(ctx, seq_cadd_n, qubit_map_n, 1)  # invert

    # Step 2: Controlled [A,borrow] += [B,zero_ext]
    qc_run_instruction(ctx, seq_cadd_ext, qubit_map_ext, 0)  # forward

    # Step 3: CCX(target=result, ctrl1=borrow, ctrl2=control)
    qc_circuit_ccx(ctx, 2*bits+1, ctrl, 0)

    # Step 4: Undo step 2
    qc_run_instruction(ctx, seq_cadd_ext, qubit_map_ext, 1)  # invert

    # Step 5: Controlled A += B (restore)
    qc_run_instruction(ctx, seq_cadd_n, qubit_map_n, 0)  # forward

    seq = capture_circuit_to_sequence(ctx)
    seq->total_qubits = 2*bits + 4
    free sequences, destroy ctx
    return seq
```

**Qubit mapping arrays for controlled variant:**

`qubit_map_n` (for cQQ_add on n bits, layout [0..n-1]=target, [n..2n-1]=source, [2n]=ctrl):
- map[i] = i+1 for i in [0, bits)  (target -> A)
- map[bits+i] = bits+1+i for i in [0, bits)  (source -> B)
- map[2*bits] = 2*bits+3  (control)

`qubit_map_ext` (for cQQ_add on n+1 bits, layout [0..n]=target, [n+1..2n+1]=source, [2n+2]=ctrl):
- map[i] = i+1 for i in [0, bits)  (target LSBs -> A)
- map[bits] = 2*bits+1  (target MSB -> borrow)
- map[bits+1+i] = bits+1+i for i in [0, bits)  (source LSBs -> B)
- map[2*bits+1] = 2*bits+2  (source MSB -> zero_ext)
- map[2*bits+2] = 2*bits+3  (control)

**Testability:** Unit test with small bit widths. Verify controlled behavior (result set only when control=1).

---

### Step 4: Move controlled stub and add externs

**File:** `src/integer_comparison.c`
**Lines removed:** ~15 (the `qc_c_cmp_qq_less_seq` stub and its doc comment)

The controlled variant stub currently lives in `integer_comparison.c` (lines 410-424). Move it to `integer_comparison_ctrl.c` where it belongs with the other controlled variants. This also frees line budget in `integer_comparison.c` for the capture helpers.

Add an `extern` declaration in `integer_comparison_ctrl.c` for `qc_cmp_qq_less_seq` if needed (unlikely since the controlled variant is independent).

**Testability:** Build succeeds; existing tests still pass.

---

### Step 5: Add capture helpers to `integer_comparison.c`

**File:** `src/integer_comparison.c`
**Lines added:** ~95

Add static `cmp_capture_circuit_to_sequence` and `cmp_create_capture_ctx` functions, copied from the divmod pattern. These are used by step 2's implementation.

Also add the same pair as static functions in `integer_comparison_ctrl.c` for step 3's implementation.

**Testability:** Tested indirectly via steps 2-3.

---

### Step 6: Write unit tests

**File:** `tests/test_qq_comparison.c` (new, ~200 lines)

Tests:

1. **`test_qq_less_seq_returns_non_null`** -- Verify `qc_cmp_qq_less_seq(bits)` returns non-NULL for bits = 1, 2, 4, 8.

2. **`test_qq_less_seq_total_qubits`** -- Verify `total_qubits == 2*bits + 3` for several widths.

3. **`test_qq_less_seq_correctness_2bit`** -- For bits=2, exhaustively test all 16 (A,B) combinations. Initialize A and B via X gates, run the comparison sequence, verify the result qubit matches (A < B).

4. **`test_c_qq_less_seq_returns_non_null`** -- Verify `qc_c_cmp_qq_less_seq(bits)` returns non-NULL for bits = 1, 2, 4, 8.

5. **`test_c_qq_less_seq_total_qubits`** -- Verify `total_qubits == 2*bits + 4`.

6. **`test_c_qq_less_seq_controlled`** -- For bits=2, verify that with control=0 the result is always 0, and with control=1 the result matches uncontrolled.

7. **`test_qq_less_seq_register_preservation`** -- Verify that A and B registers are unchanged after the comparison (uncomputation is correct).

8. **`test_qq_less_edge_cases`** -- bits=1 (smallest), bits=0 (should return NULL), bits=64 (should return NULL).

**Note:** Correctness tests require simulation. If the test harness supports it (via `qc_circuit_set_simulate` + gate extraction + classical evaluation), use that. Otherwise, verify structural properties (non-NULL, total_qubits, gate count > 0) and defer simulation tests to the integration-tests package.

---

## Execution Order

| Order | Step | File(s) | Depends On | Est. Lines |
|-------|------|---------|-----------|-----------|
| 1 | Step 4: Move controlled stub | integer_comparison.c, integer_comparison_ctrl.c | -- | -15 / +15 |
| 2 | Step 5: Add capture helpers | integer_comparison.c, integer_comparison_ctrl.c | Step 4 | +95 each |
| 3 | Step 2: Implement uncontrolled | integer_comparison.c | Step 5 | +70 |
| 4 | Step 3: Implement controlled | integer_comparison_ctrl.c | Step 5 | +75 |
| 5 | Step 6: Write tests | tests/test_qq_comparison.c | Steps 2-3 | +200 |
| 6 | Build and verify | -- | Step 5 | -- |

## Line Budget

| File | Before | After | Limit |
|------|--------|-------|-------|
| `integer_comparison.c` | 424 | ~480 | 500 |
| `integer_comparison_ctrl.c` | 402 | ~490 | 500 |
| `tests/test_qq_comparison.c` | 0 (new) | ~200 | 500 |

## Dependencies

- `qc_arith_qq_add_seq(bits)` -- must work for bits in [1, 64] (existing, tested)
- `qc_arith_cqq_add_seq(bits)` -- must work for bits in [1, 64] (existing, tested)
- `qc_run_instruction(ctx, seq, map, invert)` -- invert=1 must correctly reverse QFT sequences (existing, tested)
- `qc_circuit_cx`, `qc_circuit_ccx` -- basic gate emission (existing, tested)
- `capture_circuit_to_sequence` pattern -- established in divmod/cmul (existing, tested)

## Rollback

If the capture-based approach produces incorrect results (e.g., ancilla interference), fall back to direct sequence construction with `copy_remap_layers` ported from the monolith. This is more work but mechanically equivalent to the monolith's proven code.
