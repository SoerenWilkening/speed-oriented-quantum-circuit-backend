# PRD: Fix CQ Comparison Wrapper Qubit Mapping

## Goal

Fix the `qc_cmp_cq_less()`, `qc_cmp_cq_greater()`, and `qc_cmp_cq_equal()`
public API wrappers in `integer_comparison.c` so that CQ ordering comparisons
work correctly for all valid widths without hangs, memory corruption, or
incorrect results.

## Motivation

CQ ordering comparisons (`qc_cmp_cq_less`, `qc_cmp_cq_greater`) hang for
certain width/value combinations. CQ equality (`qc_cmp_cq_equal`) accesses
undefined qubit mappings for width >= 3. These failures block:

- Lifting the `_MAX_CMP_CQ_WIDTH = 2` workaround in quantum-cython-types
- All CQ ordering comparison tests (currently skipped due to hangs)
- QQ equality for width >= 3 (at the Python/Cython layer, QQ equality
  routes through CQ equality for certain widths; `qc_cmp_qq_equal` in C
  has its own direct implementation, but the Python dispatch path uses CQ)

## Root Cause

The issue report (Issue 3) attributed the bug to an off-by-one error in
`hp_iqft()` in `hot_path_add.c`. **This is a misdiagnosis.** Investigation
confirms that `hp_iqft()` produces correct layer indices for all valid widths.
The sequence-level functions (`qc_cmp_cq_less_seq`, `qc_split_cq_sub_seq`,
etc.) build correct gate sequences.

The actual root cause is in the **public API wrapper functions** in
`integer_comparison.c`. These wrappers build a `qubit_array[]` mapping from
abstract qubit indices to physical qubits, then call `qc_run_instruction()`.
However, they fail to allocate and map **ancilla qubits** that the sequences
require:

1. **`qc_cmp_cq_less()`** (line 688): The CQ less-than sequence uses abstract
   qubit layout `[0]=result, [1..width]=A, [width+1]=borrow_ancilla`. The
   wrapper sets `qubit_array[0..width]` but leaves `qubit_array[width+1]`
   **uninitialized** (stack garbage). When `qc_run_instruction` maps
   abstract qubit `width+1` through the garbage value, it accesses an
   out-of-bounds qubit index, causing memory corruption or infinite loops
   in the circuit's layer management.

2. **`qc_cmp_cq_greater()`** (line 718): Delegates to the same less-than
   sequence (with value+1), inheriting the same missing-borrow bug.

3. **`qc_cmp_cq_equal()`** (line 657): For width >= 3, the equality sequence
   uses AND-ancilla qubits at indices `[width+1..width+width-2]`. The wrapper
   does not map these, leaving them as uninitialized stack values.

### Evidence

- `qc_cmp_cq_less_seq(2, 0)` builds a correct 31-gate sequence. Applying it
  via `qc_run_instruction` with a properly populated `qubit_array[0..3]`
  succeeds. The public wrapper `qc_cmp_cq_less(ctx, a, 2, 0, result)` hangs
  because `qubit_array[3]` is uninitialized.
- Width=1 with certain values works only because the garbage stack value
  happens to be a valid qubit index.
- Width=3+ equality works for width <= 2 only because no AND-ancilla are
  needed for MCX decomposition of 1-2 controls.

### How the Monolith Handles This

In the monolith's Python layer (`qint_comparison.pxi`, line 321), the borrow
ancilla is **explicitly allocated** via the qubit allocator:

```cython
_lt_borrow = allocator_alloc(alloc, 1, True)
regs = (result_qubit,) + tuple(a_qubits) + (_lt_borrow,)
run_instruction(_uc_seq, &regs[0], False, _circuit)
allocator_free(alloc, _lt_borrow, 1)
```

The C backend in the monolith does **not** have public wrapper functions like
`qc_cmp_cq_less()`. The qubit mapping is always handled by the Python/Cython
layer. The micro-package added these C wrappers as a convenience API, but
they are incomplete.

## Requirements

### R1: Fix borrow ancilla in `qc_cmp_cq_less` and `qc_cmp_cq_greater`

The wrappers must allocate a temporary qubit for the borrow ancilla, include
it in `qubit_array`, and free it after the sequence is applied.

**Success criteria:**
- `qc_cmp_cq_less(ctx, a, w, v, result)` completes without hanging for all
  widths 1-63 and all representable values
- `qc_cmp_cq_greater(ctx, a, w, v, result)` same
- Gate counts match the sequence-level functions

### R2: Fix AND-ancilla in `qc_cmp_cq_equal`

For width >= 3, the wrapper must allocate temporary qubits for the MCX
decomposition AND-ancilla chain.

**Success criteria:**
- `qc_cmp_cq_equal(ctx, a, w, v, result)` works for all widths 1-64

### R3: Fix ancilla in controlled comparison wrappers

The controlled variants (`qc_cmp_cq_less` called from Python with a control
qubit) may have the same issue. Verify and fix if needed. Currently the
controlled wrappers are not exposed in the public API but the `_seq()` builder
functions exist.

### R4: Regression tests

Add C-level tests that exercise CQ comparisons at widths 1-8 with multiple
values, verifying:
- No crashes or hangs
- Correct gate counts (match sequence-level output)
- Functions return QC_OK

### R5: Simulation correctness tests (sibling package follow-up)

Python-level simulation tests that verify CQ less-than and greater-than
produce correct results when simulated via OpenQASM live in
`integration-tests/`, not in this package. After the C-level fix lands,
a follow-up issue should be filed for integration-tests to add or update
these simulation tests. This requirement is NOT implemented in this plan.

## Out of Scope

- **`hp_iqft()` changes**: Investigation confirms `hp_iqft()` is correct.
  No changes to `hot_path_add.c` are needed.
- **Removing `_MAX_CMP_CQ_WIDTH`**: This is in quantum-cython-types and will
  be addressed as a separate follow-up issue after the C backend is fixed.
- **QQ comparison bugs**: Issue 1 (QQ ordering comparisons never set result
  qubit) is a separate Python-level issue.
- **Public API signature changes**: The fix must not change any function
  signatures in `quantum_circuit.h`.

## Key Design Decisions

### D1: Use `qc_qubit_alloc` / `qc_qubit_free` for ancilla

The wrappers should use the existing qubit allocator (`qc_qubit_alloc`,
`qc_qubit_free`) to obtain temporary ancilla qubits. This ensures the
ancilla don't collide with user-allocated qubits.

### D2: Free ancilla after sequence application

Ancilla qubits must be freed after `qc_run_instruction` returns. The
comparison sequences are designed so ancilla are returned to |0> after
the sequence completes (the add-back step restores the borrow qubit).

### D3: Stack-allocated qubit_array is sufficient

The current `uint32_t qubit_array[128]` is large enough for the maximum
qubit mapping needed (width + ancilla count is always < 128 for width <= 64).

## Risks

- **Qubit pressure**: Allocating ancilla in the C wrapper means the circuit
  context must have available qubits. If the user's circuit is at capacity,
  the allocation will fail. This matches the monolith's behavior (Python
  allocator can also fail).
- **Ancilla not in |0>**: If the sequence has a bug that doesn't properly
  restore the borrow ancilla to |0>, freeing it could cause issues. The
  existing `qc_split_cq_add_seq` / `qc_split_cq_sub_seq` pattern (subtract
  then add back) should restore the ancilla, but this needs test verification.

## Downstream Impact

Once this fix lands:
- quantum-cython-types can remove `_MAX_CMP_CQ_WIDTH = 2` (separate issue)
- Integration tests for CQ comparisons can be un-skipped
- The CQ comparison simulation tests should pass
