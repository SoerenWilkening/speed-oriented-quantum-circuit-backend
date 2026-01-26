# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-01-25)

**Core value:** Write quantum algorithms in natural programming style that compiles to efficient, memory-optimized quantum circuits.
**Current focus:** Phase 5: Variable-Width Integers

## Current Position

Phase: 5 of 10 (Variable-Width Integers)
Plan: 3 of 4 in current phase
Status: In progress
Last activity: 2026-01-26 - Completed 05-03-PLAN.md (Python qint width support)

Progress: [█████░░░░░] 50%

## Performance Metrics

**Velocity:**
- Total plans completed: 16
- Average duration: 5.4 min
- Total execution time: 1.5 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 - Testing Foundation | 3 | 18 min | 6 min |
| 02 - C Layer Cleanup | 3 | 18 min | 6 min |
| 03 - Memory Architecture | 3 | 22 min | 7.3 min |
| 04 - Module Separation | 4 | 15 min | 3.8 min |
| 05 - Variable-Width Integers | 3 | 21 min | 7 min |

**Recent Trend:**
- Last 5 plans: 04-04 (3 min), 05-01 (4 min), 05-02 (6 min), 05-03 (11 min)
- Trend: 05-03 took longer due to debugging qubit array compatibility

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
- width parameter primary with bits alias: Clean API, backward compat maintained (05-03)
- Default width 8 bits: Matches INTEGERSIZE constant (05-03)
- UserWarning for overflow, not error: Per CONTEXT.md, modular arithmetic expected (05-03)
- qbool unsigned 1-bit [0,1]: True=1 should not warn (05-03)
- Extract used qubits from right-aligned: C backend expects compact arrays (05-03)

### Pending Todos

None yet.

### Blockers/Concerns

**Critical Path Dependencies:**
- Phase 5 plans 01-03 complete - width metadata, arithmetic, Python API
- Phase 5 plan 04 ready (multiplication operations)
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
- Logic operations (and, or, invert) still use INTEGERSIZE layout - Python bindings adapted (05-03)
- All 59 tests pass with variable-width arithmetic

## Session Continuity

Last session: 2026-01-26
Stopped at: Completed 05-03-PLAN.md - Python qint width support complete
Resume file: None

## Phase 5 Progress

**Plan 01 Complete** - Width metadata foundation established
**Plan 02 Complete** - Width-aware arithmetic operations
**Plan 03 Complete** - Python qint width support

**What was built in 05-03:**
- qint(value, width=N) creates N-bit quantum integers
- qint.width property (read-only)
- Width validation (ValueError for < 1 or > 64)
- Overflow warnings when value exceeds width range
- qbool as syntactic sugar for qint(width=1)
- Fixed qubit array extraction for C backend

**Next steps:**
- 05-04: Multiplication and remaining variable-width operations
