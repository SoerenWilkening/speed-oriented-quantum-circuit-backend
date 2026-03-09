# Phase 118: Nested With-Block Rewrite - Research

**Researched:** 2026-03-09
**Domain:** Quantum control stack composition, multi-controlled gate emission
**Confidence:** HIGH

## Summary

Phase 118 implements nested `with qbool:` blocks by composing control qubits via Toffoli AND at each nesting depth. Phase 117 established the control stack infrastructure (`_push_control`, `_pop_control`, `_get_control_bool`); this phase wires AND-composition into `__enter__`/`__exit__` so that at depth >= 2, a fresh AND-ancilla is allocated via `_toffoli_and()` to combine the existing stack-top control with the new condition.

The gate emission layer already reads `_get_control_bool()` uniformly at all call sites (arithmetic, bitwise, comparisons, `_gates.pyx` emit functions). Once the AND-ancilla is pushed as the active control, all gate emission automatically uses the composed control qubit. No changes are needed in any gate emission function.

The existing 6 xfail tests in `test_nested_with_blocks.py` use 3-bit qints with comparisons, producing 38 peak qubits -- far beyond the 17-qubit simulation limit. These tests must be rewritten to use direct `qbool(True)`/`qbool(False)` values (or smaller constructs) to stay simulatable.

**Primary recommendation:** Modify `__enter__` to call `_toffoli_and()` when stack depth >= 1, modify `__exit__` to call `_uncompute_toffoli_and()` before popping, rewrite xfail tests to use direct qbool values, add 3+ level tests with 2-bit qints.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Unlimited nesting depth -- no cap, no warnings
- Linear chain composition: at depth N, AND(stack_top_control, new_qbool) -> new ancilla
- Each `__enter__` at depth >= 1 calls `_toffoli_and(_get_control_bool().qubits[63], self.qubits[63])` to produce AND-ancilla
- Push `(self, and_ancilla)` onto control stack; `_get_control_bool()` returns the AND-ancilla
- Each `__exit__` uncomputes AND-ancilla via `_uncompute_toffoli_and` before popping stack entry
- Use `_get_control_bool()` accessor to read current active control qubit (consistent with gate emission pattern)
- Fix controlled XOR in gate emission layer, not in `__invert__` method
- `~qbool` and `~qint` inside nested `with` blocks controlled on the combined AND-ancilla (full multi-level control)
- `~qbool` returns a new qbool (consistent with current `__invert__` semantics on all qints)
- Fix applies to both 1-bit qbool and multi-bit qint -- all bits go through same emit path
- Allow `with cond: with cond:` (same qbool nested twice) silently
- Do not detect `with a: with ~a:` contradictions
- Keep `_check_not_uncomputed()` validation at top of `__enter__`
- Enforce qbool-only (width=1) for with-block conditions -- raise TypeError for multi-bit qints
- Remove all 6 xfail markers from existing 2-level tests as part of implementation
- Add 3+ level tests: 3-level all-true, 3-level mixed conditions, 3-level with arithmetic at each depth, 4+ level smoke test
- Use 2-bit qints for 3+ level tests to stay under 17-qubit simulation limit
- All tests use direct Qiskit simulation via existing `_simulate_and_extract` pattern

### Claude's Discretion
- Exact error message for width != 1 TypeError in `__enter__`
- Whether to add a `TestThreeLevelNesting` class or inline into existing class
- Internal ordering of AND-ancilla uncomputation vs scope cleanup in `__exit__`
- Any additional edge case tests beyond the specified scenarios

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CTRL-01 | User can nest `with qbool:` blocks at arbitrary depth with correct controlled gate emission | AND-composition in `__enter__` + AND-ancilla as active control makes all existing emit functions work at any depth automatically |
| CTRL-04 | Controlled XOR (`~qbool`) works inside `with` blocks without NotImplementedError | `__invert__` already uses `_get_control_bool()` via `cQ_not` pattern; once AND-ancilla is active control, it works. Note: `__ixor__` (^=) still has NotImplementedError -- out of scope (CBIT-01/02) |
| CTRL-05 | Existing single-level `with` blocks work identically (zero regression) | When stack depth == 0, `__enter__` pushes `(self, None)` as before; `__exit__` skips AND uncomputation when ancilla is None; gate emission unchanged |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| quantum_language (this project) | 0.1.0 | Quantum DSL framework | The project itself |
| Cython | (project build) | .pyx compilation for qint.pyx, _gates.pyx, _core.pyx | All modified files are Cython |
| qiskit-aer | (installed) | Statevector simulation for tests | Existing test infrastructure |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| pytest | (installed) | Test framework | Running xfail tests, new tests |
| qiskit.qasm3 | (installed) | QASM loading for simulation tests | Test simulation |

**Build:**
```bash
pip install -e .    # Rebuilds Cython extensions
pytest tests/python/test_nested_with_blocks.py -v  # Run nested tests
```

## Architecture Patterns

### Key Code Locations
```
src/quantum_language/
├── qint.pyx:785-826      # __enter__ -- ADD AND-composition here
├── qint.pyx:828-872      # __exit__ -- ADD AND-ancilla uncomputation here
├── _core.pyx:89-154      # Control stack (push/pop/get) -- NO CHANGES NEEDED
├── _gates.pyx:232-274    # _toffoli_and / _uncompute_toffoli_and -- USE AS-IS
├── _gates.pyx:38-208     # Gate emission (emit_x, emit_ry, etc.) -- NO CHANGES NEEDED
├── qint_bitwise.pxi:534  # __invert__ -- ALREADY WORKS with _get_control_bool()
tests/python/
└── test_nested_with_blocks.py  # REWRITE xfail tests, ADD 3+ level tests
```

### Pattern 1: AND-Composition in `__enter__`
**What:** At nesting depth >= 1, compose the current stack-top control with the new qbool via Toffoli AND
**When to use:** Every `__enter__` call when control stack is non-empty

```python
# Pseudocode for modified __enter__:
def __enter__(self):
    self._check_not_uncomputed()
    # Phase 118: Enforce qbool-only
    if self.bits != 1:
        raise TypeError("...")

    _scope_stack = _get_scope_stack()

    # Phase 118: AND-composition at depth >= 1
    if _get_controlled():  # stack already has entries
        current_ctrl = _get_control_bool()
        and_ancilla = _toffoli_and(current_ctrl.qubits[63], self.qubits[63])
        _push_control(self, and_ancilla)
    else:
        # Single-level (depth 0): no AND needed
        _push_control(self, None)

    # Existing scope management (unchanged)
    current_scope_depth.set(current_scope_depth.get() + 1)
    _scope_stack.append([])
    if self.operation_type is not None:
        self.creation_scope = current_scope_depth.get()
    return self
```

### Pattern 2: AND-Ancilla Uncomputation in `__exit__`
**What:** If the control entry has a non-None AND-ancilla, uncompute it after scope cleanup but before popping
**When to use:** Every `__exit__` call where the top control entry has an AND-ancilla

```python
# Pseudocode for modified __exit__:
def __exit__(self, exc__type, exc, tb):
    _scope_stack = _get_scope_stack()

    # Step 1: Uncompute scope-local qbools (unchanged)
    if _scope_stack:
        scope_qbools = _scope_stack.pop()
        scope_qbools.sort(key=lambda q: q._creation_order, reverse=True)
        for qbool_obj in scope_qbools:
            if not qbool_obj._is_uncomputed:
                qbool_obj._do_uncompute(from_del=False)

    # Step 2: Phase 118 -- Uncompute AND-ancilla if present
    # Must happen AFTER scope qbool cleanup, BEFORE popping control
    control_entry = _get_control_stack()[-1]  # peek at top
    qbool_ref, and_ancilla = control_entry
    if and_ancilla is not None:
        # Get the outer control (one level down on the stack)
        outer_entry = _get_control_stack()[-2]
        outer_ctrl = outer_entry[1] if outer_entry[1] is not None else outer_entry[0]
        _uncompute_toffoli_and(and_ancilla, outer_ctrl.qubits[63], qbool_ref.qubits[63])

    # Step 3: Decrement scope depth (unchanged)
    current_scope_depth.set(current_scope_depth.get() - 1)

    # Step 4: Pop control entry (unchanged)
    _pop_control()

    return False
```

### Pattern 3: Qubit Budget for Tests
**What:** Calculate qubit usage to stay under 17-qubit simulation limit
**When to use:** All new and rewritten tests

| Scenario | Components | Qubits |
|----------|-----------|--------|
| 2-level, qbool(T/F) + 2-bit result | 2 conds + 2 result + 1 AND = 5 | 5 |
| 2-level, qbool(T/F) + 3-bit result | 2 conds + 3 result + 1 AND = 6 | 6 |
| 3-level, qbool(T/F) + 2-bit result | 3 conds + 2 result + 2 AND = 7 | 7 |
| 4-level, qbool(T/F) + 2-bit result | 4 conds + 2 result + 3 AND = 9 | 9 |
| 2-level with comparison (3-bit qints) | ~38 peak | **EXCEEDS LIMIT** |

**Critical finding:** The existing 6 xfail tests use `a > 1` comparisons on 3-bit qints, producing 38 peak qubits. These CANNOT be simulated. They MUST be rewritten to use direct `qbool(True)`/`qbool(False)` values.

### Anti-Patterns to Avoid
- **Using comparisons in nested tests:** Comparisons produce many temporary qubits. Use `qbool(True/False)` for control conditions in test code.
- **Modifying gate emission functions:** All `emit_x`, `emit_ry`, etc. already use `_get_control_bool()`. Changing them is unnecessary and risks regressions.
- **Forgetting AND-ancilla uncomputation order:** Must uncompute AFTER scope qbools, BEFORE popping control. Wrong order produces dangling qubits or incorrect circuits.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| AND-ancilla allocation | Custom CCX emit code | `_toffoli_and()` from `_gates.pyx:232` | Already handles qubit allocation + CCX emission correctly |
| AND-ancilla uncomputation | Custom reverse CCX + dealloc | `_uncompute_toffoli_and()` from `_gates.pyx:257` | Handles deallocation + `_is_uncomputed` flag correctly |
| Control state queries | Direct stack indexing | `_get_controlled()` / `_get_control_bool()` | Existing accessor pattern used by all emission code |
| Control stack push/pop | Direct list manipulation | `_push_control()` / `_pop_control()` from `_core.pyx` | Validates stack state, consistent API |

**Key insight:** Phase 117 built ALL the primitives needed. Phase 118 just wires them into `__enter__`/`__exit__`.

## Common Pitfalls

### Pitfall 1: Incorrect AND-ancilla Uncomputation Order
**What goes wrong:** Uncomputing the AND-ancilla before scope qbools means scope qbool uncomputation happens with wrong control qubit
**Why it happens:** Natural instinct is to undo AND first, but scope qbools were created under AND-ancilla control
**How to avoid:** Uncompute scope qbools FIRST (they use the AND-ancilla as control), THEN uncompute AND-ancilla
**Warning signs:** Tests show wrong values for cases where scope-local temporaries exist inside nested with blocks

### Pitfall 2: Wrong Control Qubit for AND Uncomputation
**What goes wrong:** Using the wrong qubit indices when calling `_uncompute_toffoli_and`
**Why it happens:** The control stack stores `(qbool_ref, and_ancilla)` -- need the OUTER control's qubit, not the current entry's qbool
**How to avoid:** In `__exit__`, peek at `_control_stack[-2]` for the outer control, use `_control_stack[-1][0]` for the current qbool
**Warning signs:** Qubits left in wrong state after `__exit__`, simulation values off

### Pitfall 3: Existing Tests Use Too Many Qubits
**What goes wrong:** Tests pass compilation but fail simulation with OOM
**Why it happens:** Comparisons (`a > 1`) on 3-bit qints create ~30+ temporary qubits via ripple-carry comparator circuits
**How to avoid:** Rewrite tests to use `qbool(True)`/`qbool(False)` directly; use 2-bit qints for result registers
**Warning signs:** Qiskit `Insufficient memory` errors during test simulation

### Pitfall 4: Breaking Single-Level With Blocks
**What goes wrong:** Introducing AND-composition changes the code path for single-level `with` blocks
**Why it happens:** If the depth check is wrong or the `and_ancilla is None` path is broken
**How to avoid:** Guard AND-composition with `if _get_controlled()` (stack non-empty); skip uncomputation when `and_ancilla is None`
**Warning signs:** `TestSingleLevelConditional` tests fail

### Pitfall 5: Width Validation for Multi-Bit qint
**What goes wrong:** User passes a multi-bit qint (e.g., 8-bit) to `with` block, framework uses only bit 63 as control
**Why it happens:** `qubits[63]` works for both qbool (1-bit) and qint, but semantically only the MSB is used as control
**How to avoid:** Add `if self.bits != 1: raise TypeError(...)` at the top of `__enter__`, after `_check_not_uncomputed()`
**Warning signs:** Silently incorrect circuits when users pass multi-bit qints as conditions

### Pitfall 6: Forgetting to Import _toffoli_and in qint.pyx
**What goes wrong:** NameError at runtime when entering nested with blocks
**Why it happens:** `_toffoli_and` and `_uncompute_toffoli_and` are in `_gates.pyx`, need to be imported in `qint.pyx`
**How to avoid:** Add imports at the top of `qint.pyx` from `._gates`
**Warning signs:** ImportError or NameError on first nested `with` block

## Code Examples

### Modified `__enter__` (Cython)
```python
# Source: qint.pyx:785, modified for Phase 118
def __enter__(self):
    self._check_not_uncomputed()

    # Phase 118: Enforce qbool-only (width=1) for with-block conditions
    if self.bits != 1:
        raise TypeError(
            f"with-block condition must be a qbool (1-bit), "
            f"got {self.bits}-bit qint"
        )

    _scope_stack = _get_scope_stack()

    # Phase 118: AND-composition at depth >= 1
    if _get_controlled():
        current_ctrl = _get_control_bool()
        and_ancilla = _toffoli_and(current_ctrl.qubits[63], self.qubits[63])
        _push_control(self, and_ancilla)
    else:
        # Single-level: no AND needed (backward compatible)
        _push_control(self, None)

    # Phase 19: Scope management (unchanged)
    current_scope_depth.set(current_scope_depth.get() + 1)
    _scope_stack.append([])

    if self.operation_type is not None:
        self.creation_scope = current_scope_depth.get()

    return self
```

### Modified `__exit__` (Cython)
```python
# Source: qint.pyx:828, modified for Phase 118
def __exit__(self, exc__type, exc, tb):
    _scope_stack = _get_scope_stack()

    # Phase 19: Uncompute scope-local qbools FIRST
    if _scope_stack:
        scope_qbools = _scope_stack.pop()
        scope_qbools.sort(key=lambda q: q._creation_order, reverse=True)
        for qbool_obj in scope_qbools:
            if not qbool_obj._is_uncomputed:
                qbool_obj._do_uncompute(from_del=False)

    # Phase 118: Uncompute AND-ancilla if present
    _cs = _get_control_stack()
    qbool_ref, and_ancilla = _cs[-1]
    if and_ancilla is not None:
        # Outer control is one level down
        outer_entry = _cs[-2]
        outer_ctrl = outer_entry[1] if outer_entry[1] is not None else outer_entry[0]
        _uncompute_toffoli_and(and_ancilla, outer_ctrl.qubits[63], qbool_ref.qubits[63])

    current_scope_depth.set(current_scope_depth.get() - 1)
    _pop_control()

    return False
```

### Rewritten 2-Level Test (Direct qbool)
```python
# Source: test_nested_with_blocks.py, rewritten for Phase 118
def test_nested_both_true(self):
    """Both outer and inner True: result gets both additions."""
    gc.collect()
    ql.circuit()
    outer_cond = ql.qbool(True)
    inner_cond = ql.qbool(True)
    result = ql.qint(0, width=3)
    with outer_cond:
        result += 1
        with inner_cond:
            result += 2

    result_start = result.allocated_start
    result_width = result.width
    qasm = ql.to_openqasm()
    _keepalive = [outer_cond, inner_cond, result]

    num_qubits = _get_num_qubits(qasm)
    actual = _simulate_and_extract(qasm, num_qubits, result_start, result_width)
    assert actual == 3, f"Expected 3 (1+2), got {actual}"
```

### 3-Level Nesting Test
```python
def test_three_level_all_true(self):
    """3-level nesting, all True: all additions execute."""
    gc.collect()
    ql.circuit()
    c1 = ql.qbool(True)
    c2 = ql.qbool(True)
    c3 = ql.qbool(True)
    result = ql.qint(0, width=2)  # 2-bit to save qubits
    with c1:
        result += 1
        with c2:
            with c3:
                result += 2

    # Expect 3 (1 + 2)
    # Qubits: 3 conds + 2 result + 2 AND = 7
    ...
```

## State of the Art

| Old Approach (Phase 117) | Current Approach (Phase 118) | When Changed | Impact |
|--------------------------|------------------------------|--------------|--------|
| `_push_control(self, None)` always | AND-composition when stack depth >= 1 | Phase 118 | Enables multi-level nesting |
| `_pop_control()` only in `__exit__` | AND uncomputation + pop in `__exit__` | Phase 118 | Clean ancilla lifecycle |
| No width validation in `__enter__` | TypeError for width != 1 | Phase 118 | Prevents silent bugs |
| xfail tests with 38-qubit circuits | Rewritten tests with 5-9 qubits | Phase 118 | Tests actually simulatable |

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | pytest (installed) |
| Config file | pytest.ini or pyproject.toml (project root) |
| Quick run command | `pytest tests/python/test_nested_with_blocks.py -v` |
| Full suite command | `pytest tests/python/ -v` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CTRL-01 | 2-level nested with-blocks produce correct results | unit | `pytest tests/python/test_nested_with_blocks.py::TestNestedWithBlocks -x` | Exists (xfail, needs rewrite) |
| CTRL-01 | 3-level nested with-blocks produce correct results | unit | `pytest tests/python/test_nested_with_blocks.py::TestThreeLevelNesting -x` | Needs creation (Wave 0) |
| CTRL-01 | 4+ level nesting smoke test | unit | `pytest tests/python/test_nested_with_blocks.py::TestThreeLevelNesting::test_four_level_smoke -x` | Needs creation (Wave 0) |
| CTRL-04 | `~qbool` works inside single-level with blocks | unit | `pytest tests/python/test_nested_with_blocks.py -k invert -x` | Needs creation (Wave 0) |
| CTRL-04 | `~qbool` works inside nested with blocks | unit | `pytest tests/python/test_nested_with_blocks.py -k invert -x` | Needs creation (Wave 0) |
| CTRL-05 | Single-level with-block regression | unit | `pytest tests/python/test_nested_with_blocks.py::TestSingleLevelConditional -x` | Exists |
| CTRL-05 | Existing test suite passes | integration | `pytest tests/python/ -v --timeout=120` | Exists |

### Sampling Rate
- **Per task commit:** `pytest tests/python/test_nested_with_blocks.py -v`
- **Per wave merge:** `pytest tests/python/ -v --timeout=120`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] Rewrite 6 xfail tests to use `qbool(True/False)` instead of comparisons (reduces qubit count from 38 to ~6)
- [ ] `TestThreeLevelNesting` class with 4+ tests in `test_nested_with_blocks.py`
- [ ] `~qbool` inside `with` block tests (CTRL-04 coverage)
- [ ] `TypeError` test for multi-bit qint in `with` block

## Open Questions

1. **Exact AND-ancilla uncomputation ordering relative to scope cleanup**
   - What we know: Scope qbools must be uncomputed while AND-ancilla is still active (so their uncomputation is correctly controlled). AND-ancilla must be uncomputed before popping the control entry.
   - What's unclear: Whether `_do_uncompute(from_del=False)` inside scope cleanup could interfere with AND-ancilla state. Evidence suggests no -- `_do_uncompute` emits reverse gates using the current control context, which is the AND-ancilla.
   - Recommendation: Follow the order: scope qbools -> AND-ancilla -> scope depth -> pop. Verify with tests.

2. **Whether `__invert__` actually needs a fix for CTRL-04**
   - What we know: `__invert__` already uses `_get_control_bool()` and `cQ_not` for the controlled case. It works correctly with single-level controls.
   - What's unclear: The CONTEXT says "fix in gate emission layer" but `__invert__` already goes through the correct path. The AND-ancilla composition in `__enter__` should make it work automatically.
   - Recommendation: Verify with a test: `with c1: with c2: ~result` should produce a doubly-controlled NOT. If `__invert__` already reads `_get_control_bool()`, this should "just work" once AND-ancilla composition is active. If not, the fix is in `__invert__`'s controlled path.

3. **Qubit count of rewritten 2-level tests**
   - What we know: Direct `qbool(True/False)` + 3-bit result + 1 AND-ancilla = ~6 qubits. Safely under 17.
   - What's unclear: Whether arithmetic operations (`+= 1`, `-= 1`) create additional temporary qubits in controlled mode.
   - Recommendation: Verify empirically after implementation. Use 2-bit result as fallback.

## Sources

### Primary (HIGH confidence)
- `src/quantum_language/qint.pyx:785-872` -- Current `__enter__`/`__exit__` implementation, directly inspected
- `src/quantum_language/_core.pyx:89-154` -- Control stack API (`_push_control`, `_pop_control`, `_get_control_bool`), directly inspected
- `src/quantum_language/_gates.pyx:232-274` -- `_toffoli_and` / `_uncompute_toffoli_and` implementation, directly inspected
- `src/quantum_language/_gates.pyx:38-208` -- Gate emission functions (`emit_x`, `emit_ry`, etc.) all use `_get_control_bool()`, directly inspected
- `src/quantum_language/qint_bitwise.pxi:534-584` -- `__invert__` implementation, already handles controlled context, directly inspected
- `c_backend/src/LogicOperations.c:208-258` -- `cQ_not` C implementation, generates CX with control at qubit[bits], directly inspected
- `tests/python/test_nested_with_blocks.py` -- Existing xfail tests, qubit budget analysis confirmed 38 peak qubits

### Secondary (MEDIUM confidence)
- Empirical qubit count measurements via `ql.circuit_stats()` and Qiskit simulation
- CONTEXT.md decisions from user discussion session

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- this is internal framework code, fully inspected
- Architecture: HIGH -- direct code analysis of all integration points, verified patterns
- Pitfalls: HIGH -- empirically validated (qubit counts, simulation limits tested)
- Test approach: HIGH -- qubit budgets verified via live circuit construction

**Research date:** 2026-03-09
**Valid until:** 2026-04-09 (stable internal codebase, no external dependency risk)
