# Phase 105: Full Walk Operators - Context

**Gathered:** 2026-03-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Compose a complete quantum walk step U = R_B * R_A from manual height-controlled diffusion operators using raw primitives. R_A applies diffusion at even depths (excluding root), R_B at odd depths plus root, and the walk step is compiled via `@ql.compile`. Phase 104's `apply_diffusion()` is the building block; this phase composes it into the full walk operator.

</domain>

<decisions>
## Implementation Decisions

### Height-controlled diffusion cascade (WALK-04)
- `apply_diffusion()` already uses `h_qubit_idx` as a control qubit -- height-controlled activation is built into the existing D_x implementation
- The "cascade" is simply looping `apply_diffusion()` over the correct depth levels -- each call is independently height-controlled
- No additional height-conditioning infrastructure needed beyond what Phase 104 provides

### R_A operator (WALK-05)
- Loop `apply_diffusion()` over even depths (0, 2, 4, ...), excluding root (depth == max_depth)
- Follows `walk.py:R_A()` (line 1099) pattern exactly
- Depth 0 (leaves) included for completeness even though it's a no-op -- matches QWalkTree convention
- Standalone function `r_a()` in chess_walk.py (purely functional, no class)

### R_B operator (WALK-06)
- Loop `apply_diffusion()` over odd depths (1, 3, 5, ...) plus root
- Root always in R_B regardless of max_depth parity -- add explicitly if max_depth is even (matches `walk.py:R_B()` line 1115)
- Standalone function `r_b()` in chess_walk.py

### Walk step compilation (WALK-07)
- Compile U = R_B * R_A via `@ql.compile` using the `_all_qubits_register` trick from QWalkTree (walk.py line 1184)
- Create a helper to build a qint wrapping all tree qubits (height + all branch registers) so none are treated as internal ancillas
- This enables cache reuse and automatic controlled variant derivation
- `walk_step()` function: first call captures, subsequent calls replay

### Testing approach
- Component tests within 17-qubit budget: verify R_A/R_B call apply_diffusion at correct depths
- Structural verification: check that R_A and R_B height controls are disjoint (port `verify_disjointness()` logic as a standalone check)
- Walk step compilation test: verify walk_step produces a valid circuit (gate count > 0, no errors)
- No simulation tests -- chess circuits exceed 17-qubit budget; circuit generation only

### Claude's Discretion
- Exact function signatures and parameter ordering for r_a(), r_b(), walk_step()
- Whether _all_qubits_register is a module-level helper or inline
- Disjointness verification as a separate function or inline assertion
- Test case depth/branching values for component tests

</decisions>

<specifics>
## Specific Ideas

- Follow QWalkTree walk.py R_A/R_B/walk_step pattern closely -- this is a proven implementation
- Keep compact implementation style consistent with Phase 104's chess_walk.py
- The `_all_qubits_register` trick (walk.py line 1184-1210) is essential for preventing forward-call tracking from blocking repeated walk_step() invocations

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `chess_walk.py:apply_diffusion()`: Complete D_x with height control, validity evaluation, Montanaro angles -- the core building block
- `chess_walk.py:height_qubit()`: Physical qubit index lookup for height register
- `chess_walk.py:prepare_walk_data()`: Precomputes move data per level
- `walk.py:R_A()` (line 1099), `R_B()` (line 1115), `walk_step()` (line 1212): Reference implementations
- `walk.py:_all_qubits_register()` (line 1184): Pattern for wrapping all tree qubits into a single qint for compile

### Established Patterns
- @ql.compile with key=lambda for cache key derivation (walk.py line 1247)
- Purely functional module style in chess_walk.py (no class, standalone functions)
- Root always in R_B regardless of max_depth parity (Montanaro convention)

### Integration Points
- chess_walk.py:apply_diffusion() is the sole building block for R_A/R_B
- Phase 106 demo scripts will call walk_step() as the main walk operator
- Tests in tests/python/test_chess_walk.py (extend existing test file)

</code_context>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 105-full-walk-operators*
*Context gathered: 2026-03-05*
