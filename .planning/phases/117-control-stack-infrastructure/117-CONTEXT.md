# Phase 117: Control Stack Infrastructure - Context

**Gathered:** 2026-03-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace flat `_controlled`/`_control_bool` globals in `_core.pyx` with a stack-based control context (`_control_stack`), add Toffoli AND emission primitives (`emit_ccx`, `_toffoli_and`, `_uncompute_toffoli_and`), and update `__enter__`/`__exit__` to use push/pop semantics for single-level equivalence. Phase 118 will add multi-level AND-ancilla composition on top of this infrastructure.

</domain>

<decisions>
## Implementation Decisions

### Stack entry design
- Minimal tuple format: `(qbool_ref, and_ancilla_or_None)`
- Stack depth implies controlled state: `_get_controlled()` returns `len(_control_stack) > 0`
- Active control qubit: at depth 1 returns the qbool itself (`stack[-1][0]`); at depth 2+ returns the AND-ancilla (`stack[-1][1]`)
- `_list_of_controls` replaced entirely by the stack — remove `_list_of_controls`, `_get_list_of_controls()`, `_set_list_of_controls()`
- Stack stored as `cdef list _control_stack = []` in `_core.pyx`, consistent with existing global pattern
- Accessors: `_get_control_stack()`, `_push_control(qbool_ref, and_ancilla)`, `_pop_control()`

### Toffoli AND helpers
- `emit_ccx` lives in `_gates.pyx` alongside other `emit_*` functions — same `add_gate()` C call pattern
- `_toffoli_and` and `_uncompute_toffoli_and` also live in `_gates.pyx` — gate emission helpers calling `emit_ccx` internally
- `_toffoli_and` allocates a `qbool` (not raw qubit) for the AND-ancilla — `_get_control_bool()` can return it directly and `_gates.pyx` reads `.qubits[63]` as usual
- Existing `&` operator on qbool left as-is — it has complex dependency tracking semantics. New helpers are lightweight alternatives for control stack use. Phase 118+ can decide whether to unify

### Backward compatibility strategy
- `_get_controlled()` becomes thin wrapper: `return len(_control_stack) > 0`
- `_set_controlled()` becomes no-op (controlled state implicit from stack depth)
- `_get_control_bool()` reads stack top: returns `entry[1] if entry[1] is not None else entry[0]`
- `_set_control_bool()` becomes no-op (managed by push/pop)
- `__enter__` updated in Phase 117 to call `_push_control(self, None)` — validates stack works before Phase 118 adds AND composition
- `__exit__` updated to call `_pop_control()` instead of `_set_controlled(False)`
- `circuit()` reset clears `_control_stack = []`, removing old `_controlled`, `_control_bool`, `_list_of_controls` globals
- `compile.py` save/restore updated to save/restore full stack via shallow copy: `saved_stack = list(_get_control_stack())`

### Ancilla lifecycle
- AND-ancilla qubits allocated from main `qubit_allocator_t` via `qbool()` — no separate pool
- Uncomputation ordering in `__exit__`: (1) scope cleanup (uncompute scope-internal qints), (2) uncompute AND-ancilla via reverse CCX, (3) pop stack entry
- AND-ancilla deallocated immediately on `__exit__` after uncomputation — qubit returns to free list for reuse
- After reverse CCX, set `ancilla._uncomputed = True` to prevent `__del__` double-uncomputation

### Claude's Discretion
- Exact error messages for edge cases (e.g., pop from empty stack)
- Whether to add debug logging for stack push/pop operations
- Internal organization of new accessor functions within `_core.pyx`

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `_core.pyx` global pattern: `cdef` module globals + Python accessor functions — stack follows same pattern
- `_gates.pyx` emit pattern: `memset(&g, 0, sizeof(gate_t))` + gate builder + `add_gate()` — `emit_ccx` follows same pattern
- `qbool()` constructor: allocates 1-bit qint with `.qubits[63]` — reusable for AND-ancilla allocation
- `ccx()` C function: already available via gate.h extern — no new C code needed for emit_ccx

### Established Patterns
- All gate emission checks `_get_controlled()` then reads `_get_control_bool().qubits[63]` — no change needed in `_gates.pyx`
- `circuit.__init__()` resets all Python globals — add `_control_stack = []` to reset block
- `compile.py` saves/restores control context before capture — update to stack-based save/restore
- `_uncomputed` flag on qint: used to prevent double-uncomputation — same pattern for AND-ancillas

### Integration Points
- `_core.pyx:28-29`: Replace `_controlled`/`_control_bool` globals with `_control_stack`
- `_core.pyx:91-107`: Replace get/set accessors with stack-based wrappers
- `_core.pyx:387-401`: Update `circuit()` reset to clear stack
- `qint.pyx:784-834`: Update `__enter__` to use `_push_control`
- `qint.pyx:836-882`: Update `__exit__` to use `_pop_control`
- `compile.py`: Update control context save/restore to use full stack
- `_gates.pyx`: Add `emit_ccx`, `_toffoli_and`, `_uncompute_toffoli_and`

</code_context>

<specifics>
## Specific Ideas

No specific requirements — standard infrastructure refactoring following existing codebase patterns.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 117-control-stack-infrastructure*
*Context gathered: 2026-03-09*
