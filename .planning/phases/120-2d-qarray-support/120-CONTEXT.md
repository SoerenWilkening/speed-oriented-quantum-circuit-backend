# Phase 120: 2D Qarray Support - Context

**Gathered:** 2026-03-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix 2D qarray construction and indexing so users can create and manipulate board-like quantum data structures. Prerequisite for Phase 121 (Chess Engine Rewrite). Existing qarray class already has 2D scaffolding — this phase fixes bugs and ensures the complete path works end-to-end.

</domain>

<decisions>
## Implementation Decisions

### API naming
- `ql.qarray()` is the canonical constructor (matches `ql.qint()` / `ql.qbool()` naming pattern)
- `ql.array()` remains as an undocumented alias for backward compatibility — no deprecation warning, just not the documented API
- `ql.qarray()` accepts data as positional argument: `ql.qarray([1, 2, 3])` and `ql.qarray(dim=(8, 8))`
- Default dtype is `qint` when not specified (consistent with current behavior)

### Mutation semantics
- `arr[r, c]` returns the actual stored qbool/qint object (view semantics, preserves quantum identity)
- `arr[r, c] |= flag` works in-place: `__getitem__` returns stored element, `__ior__` emits quantum gate, `__setitem__` writes back same object
- Direct assignment `arr[r, c] = new_qbool` also supported (replaces stored element)
- Row/column slicing (`board[r, :]`, `board[:, c]`) returns view arrays sharing the same qubit objects
- Classical `int` indexing only — no quantum indexing with `qint` (raises TypeError)

### Phase scope
- Fix bugs only — no convenience methods (no `.rows()`, `.cols()`, `.reshape()`, `.flatten()`)
- Both nested list construction (`ql.qarray([[1,2],[3,4]])`) and dimension-based (`ql.qarray(dim=(2,2))`) must work
- Tests use small arrays (2x2, 3x3) to stay within 17-qubit simulation limit
- No 8x8 simulation tests — chess engine is circuit-build-only
- Update CLAUDE.md examples to use `ql.qarray()` instead of `ql.array()`

### Claude's Discretion
- Specific bug identification and fix approach (researcher/planner determines what's actually broken)
- Test organization and naming
- Error message wording for invalid 2D operations

</decisions>

<specifics>
## Specific Ideas

- API naming follows the `ql.qint()` / `ql.qbool()` / `ql.qarray()` type-constructor pattern
- Chess engine (Phase 121) will use classical `for r in range(8): for c in range(8):` loops with `board[r, c]` access
- Quantum identity preservation is critical: the qbool at `board[r, c]` must be the same qubit object, not a copy

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `qarray` class (`src/quantum_language/qarray.pyx`): Already has `dim=` construction, `_multi_to_flat()`, `_handle_multi_index()`, `__setitem__` tuple path
- `_qarray_utils.py`: `_detect_shape()`, `_flatten()` for nested list processing
- `__init__.py`: `array()` factory function at line 65 wrapping `qarray` constructor

### Established Patterns
- `_create_view()` static method creates qarrays sharing element references (view semantics)
- Element storage is flat `_elements` list with `_shape` tuple for multi-dimensional interpretation
- Row-major ordering via `_multi_to_flat()` stride calculation

### Integration Points
- `__init__.py` exports: Need to expose `ql.qarray` as the canonical constructor (currently exported as class, not as a function wrapper)
- `_core.pyx` has a separate legacy `array()` function (line 373) with different signature — not related to this work
- CLAUDE.md "Quantum array" section needs updating to show `ql.qarray()` API

</code_context>

<deferred>
## Deferred Ideas

- Quantum indexing with qint indices (QRAM-select) — major feature, separate phase
- Convenience iteration methods (.rows(), .cols()) — add if needed later
- Sparse 2D qarray — explicitly out of scope per REQUIREMENTS.md
- Reshape/flatten methods — add if needed later

</deferred>

---

*Phase: 120-2d-qarray-support*
*Context gathered: 2026-03-09*
