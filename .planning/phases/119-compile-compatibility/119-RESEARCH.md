# Phase 119: Compile Compatibility - Research

**Researched:** 2026-03-09
**Domain:** @ql.compile function capture/replay inside nested `with` blocks
**Confidence:** HIGH

## Summary

The `@ql.compile` infrastructure already handles nested `with` blocks correctly through the AND-ancilla indirection established in Phase 117/118. The key mechanism is that `_get_control_bool()` returns the AND-ancilla (not the raw qbool) when in a nested context, and the replay path maps `block.control_virtual_idx` to `_get_control_bool().qubits[63]` -- which is the AND-ancilla qubit. This means the existing architecture "just works" for the replay path (cache hit).

Live investigation confirmed: a compiled function replayed inside a 2-level nested `with` block correctly emits all gates controlled on the AND-ancilla qubit. When c2=False, the AND-ancilla is 0, and the controlled gates have no effect (result=0). When both conditions are True, the AND-ancilla is 1, and the controlled gates execute (result=1). The first-call capture path emits uncontrolled gates as documented -- this is an accepted trade-off that also applies to single-level `with` blocks.

**Primary recommendation:** This phase is tests-only. Write comprehensive tests verifying the replay path works correctly for all scenarios (forward, inverse, adjoint, compiled-calling-compiled). No compile.py code changes are expected.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Tests first, fix if needed -- the existing architecture (AND-ancilla as single combined control qubit via `_get_control_bool()`) suggests it should already work
- Controlled variant derivation adds ONE virtual control qubit, which maps to `_get_control_bool().qubits[63]` during replay -- this IS the AND-ancilla in nested contexts
- Save/restore uses `list(_get_control_stack())` shallow copy -- should preserve full stack
- Qiskit simulation for all tests (proves end-to-end correctness, matches Phase 118 pattern)
- Tests use `qbool(True/False)` for conditions (1 qubit per condition, keeps circuits under 17-qubit limit)
- Tests live in new file: `tests/python/test_compile_nested_with.py`
- Thorough 2-level nested with tests for compiled functions
- One 3-level smoke test to confirm arbitrary depth
- Inverse (`f.inverse(x)`) and adjoint (`f.adjoint(x)`) inside nested with blocks
- Compiled function calling another compiled function inside 2-level nested with (one test)
- Single-level with + compile regression test (ensure no regression from any changes)
- Only compiled functions CALLED inside nested with blocks -- not functions whose body contains nested with
- First call inside nested with emits uncontrolled gates (accepted trade-off, documented behavior) -- test documents this is expected
- Parametric compilation inside nested with: skip testing
- 15 pre-existing test_compile.py failures: out of scope, don't regress them further

### Claude's Discretion
- Exact test scenarios and expected values
- Whether any compile.py code changes are needed (may be tests-only if architecture already works)
- Internal test organization within the new test file

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CTRL-06 | Nested controls work inside `@ql.compile` captured functions | Verified: replay path correctly maps control_virtual_idx to AND-ancilla via `_get_control_bool()`. Stack save/restore preserves full stack. Tests will validate all scenarios. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| quantum_language | project | Quantum DSL framework under development | The framework being tested |
| pytest | 9.0.2 | Test framework | Already used in all project tests |
| qiskit | installed | QASM parsing and circuit loading | Used for simulation verification |
| qiskit-aer | installed | Statevector simulation | Proves end-to-end correctness |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| gc | stdlib | Garbage collection | Call `gc.collect()` before each test to clear stale qint references |
| re | stdlib | Regex for QASM parsing | Extract qubit count from QASM string |
| warnings | stdlib | Warning suppression | Suppress "Value exceeds range" warnings in tests |

## Architecture Patterns

### Recommended Test Structure
```
tests/python/
    test_compile_nested_with.py    # NEW: All Phase 119 tests
    test_nested_with_blocks.py     # Phase 118 (existing, regression baseline)
    test_control_stack.py          # Phase 117 (existing, regression baseline)
```

### Pattern 1: Replay-Path Testing (Cache Pre-Population)
**What:** To test the replay (controlled) path, the compile cache must be populated FIRST on the same circuit (since `ql.circuit()` clears the cache). Call the compiled function once outside any `with` block on a throwaway register, then call again inside nested `with` blocks on the test register.
**When to use:** ALL compile + nested with tests (replay path is the meaningful path to verify).
**Example:**
```python
# Source: Live investigation confirmed this pattern
@ql.compile
def inc(x):
    x += 1
    return x

gc.collect()
ql.circuit()
# Pre-populate cache (throwaway register, same width)
throwaway = ql.qint(0, width=2)
_ = inc(throwaway)

# Now test replay inside nested with
c1 = ql.qbool(True)
c2 = ql.qbool(False)
result = ql.qint(0, width=2)
with c1:
    with c2:
        result = inc(result)  # REPLAY path -- correctly controlled

# Verify via Qiskit simulation
qasm = ql.to_openqasm()
_keepalive = [c1, c2, result, throwaway]
# ... simulate and extract result ...
assert actual == 0  # c2=False, so nothing executes
```

### Pattern 2: Simulation Helper (Reuse from Phase 118)
**What:** `_simulate_and_extract()` function from `test_nested_with_blocks.py` handles QASM loading, simulation, and bit extraction.
**When to use:** Every test that verifies correctness by simulation.
**Example:**
```python
# Source: tests/python/test_nested_with_blocks.py:37
def _simulate_and_extract(qasm_str, num_qubits, result_start, result_width):
    """Simulate QASM and extract integer from result register."""
    circuit = qiskit.qasm3.loads(qasm_str)
    if not circuit.cregs:
        circuit.measure_all()
    simulator = AerSimulator(method="statevector", max_parallel_threads=4)
    job = simulator.run(circuit, shots=1)
    result = job.result()
    counts = result.get_counts()
    bitstring = list(counts.keys())[0]
    msb_pos = num_qubits - result_start - result_width
    lsb_pos = num_qubits - 1 - result_start
    result_bits = bitstring[msb_pos : lsb_pos + 1]
    return int(result_bits, 2)
```

### Pattern 3: First-Call Trade-Off Documentation
**What:** The first call to a compiled function inside a `with` block always emits uncontrolled gates into the live circuit. This is documented behavior, not a bug. Tests should document this.
**When to use:** One dedicated test that explicitly verifies and documents the first-call trade-off.
**Example:**
```python
def test_first_call_emits_uncontrolled(self):
    """First call inside nested with emits uncontrolled gates (known trade-off).

    This is accepted behavior documented in compile.py:1204-1210.
    The first call captures in uncontrolled mode, so gates emitted during
    capture are not controlled. Subsequent calls (replay) are correctly
    controlled.
    """
    # ... test verifying first call gives uncontrolled result ...
```

### Anti-Patterns to Avoid
- **Separate circuit per test call:** Never call `ql.circuit()` between cache-populating call and test call -- it clears the compile cache
- **Using comparisons for conditions:** Use `ql.qbool(True/False)` instead of `a > 3` to minimize qubit count (1 qubit vs 5-6 qubits per condition)
- **Forgetting `_keepalive`:** Always keep references to all qints/qbools alive until after QASM export -- garbage collection can trigger uncomputation that modifies the circuit
- **Testing adjoint without forward call:** `.adjoint()` / `.inverse()` requires a prior `.forward()` call on the SAME qubits in the SAME circuit

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Simulation verification | Custom QASM parsing | `_simulate_and_extract()` from test_nested_with_blocks.py | Proven correct, handles bit extraction edge cases |
| Qubit count extraction | Manual QASM parsing | `_get_num_qubits()` from test_nested_with_blocks.py | Handles all QASM qubit declaration formats |
| Control qubit mapping | Manual virtual-to-real mapping | Existing `_replay()` mechanism | Complex qubit remapping with ancilla allocation |

## Common Pitfalls

### Pitfall 1: Circuit Reset Clears Compile Cache
**What goes wrong:** `ql.circuit()` clears all compile caches. If you call the compiled function in a separate circuit to pre-populate the cache, the cache is empty when you call it in the test circuit.
**Why it happens:** `_register_cache_clear_hook()` connects circuit reset to cache invalidation.
**How to avoid:** Always pre-populate the cache on a throwaway register within the SAME circuit, before the test call inside nested `with` blocks.
**Warning signs:** Getting the first-call (capture) behavior instead of replay behavior.

### Pitfall 2: Qubit Budget Overflow
**What goes wrong:** Tests exceed the 17-qubit simulation limit, causing Qiskit OOM or slow execution.
**Why it happens:** Each condition qbool = 1 qubit, each AND-ancilla = 1 qubit, plus result register, plus throwaway register for pre-caching, plus internal ancillas from compiled function.
**How to avoid:** Use 2-bit result registers and `qbool(True/False)` conditions. Budget: 2-level test = 2 conds + 2 AND-ancilla + 2 result + 2 throwaway + ~4 internal ancillas = ~12 qubits.
**Warning signs:** `num_qubits > 17` assertion failure.

### Pitfall 3: Forgetting _keepalive References
**What goes wrong:** Python garbage collection runs before QASM export, triggering auto-uncomputation of qints/qbools, which adds unwanted gates to the circuit.
**Why it happens:** Python's reference counting GC can collect temporary objects between the `with` block exit and `ql.to_openqasm()` call.
**How to avoid:** Always store all qints/qbools in a `_keepalive` list before calling `ql.to_openqasm()`.
**Warning signs:** Inconsistent test results, extra gates in QASM, bitstring values that don't match expectations.

### Pitfall 4: Adjoint Requires Forward Call on Same Qubits
**What goes wrong:** `ValueError: No prior forward call found for these input qubits`.
**Why it happens:** `.inverse()` and `.adjoint()` look up a forward call record keyed by input qubit indices. The forward call must have been made on the EXACT same qubits in the SAME circuit.
**How to avoid:** Always call `add1(result)` forward first, THEN call `add1.inverse(result)` or `add1.adjoint(result)` on the same `result` qint.
**Warning signs:** ValueError on inverse/adjoint call.

### Pitfall 5: First-Call Trade-Off in Tests
**What goes wrong:** A test verifying controlled behavior fails because it is the first call (capture path), which emits uncontrolled gates.
**Why it happens:** `_capture_and_cache_both()` captures in uncontrolled mode and emits uncontrolled gates into the live circuit on first call (compile.py:1204-1210).
**How to avoid:** Always pre-populate the cache first. Document the first-call trade-off explicitly in one dedicated test.
**Warning signs:** Test fails with the compiled function executing when conditions are False.

## Code Examples

### Verified: Compile Replay Inside 2-Level Nested With (Both True)
```python
# Source: Live investigation on current codebase (2026-03-09)
# Result: PASS -- replay correctly controls on AND-ancilla
@ql.compile
def inc(x):
    x += 1
    return x

gc.collect()
ql.circuit()
throwaway = ql.qint(0, width=2)
_ = inc(throwaway)  # Pre-populate cache

c1 = ql.qbool(True)
c2 = ql.qbool(True)
result = ql.qint(0, width=2)
with c1:
    with c2:
        result = inc(result)  # Replay: controlled on AND-ancilla

# Simulation confirms result = 1 (both True, inc executes)
```

### Verified: Compile Replay Inside 2-Level Nested With (c2=False)
```python
# Source: Live investigation on current codebase (2026-03-09)
# Result: PASS -- replay correctly gates on AND-ancilla, result stays 0
# (same setup as above, but c2 = ql.qbool(False))
# Simulation confirms result = 0 (c2=False, AND-ancilla=0, inc skipped)
```

### Verified: First-Call Trade-Off (Both Single and Nested)
```python
# Source: Live investigation on current codebase (2026-03-09)
# When compile cache is empty and first call is inside `with`:
# - Single-level with, c1=False: result = 1 (uncontrolled, NOT expected 0)
# - 2-level nested with, c2=False: result = 1 (uncontrolled, NOT expected 0)
# This is documented behavior (compile.py:1204-1210), NOT a bug.
```

### Key Compile Infrastructure Entry Points
```python
# Source: compile.py

# Stack save/restore during capture (line 1213-1219):
if is_controlled:
    saved_stack = list(_get_control_stack())  # Shallow copy of full stack
    _set_control_stack([])                     # Clear for uncontrolled capture
    try:
        block = self._capture(args, kwargs, quantum_args)
    finally:
        _set_control_stack(saved_stack)        # Restore full stack

# Control qubit mapping during replay (line 1470-1473):
if block.control_virtual_idx is not None and v == block.control_virtual_idx:
    control_bool = _get_control_bool()          # Returns AND-ancilla in nested context
    virtual_to_real[v] = int(control_bool.qubits[63])

# Inverse control mapping (line 1912-1920):
is_controlled = _get_controlled()
if is_controlled:
    control_bool = _get_control_bool()          # Returns AND-ancilla in nested context
    ctrl_qubit = int(control_bool.qubits[63])
    ctrl_virt_idx = record.block.total_virtual_qubits
    adjoint_gates = _derive_controlled_gates(adjoint_gates, ctrl_virt_idx)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Flat control globals (_controlled, _control_bool) | Stack-based control (`_control_stack`) with push/pop | Phase 117 | Enables multi-level nesting |
| Manual AND-composition by caller | Automatic AND-composition in `__enter__` via `_toffoli_and` | Phase 118 | Inner gates automatically doubly-controlled |
| Compile stack save/restore: set_controlled(False) + set_control_bool(None) | `list(_get_control_stack())` + `_set_control_stack([])` | Phase 117 | Full stack preserved during capture |

## Open Questions

1. **Adjoint inside nested with: correctness**
   - What we know: The inverse path code (compile.py:1912-1920) uses `_get_control_bool()` which should return the AND-ancilla in nested context. Code inspection looks correct.
   - What's unclear: Live testing of `.adjoint()` inside nested with blocks showed unexpected results (result=0 instead of 1), but this appeared to be a pre-existing adjoint behavior issue, NOT related to nested with blocks. The non-nested adjoint also showed the same result.
   - Recommendation: Test adjoint inside nested with, but compare against single-level adjoint baseline first. If single-level adjoint has the same issue, it's out of scope for Phase 119.

2. **Compiled-calling-compiled inside nested with**
   - What we know: The replay path uses `_get_controlled()` and `_get_control_bool()` which return the current stack state. If a compiled function calls another compiled function inside nested with, both should use the same AND-ancilla.
   - What's unclear: Have not tested this path live. The inner compiled function's `_capture_and_cache_both` will save/restore the control stack, but the stack may have additional entries from the outer compiled function's replay.
   - Recommendation: Write one test for this scenario. If it fails, investigate whether the inner function's stack save/restore interferes with the outer context.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | pytest 9.0.2 |
| Config file | pytest.ini |
| Quick run command | `pytest tests/python/test_compile_nested_with.py -x -v` |
| Full suite command | `pytest tests/python/test_compile_nested_with.py tests/python/test_nested_with_blocks.py tests/python/test_control_stack.py -v` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CTRL-06a | Compiled function replayed inside 2-level nested with, both True | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_replay_both_true -x` | Wave 0 |
| CTRL-06b | Compiled function replayed inside 2-level nested with, inner False | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_replay_inner_false -x` | Wave 0 |
| CTRL-06c | Compiled function replayed inside 2-level nested with, outer False | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_replay_outer_false -x` | Wave 0 |
| CTRL-06d | Compiled function replayed inside 2-level nested with, both False | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_replay_both_false -x` | Wave 0 |
| CTRL-06e | First-call trade-off documented | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_first_call_trade_off -x` | Wave 0 |
| CTRL-06f | 3-level nested with smoke test | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_three_level_smoke -x` | Wave 0 |
| CTRL-06g | Inverse inside nested with | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileInverseNestedWith -x` | Wave 0 |
| CTRL-06h | Adjoint inside nested with | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileAdjointNestedWith -x` | Wave 0 |
| CTRL-06i | Compiled-calling-compiled inside nested with | unit | `pytest tests/python/test_compile_nested_with.py::TestCompiledCallingCompiled -x` | Wave 0 |
| CTRL-06j | Single-level compile regression | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileSingleLevelRegression -x` | Wave 0 |

### Sampling Rate
- **Per task commit:** `pytest tests/python/test_compile_nested_with.py -x -v`
- **Per wave merge:** `pytest tests/python/test_compile_nested_with.py tests/python/test_nested_with_blocks.py tests/python/test_control_stack.py -v`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/python/test_compile_nested_with.py` -- covers CTRL-06 (all sub-requirements)
- No framework install needed -- pytest and Qiskit already installed and working

## Sources

### Primary (HIGH confidence)
- `src/quantum_language/compile.py` -- direct code inspection of `_capture_and_cache_both()` (line 1196-1263), `_replay()` (line 1447-1520), `_derive_controlled_block()` (line 1265-1293), `_InverseCompiledFunc.__call__()` (line 1807-1849), inverse control mapping (line 1912-1920)
- `src/quantum_language/_core.pyx` -- `_get_control_bool()` (line 97-102), `_get_control_stack()` / `_set_control_stack()` (line 116-123), `_push_control()` (line 125-136)
- `src/quantum_language/qint.pyx` -- `__enter__()` AND-composition logic (line 818-822), `__exit__()` AND-ancilla uncomputation (line 881-887)
- `tests/python/test_nested_with_blocks.py` -- Phase 118 test patterns, `_simulate_and_extract()` helper
- `tests/python/test_control_stack.py` -- `TestIntegration::test_compile_save_restore_stack` baseline

### Secondary (MEDIUM confidence)
- Live investigation (2026-03-09) -- tested compile replay inside nested with blocks with Qiskit simulation, confirmed correct behavior for replay path

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- code inspection + live verification
- Architecture: HIGH -- live investigation confirmed replay path works; stack save/restore preserves full stack
- Pitfalls: HIGH -- discovered and verified all key pitfalls through live testing (cache clearing, first-call trade-off, qubit budget, keepalive)

**Research date:** 2026-03-09
**Valid until:** 2026-04-09 (stable internal framework, no external dependencies changing)
