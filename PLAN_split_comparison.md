# Implementation Plan: Split integer_comparison.c (refactor-4xn)

## Status: DONE (2026-04-02)

## Overview

Split `src/integer_comparison.c` (823 lines) into two source files and one internal header, each under 500 lines. The controlled sequence builders and public API wrappers move to the new file; helpers and uncontrolled sequence builders stay. Three implementation steps, each independently testable via the existing build and test suite.

---

## Step 1: Create `src/comparison_internal.h` (~60 lines)

**What**: Extract the shared static helpers into a new internal header with renamed, non-static declarations.

**Details**:
- Create `src/comparison_internal.h` with include guard `QC_COMPARISON_INTERNAL_H`
- Include `"internal.h"` and `<stdint.h>`
- Declare (not define) the following functions with `qc_cmp_` prefix:
  - `void qc_cmp_seq_gate_init(qc_gate_internal_t *g)`
  - `void qc_cmp_seq_gate_x(qc_gate_internal_t *g, uint32_t target)`
  - `void qc_cmp_seq_gate_cx(qc_gate_internal_t *g, uint32_t target, uint32_t control)`
  - `void qc_cmp_seq_gate_ccx(qc_gate_internal_t *g, uint32_t target, uint32_t ctrl1, uint32_t ctrl2)`
  - `qc_sequence_t *qc_cmp_alloc_sequence(int num_layers, int max_gates_per_layer)`
  - `int qc_cmp_mcx_decomp_layers(int num_controls)`
  - `void qc_cmp_emit_mcx_decomp(qc_sequence_t *seq, int *layer, uint32_t target, const uint32_t *controls, int num_controls, int anc_start)`

**Test**: Build compiles (header is syntactically valid). No behavior change yet -- header is not included anywhere.

**Estimated lines**: ~60

---

## Step 2: Create `src/integer_comparison_ctrl.c` (~390 lines)

**What**: Move the three controlled CQ sequence builders AND the three public API wrappers into a new file.

**Details**:
- Create `src/integer_comparison_ctrl.c`
- File header comment referencing `refactor-4xn`
- `#include "comparison_internal.h"`
- Extern declarations for `qc_split_cq_add_seq` and `qc_split_cq_sub_seq` (from `hot_path_add.c`)
- Move these controlled sequence builders verbatim from `integer_comparison.c`:
  - `qc_c_cmp_cq_equal_seq` (lines 393-505)
  - `qc_c_cmp_cq_less_seq` (lines 507-589)
  - `qc_c_cmp_cq_greater_seq` (lines 591-614)
- Move these public API wrappers verbatim from `integer_comparison.c`:
  - `qc_cmp_cq_equal` (lines 670-725, 56 lines)
  - `qc_cmp_cq_less` (lines 727-774, 48 lines)
  - `qc_cmp_cq_greater` (lines 776-822, 47 lines)
- The wrappers call the uncontrolled `_seq` builders (e.g. `qc_cmp_cq_equal_seq`), which remain in `integer_comparison.c`. Add extern declarations for these in the ctrl file.
- Replace internal calls: `qc_seq_gate_init` -> `qc_cmp_seq_gate_init`, etc.
- Delete the moved functions from `integer_comparison.c`

**Why wrappers move here**: The public API wrappers are 153 lines (not ~70 as originally estimated). Keeping them in File 1 would push it to ~598 lines, exceeding 500. Moving them to the ctrl file keeps both files well under 500.

**Test**: `cmake -B build && cmake --build build && cd build && ctest --output-on-failure` -- all tests pass.

**Estimated lines**: ~390

---

## Step 3: Update `src/integer_comparison.c` to use shared header (~445 lines)

**What**: Replace the static helper definitions with the renamed `qc_cmp_` functions, defined here and declared via the shared header.

**Details**:
- Add `#include "comparison_internal.h"` at top
- Rename helper functions from `static` to non-static with `qc_cmp_` prefix:
  - `static qc_seq_gate_init` -> `qc_cmp_seq_gate_init` (remove `static`)
  - `static qc_seq_gate_x` -> `qc_cmp_seq_gate_x` (remove `static`)
  - `static qc_seq_gate_cx` -> `qc_cmp_seq_gate_cx` (remove `static`)
  - `static qc_seq_gate_ccx` -> `qc_cmp_seq_gate_ccx` (remove `static`)
  - `static alloc_sequence` -> `qc_cmp_alloc_sequence` (remove `static`)
  - `static mcx_decomp_layers` -> `qc_cmp_mcx_decomp_layers` (remove `static`)
  - `static emit_mcx_decomp` -> `qc_cmp_emit_mcx_decomp` (remove `static`)
- Update all call sites within `integer_comparison.c` to use the new names
- Remove the controlled function definitions (already moved in Step 2)
- Remove the public API wrappers (already moved in Step 2)
- Keep: uncontrolled _seq builders, QQ stubs

**Test**: `cmake -B build && cmake --build build && cd build && ctest --output-on-failure` -- all tests pass. Verify no file exceeds 500 lines.

**Estimated lines**: ~445

---

## Execution Order

Steps 1 and 2 depend on each other logically but can be done together in a single commit. Step 3 completes the refactor. In practice, all three steps should be done atomically in one commit to avoid broken intermediate states.

**Recommended**: Implement all three steps together, verify build + tests, commit once.

## Risk Assessment

- **Low risk**: This is pure file reorganization with no logic changes.
- **Build**: CMakeLists.txt uses `file(GLOB src/*.c)`, so the new `.c` file is automatically discovered. No CMake edits needed.
- **Symbol visibility**: The renamed helpers become non-static (visible to the linker). The `qc_cmp_` prefix prevents collisions with other translation units.
- **Extern declarations**: The `qc_split_cq_{add,sub}_seq` externs are duplicated in both files. This is acceptable for C and avoids coupling via an additional header.

## Verification Checklist

- [ ] `wc -l src/integer_comparison.c` <= 500
- [ ] `wc -l src/integer_comparison_ctrl.c` <= 500
- [ ] `wc -l src/comparison_internal.h` <= 100
- [ ] `cmake -B build && cmake --build build` -- zero warnings
- [ ] `cd build && ctest --output-on-failure` -- all tests pass
- [ ] `grep -c 'qc_c_cmp_cq_equal_seq\|qc_c_cmp_cq_less_seq\|qc_c_cmp_cq_greater_seq' src/integer_comparison.c` == 0 (controlled functions fully removed)
- [ ] `grep -c 'qc_c_cmp_cq_equal_seq\|qc_c_cmp_cq_less_seq\|qc_c_cmp_cq_greater_seq' src/integer_comparison_ctrl.c` >= 3 (controlled functions present in new file)
- [ ] `grep -c 'qc_cmp_cq_equal\b\|qc_cmp_cq_less\b\|qc_cmp_cq_greater\b' src/integer_comparison_ctrl.c` >= 3 (public API wrappers present in new file)
- [ ] `grep -c 'qc_cmp_cq_equal\b\|qc_cmp_cq_less\b\|qc_cmp_cq_greater\b' src/integer_comparison.c` == 0 (public API wrappers fully removed, only _seq variants remain)
