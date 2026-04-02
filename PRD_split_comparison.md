# PRD: Split integer_comparison.c (refactor-4xn)

## Status: COMPLETE (2026-04-02)

Split into `integer_comparison.c` (424 lines) and `integer_comparison_ctrl.c` (402 lines) with shared `comparison_internal.h` (55 lines). All under 500. All 17 tests pass.

## Problem

`src/integer_comparison.c` is 823 lines, exceeding the project's 500-line-per-module limit. The file contains six sequence builders (_seq functions), two QQ stubs, three public API wrappers, and shared internal helpers (gate initializers, sequence allocation, MCX decomposition).

## Goal

Split the file into two modules, each under 500 lines, while preserving all public and internal APIs unchanged.

## Current Structure (823 lines)

| Section | Lines | Description |
|---------|-------|-------------|
| File header + includes | 1-28 | Boilerplate, `#include "internal.h"`, extern for `qc_two_complement` |
| Gate initializers | 30-64 | `qc_seq_gate_init`, `qc_seq_gate_x`, `qc_seq_gate_cx`, `qc_seq_gate_ccx` (static) |
| Sequence allocation | 67-110 | `alloc_sequence` (static) |
| MCX decomposition | 114-168 | `mcx_decomp_layers`, `emit_mcx_decomp` (static) |
| CQ equal _seq | 170-276 | `qc_cmp_cq_equal_seq` (public) |
| CQ less _seq | 278-366 | `qc_cmp_cq_less_seq` (public), externs for split_cq_{add,sub}_seq |
| CQ greater _seq | 368-391 | `qc_cmp_cq_greater_seq` (public, delegates to less) |
| Controlled CQ equal _seq | 393-505 | `qc_c_cmp_cq_equal_seq` (public) |
| Controlled CQ less _seq | 507-589 | `qc_c_cmp_cq_less_seq` (public) |
| Controlled CQ greater _seq | 591-614 | `qc_c_cmp_cq_greater_seq` (public, delegates to c_less) |
| QQ stubs | 616-652 | `qc_cmp_qq_less_seq`, `qc_c_cmp_qq_less_seq` (stubs returning NULL) |
| Public API wrappers | 654-822 | `qc_cmp_cq_equal`, `qc_cmp_cq_less`, `qc_cmp_cq_greater` (with ancilla mgmt) |

## Proposed Split

### File 1: `integer_comparison.c` (keeps the name, ~445 lines)

Contains:
- All shared static helpers (gate initializers, `alloc_sequence`, MCX decomposition) -- promoted to file-scope non-static via a new internal header
- Uncontrolled CQ sequence builders: `qc_cmp_cq_equal_seq`, `qc_cmp_cq_less_seq`, `qc_cmp_cq_greater_seq`
- QQ stubs: `qc_cmp_qq_less_seq`, `qc_c_cmp_qq_less_seq`

Estimated: ~445 lines (28 header + 35 gate init + 44 alloc + 55 MCX + 107 equal_seq + 89 less_seq + 24 greater_seq + 37 QQ stubs + 16 comment block = ~435)

Note: The public API wrappers (`qc_cmp_cq_equal`, `qc_cmp_cq_less`, `qc_cmp_cq_greater`) are 153 lines (lines 670-822), not ~70 as previously estimated. Keeping them here would push File 1 to ~598 lines, exceeding 500. They are moved to File 2 instead.

### File 2: `integer_comparison_ctrl.c` (new file, ~390 lines)

Contains:
- `#include` of the shared internal header
- Controlled CQ sequence builders: `qc_c_cmp_cq_equal_seq`, `qc_c_cmp_cq_less_seq`, `qc_c_cmp_cq_greater_seq`
- Public API wrappers: `qc_cmp_cq_equal`, `qc_cmp_cq_less`, `qc_cmp_cq_greater` (allocate ancilla qubits and call `qc_run_instruction`)

Estimated: ~390 lines (15 header + 113 c_equal + 83 c_less + 24 c_greater + 153 wrappers = ~388)

### Shared header: `src/comparison_internal.h` (new file, ~60 lines)

Declares the helper functions that were previously `static` in `integer_comparison.c` and are now needed by both files:
- `qc_cmp_seq_gate_init`, `qc_cmp_seq_gate_x`, `qc_cmp_seq_gate_cx`, `qc_cmp_seq_gate_ccx`
- `qc_cmp_alloc_sequence`
- `qc_cmp_mcx_decomp_layers`, `qc_cmp_emit_mcx_decomp`

The `qc_cmp_` prefix avoids name collisions with other translation units.

## Constraints

1. **No header changes**: All public functions remain declared in `quantum_circuit.h`. No new public API.
2. **No logic changes**: Pure file reorganization. No algorithmic modifications.
3. **Build compatibility**: CMakeLists.txt uses `file(GLOB ...)` on `src/*.c`, so the new file is picked up automatically. No CMake changes needed.
4. **Each file under 500 lines**: File 1 ~445, File 2 ~390, shared header ~60.

## Success Criteria

1. `cmake -B build && cmake --build build` succeeds with zero warnings
2. `cd build && ctest --output-on-failure` passes all existing tests
3. No file in the split exceeds 500 lines
4. All public function signatures are unchanged
5. Gate counts produced by comparison operations are identical before and after the split

## Non-Goals

- Adding new comparison operations
- Changing any comparison algorithm
- Modifying the public header
- Adding new tests (existing tests validate correctness)
