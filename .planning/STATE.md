# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-01-25)

**Core value:** Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.
**Current focus:** Phase 5: Variable-Width Integers

## Current Position

Phase: 5 of 10 (Variable-Width Integers)
Plan: 2 of 3 in current phase
Status: In progress
Last activity: 2026-01-26 - Completed 05-02-PLAN.md (Width-aware arithmetic)

Progress: [████░░░░░░] 48%

## Performance Metrics

**Velocity:**
- Total plans completed: 15
- Average duration: 5.1 min
- Total execution time: 1.3 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 - Testing Foundation | 3 | 18 min | 6 min |
| 02 - C Layer Cleanup | 3 | 18 min | 6 min |
| 03 - Memory Architecture | 3 | 22 min | 7.3 min |
| 04 - Module Separation | 4 | 15 min | 3.8 min |
| 05 - Variable-Width Integers | 2 | 10 min | 5 min |

**Recent Trend:**
- Last 5 plans: 04-03 (4 min), 04-04 (3 min), 05-01 (4 min), 05-02 (6 min)
- Trend: Consistent execution, Phase 5 progressing well

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Bottom-up restructuring (C first): Foundation must be solid before adding features
- Open source release target: Requires clean code, docs, tests
- Keep circuit compilation model: Direct execution is future work
- Use Ruff instead of Black + isort + Flake8: Single tool, 10-100x faster (01-01)
- Use LLVM style for clang-format: Standard, readable, 100-column limit (01-01)
- Pre-commit hooks auto-fix formatting: Reduce manual work, ensure consistency (01-01)
- Characterization tests capture current behavior as-is: Purpose is regression detection, not correctness validation (01-02)
- Tests organized by functional area: qint operations, qbool operations, circuit generation (01-02)
- Auto-detect compiler in Makefile: Search for gcc/clang/cc rather than hardcoding (01-03)
- Use calloc for sequence_t array allocations: Ensures zero-initialization and prevents undefined behavior (02-01)
- Standard sequence_t initialization pattern: Allocate gates_per_layer and seq arrays immediately after malloc(sizeof(sequence_t)) (02-01)
- Cleanup-on-error pattern for complex allocations: Free in reverse order on any allocation failure (02-02)
- Temp pointer pattern for realloc: Preserves original pointer on failure (02-02)
- Return NULL from all allocation functions: Enables error propagation to callers (02-02)
- Explicit context passing via circuit_t* parameter: No global circuit variable (02-03)
- OWNERSHIP comments document memory responsibilities: Added at every allocation point (02-03)
- Keep instruction_list and QPU_state as globals for now: Stateless sequence generation, will address in Phase 4 (02-03)
- Hard-coded ALLOCATOR_MAX_QUBITS limit (8192): Prevents runaway allocation bugs (03-01)
- Freed stack only reuses single-qubit allocations initially: Simplified implementation, multi-qubit reuse can be added later (03-01)
- DEBUG_OWNERSHIP conditional compilation: Zero runtime overhead in production, enables ownership tracking for debugging (03-01)
- circuit_get_allocator() accessor for Python bindings: Follows C API pattern for opaque structs (03-01)
- QINT/QBOOL use allocator_alloc() with is_ancilla=true flag: Enables ancilla tracking, matches semantic meaning (03-02)
- free_element determines width from MSB: Enables correct allocator_free(start, width) for both QINT and QBOOL (03-02)
- Backward compat tracking maintained with documented quirk: Original decrement-by-1 behavior preserved during migration (03-02)
- Cast circuit_t* to circuit_s* in Cython calls: Matches C function signatures with forward-declared structs (03-03)
- Add qubit_allocator.c to setup.py sources: Required for linking circuit_get_allocator symbol (03-03)
- Cython cdef declarations at function start: Language requirement, before any Python statements (03-03)
- types.h as foundation module: Single source of truth for shared types (qubit_t, gate_t, sequence_t) with zero dependencies (04-01)
- definition.h as backward compatibility wrapper: Enables gradual migration from old code (04-01)
- Dependency comments in headers: Makes include hierarchy explicit and maintainable (04-01)
- optimizer.c module extraction: Gate optimization logic separated from QPU.c, reducing god object from 201 to 18 lines (04-02)
- Instruction state scope clarified: Globals kept only for sequence generation (CQ_add, CC_mul), not gate optimization (04-02)
- circuit_t typedef uses named struct: struct circuit_s pattern for forward declaration compatibility (04-02)
- circuit.h as main API header: Consolidates types, gates, optimizer, output, allocator into single user-facing include (04-03)
- circuit_output.h/c module: Separated print_circuit and circuit_to_opanqasm into dedicated module (04-03)
- QPU.h as backward compat wrapper: Now includes circuit.h, preserves instruction_t for sequence generation (04-03)
- Fixed filename typo: ciruict_outputs.c → circuit_output.c for consistency and professionalism (04-03)
- module_deps.md as comprehensive dependency documentation: Includes ASCII graph, line counts, responsibilities, and historical context (04-04)
- No .pxd changes needed for Cython: QPU.h backward compatibility wrapper sufficient for module separation (04-04)
- Verification includes full test suite: 59 tests confirm module separation doesn't break functionality (04-04)
- Right-aligned q_address array layout: indices [64-width] through [63] stores qubits for variable-width integers (05-01)
- Width stored as unsigned char: Supports 1-64 bit widths efficiently (05-01)
- QBOOL as QINT(circ, 1) wrapper: Single allocation code path, reduced complexity (05-01)
- MSB field kept for backward compat: Now points to first used element (64-width) (05-01)
- QINT_DEFAULT macro: Backward compatibility for C code calling QINT(circ) (05-01)
- QQ_add(int bits) signature: Width-parameterized addition replaces fixed INTEGERSIZE (05-02)
- cQQ_add(int bits) signature: Width-parameterized controlled addition (05-02)
- precompiled_XX_add_width[65] caches: Index 0 unused, 1-64 valid for width-specific caching (05-02)
- Legacy globals auto-populated: precompiled_QQ_add set when bits == INTEGERSIZE (05-02)

### Pending Todos

None yet.

### Blockers/Concerns

**Critical Path Dependencies:**
- Phase 5 plans 01-02 complete - width metadata and arithmetic foundation established
- Phase 5 plan 03 ready (Python bindings update)
- Phase 5 and Phase 6 can run in parallel

**Research Flags:**
- Phase 5: Medium priority - optimal gate sequences for variable-width arithmetic
- Phase 6: Medium priority - quantum bit shift/rotate circuits
- Phase 7: High priority - QFT-based arithmetic and modular operations

**Current Concerns:**
- Virtual environment symlinks point to macOS paths, need proper venv setup for local development (01-01, 01-02)
- Existing codebase has 65+ Ruff violations (bare except, tabs vs spaces) that need cleanup (01-01)
- Fixed critical C compilation issues in Integer.c and QPU.c (missing stdint.h) (01-02, 05-01)
- IntegerComparison.c uses conservative +10 buffer for layer allocation - may need precise calculation in future (02-01)
- All 59 tests pass with variable-width arithmetic

## Session Continuity

Last session: 2026-01-26
Stopped at: Completed 05-02-PLAN.md - Width-aware arithmetic complete
Resume file: None

## Phase 5 Progress

**Plan 01 Complete** - Width metadata foundation established
**Plan 02 Complete** - Width-aware arithmetic operations

**What was built in 05-02:**
- QQ_add(int bits) generates variable-width addition circuits
- cQQ_add(int bits) generates variable-width controlled addition
- precompiled_QQ_add_width[65] and precompiled_cQQ_add_width[65] caches
- Python bindings pass self.bits to parameterized functions
- Layer count scales correctly: 5*bits-2 for QQ_add

**Next steps:**
- 05-03: Python bindings update for remaining variable-width operations
