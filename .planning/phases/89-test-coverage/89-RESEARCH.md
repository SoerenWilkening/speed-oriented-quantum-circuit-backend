# Phase 89: Test Coverage - Research

**Researched:** 2026-02-24
**Domain:** Test infrastructure, xfail conversion, C test integration, coverage measurement
**Confidence:** HIGH

## Summary

Phase 89 closes the v4.1 milestone by ensuring all bugs fixed in Phases 82-88 have regression tests, closing identified coverage gaps, and integrating C backend tests into pytest. The work is primarily test file creation and modification with no production code changes.

The codebase already has solid test infrastructure: `conftest.py` provides `verify_circuit` and `clean_circuit` fixtures, pytest-cov is configured in `pyproject.toml`, and all C tests have a Makefile. The main work items are: (1) creating new test files for nested with-blocks and circuit reset, (2) writing pytest subprocess wrappers for C tests, (3) identifying and converting xfail markers for bugs fixed in Phases 82-88, and (4) measuring coverage improvement.

**Primary recommendation:** Execute in two waves: Wave 1 handles xfail conversion, C test integration, and new test creation in parallel (independent work); Wave 2 measures coverage improvement after Wave 1 tests are in place.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Convert xfails ONLY for bugs actually fixed in Phases 82-88 (optimizer fixes, QFT addition fixes, scope/segfault fixes, etc.)
- Leave known architectural limitations as xfail (QFT division at widths 3+, BK CLA not implemented, BUG-DIV-02, etc.) — these are still broken
- When removing xfail markers, preserve the original bug context as a brief docstring/comment for regression tracking
- Just remove the xfail decorator/call — existing test assertions are sufficient, no need to strengthen them
- If a supposedly-fixed xfail still fails when unmarked: re-mark as xfail with a note flagging it as a regression/incomplete fix; don't block the test suite
- Compile C tests on-the-fly from source within the pytest subprocess wrapper (no dependency on pre-built CMake binaries)
- Wrap ALL C tests in tests/c/ — test_allocator_block, test_reverse_circuit, test_comparison, and hot-path tests
- Show stdout + stderr only on failure; clean output on success
- If gcc/cc is not found, pytest.skip() the C tests with a clear warning message — don't block Python-only developers
- Use pytest-cov for Python coverage measurement
- Python code only (python-backend/) — no C coverage via gcov
- Establish baseline measurement first, then success = any measurable increase after adding new tests
- Local dev command only for now — no CI integration in this phase
- Test 2 levels deep: with-block inside with-block (quantum conditional inside quantum conditional)
- Test arithmetic + assignment operations inside nested conditionals
- Verify both true-path and false-path branches at each nesting level
- Use small qubit widths (2-3 bit QInts) to stay safely under the 17-qubit simulation limit

### Claude's Discretion
- Exact test file organization and naming
- Which specific bugs from Phases 82-88 map to which xfail markers (requires code analysis)
- Circuit reset test design details
- pytest-cov configuration options

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TEST-03 | Add tests for nested with-blocks (quantum conditionals within quantum conditionals) | Pattern analysis of existing `tests/test_conditionals.py` shows single-level with-blocks only; need to add 2-deep nesting. Use `verify_circuit` fixture from `conftest.py`. |
| TEST-04 | Add tests for circuit reset behavior (circuit state after reset_circuit) | `ql.circuit()` is the reset mechanism (used in `conftest.py` line 79). Need to verify no state leakage between circuit builds. |
| TEST-05 | Integrate C backend tests into pytest via subprocess wrapper | 6 C test files in `tests/c/` with `Makefile`. Need subprocess wrappers that compile from source, run, check exit code. |
| TEST-06 | Convert xfail markers to passing tests for all bugs fixed in this milestone | Identified BUG-CQQ-QFT xfails in `test_cross_backend.py` (fixed Phase 86-02). BUG-COND-MUL-01 comments already updated. Need full audit. |
</phase_requirements>

## Architecture Patterns

### Existing Test Infrastructure

The project has well-established test infrastructure:

**Test directories:**
- `tests/` — Root verification tests (uses `conftest.py` with `verify_circuit` fixture)
- `tests/python/` — Unit tests (separate `conftest.py`)
- `tests/bugfix/` — Bug-specific regression tests
- `tests/quick/` — Quick verification tests
- `tests/c/` — C backend tests (compiled from source, standalone executables)

**Key fixtures (tests/conftest.py):**
- `verify_circuit` — Full pipeline: build circuit -> OpenQASM export -> Qiskit simulation -> result extraction
- `clean_circuit` — Simple circuit reset via `ql.circuit()`

**Coverage configuration (pyproject.toml):**
- `pytest-cov>=6.0` in dev dependencies
- `source = ["quantum_language"]` with Cython.Coverage plugin
- HTML reports to `reports/coverage/html`
- Baseline measured at 48.2% in Phase 82-02

### Pattern: Verification Test

```python
class TestSomething:
    def test_case(self, verify_circuit):
        def build():
            a = ql.qint(value, width=W)
            # ... build circuit ...
            return (expected_value, [a, ...keepalive_refs])
        actual, expected = verify_circuit(build, width=W)
        assert actual == expected
```

### Pattern: Direct Simulation (for complex circuits)

Used when `verify_circuit` bitstring extraction is unreliable (e.g., qubit swaps in multiplication):

```python
def test_case(self):
    gc.collect()
    ql.circuit()
    # ... build circuit ...
    result_start = result.allocated_start
    result_width = result.width
    qasm = ql.to_openqasm()
    # ... simulate and extract from specific qubit positions ...
```

### Pattern: Bug Regression Test File

```python
"""Regression tests for BUG-XX: description.
Root cause: ...
Fixed by: ...
"""
class TestBugXX:
    def test_no_crash(self):
        ...
    def test_correctness(self):
        ...
```

## xfail Audit: Bugs Fixed in Phases 82-88

### Bugs Fixed (xfails to convert)

**BUG-CQQ-QFT (Fixed Phase 86-02):** QFT controlled QQ addition CCP rotation errors
- `tests/python/test_cross_backend.py`: ~28 xfail markers on `test_cqq_add` (widths 2-8), `test_cqq_mul` (widths 2-6), `test_ccq_mul` (widths 2-6)
- These are the PRIMARY xfails to convert. BUG-CQQ-QFT was fixed in Phase 86-02 (source qubit mapping in cQQ_add corrected).
- **CAUTION:** Some of these tests also have `pytest.mark.slow` — they may use many qubits. Must verify each test actually passes before removing xfail. Some wider widths may exceed 17-qubit simulation limit and need special handling.

**BUG-COND-MUL-01 (Fixed Phase 87-04):** Controlled multiplication scope corruption
- `tests/test_conditionals.py`: Already updated — docstrings mention "Fixed in Phase 87-04", no xfail markers remain.
- `tests/test_toffoli_multiplication.py`: Comments say "BUG-COND-MUL-01 workaround removed: scope depth bypass now in production __mul__/__rmul__". No xfail markers, but verify workaround comments are updated.
- `tests/python/test_cross_backend.py`: Line 33 — workaround removal comment already present.
- `tests/python/test_bug07_cond_mul.py`: Dedicated regression test already exists.
- **CONCLUSION:** BUG-COND-MUL-01 xfails already converted. No action needed.

**BUG-01 (Fixed Phase 87-01):** 32-bit multiplication segfault (MAXLAYERINSEQUENCE)
- `tests/python/test_bug01_32bit_mul.py`: Dedicated regression test already exists, no xfail markers.
- **CONCLUSION:** Already handled. No action needed.

**BUG-02 (Fixed Phase 87-03):** qarray *= scalar segfault
- `tests/python/test_bug02_qarray_imul.py`: Dedicated regression test already exists, no xfail markers.
- **CONCLUSION:** Already handled. No action needed.

**BUG-WIDTH-ADD (Fixed Phase 86-01):** Mixed-width QFT addition
- No xfail markers found in codebase with "BUG-WIDTH-ADD" text.
- Tests in `test_cross_backend.py` for addition don't have mixed-width xfails.
- **CONCLUSION:** No xfails to convert. The fix was verified by existing tests passing.

### Bugs NOT Fixed (xfails to KEEP)

- **BUG-QFT-DIV / BUG-DIV-02:** QFT division/modulo broken at widths 3+ — deferred (uncomputation architecture redesign needed)
  - `tests/python/test_cross_backend.py`: xfails on `test_div_classical`, `test_mod_classical`, `test_div_quantum`, `test_mod_quantum` at widths 3+ — KEEP
  - `tests/test_div.py`, `tests/test_mod.py`: MSB leak xfails — KEEP
  - `tests/test_toffoli_division.py`: Controlled division xfails — KEEP
  - `tests/test_copy_binops.py`: floordiv BUG-DIV-02 xfails — KEEP
  - `tests/test_modular.py`: Modular arithmetic xfails — KEEP
- **BUG-MOD-REDUCE:** _reduce_mod corruption — explicitly deferred (Beauregard redesign)
  - `tests/test_modular.py`: xfails — KEEP
- **BK CLA not implemented:** `tests/python/test_cla_bk_algorithm.py`, `tests/python/test_cla_verification.py` — KEEP
- **Mixed-width bitwise:** `tests/test_bitwise_mixed.py` — architectural limitation — KEEP
- **Uncomputation limitations:** `tests/test_uncomputation.py` — known limitation — KEEP
- **Compare preservation:** `tests/test_compare_preservation.py` — known limitation — KEEP

## C Test Integration Details

### C Test Files in tests/c/

| File | Dependencies | Executable |
|------|-------------|-----------|
| `test_allocator_block.c` | `qubit_allocator.c` only | Minimal, fast |
| `test_reverse_circuit.c` | `execution.c`, `optimizer.c`, `gate.c`, `qubit_allocator.c`, `circuit_*.c` | Medium |
| `test_comparison.c` | `Integer.c`, `IntegerComparison.c`, `gate.c`, `qubit_allocator.c` | Medium |
| `test_hot_path_add.c` | Addition + execution + optimizer + sequences | Large (many .c files) |
| `test_hot_path_mul.c` | Multiplication + addition + execution + optimizer + sequences | Large |
| `test_hot_path_xor.c` | Logic + execution + optimizer | Medium |

### Subprocess Wrapper Pattern

```python
@pytest.mark.skipif(not _has_compiler(), reason="C compiler (gcc/cc) not found")
def test_c_allocator_block():
    """Run C-level allocator block tests."""
    result = subprocess.run(
        ["make", "-C", C_TEST_DIR, "test_allocator_block"],
        capture_output=True, text=True, timeout=60
    )
    if result.returncode != 0:
        pytest.fail(f"Compilation failed:\n{result.stderr}")

    result = subprocess.run(
        [os.path.join(C_TEST_DIR, "test_allocator_block")],
        capture_output=True, text=True, timeout=30
    )
    assert result.returncode == 0, f"Test failed:\n{result.stdout}\n{result.stderr}"
```

**Key decisions:**
- Use `make` from existing Makefile (leverages existing build rules)
- Compile on-the-fly, no pre-built dependencies
- Capture stdout/stderr, show only on failure
- `pytest.skip()` if no C compiler found
- Individual test functions per C test binary for clear reporting

## Nested With-Block Test Design

Existing tests in `test_conditionals.py` only test single-level with-blocks:
```python
with cond:
    result += 1
```

Phase 89 needs 2-level nesting:
```python
with outer_cond:
    result += 1  # only if outer_cond True
    with inner_cond:
        result += 2  # only if BOTH conditions True
```

### Test Matrix (2-bit QInts for minimal qubits)

| Outer | Inner | Expected result |
|-------|-------|----------------|
| True  | True  | outer_op + inner_op |
| True  | False | outer_op only |
| False | True  | nothing (outer blocks inner) |
| False | False | nothing |

### Qubit Budget

With 2-bit QInts: outer(2) + outer_cond(1) + inner(2) + inner_cond(1) + result(3) + ancilla(~4) = ~13 qubits. Safely under 17-qubit limit.

With 3-bit QInts: outer(3) + outer_cond(1) + inner(3) + inner_cond(1) + result(4) + ancilla(~5) = ~17 qubits. At the limit, may need to stay with 2-bit.

**Recommendation:** Use 2-bit QInts (width=2) for nested tests to stay safely under the simulation limit.

## Circuit Reset Test Design

`ql.circuit()` is the circuit reset function. The `conftest.py` `verify_circuit` fixture calls it internally. Tests need to verify:

1. After `ql.circuit()`, a new circuit is empty (no gates from prior operations)
2. Qint allocations after reset start from qubit 0 (no leaked allocations)
3. Operations after reset produce correct results independent of prior circuit

### Test Approach

```python
class TestCircuitReset:
    def test_reset_no_gate_leakage(self):
        """After reset, new circuit has no gates from prior operations."""
        ql.circuit()
        a = ql.qint(3, width=3)
        b = a + 1  # generates gates

        ql.circuit()  # reset
        c = ql.qint(5, width=3)
        qasm = ql.to_openqasm()
        # qasm should only contain initialization of value 5, not the a+1 gates

    def test_reset_qubit_allocation_fresh(self):
        """After reset, qubit allocation starts fresh."""
        ql.circuit()
        a = ql.qint(0, width=8)  # allocates qubits 0-7

        ql.circuit()  # reset
        b = ql.qint(0, width=3)
        assert b.allocated_start == 0  # should start at 0, not 8

    def test_reset_simulation_independence(self, verify_circuit):
        """Results after reset are independent of prior circuit."""
        # Build a complex circuit first
        ql.circuit()
        x = ql.qint(7, width=3)
        y = x + 1

        # Now reset and build a simple one
        def build():
            a = ql.qint(3, width=3)
            b = a + 1
            return (4, [a, b])
        actual, expected = verify_circuit(build, width=3)
        assert actual == expected  # verify_circuit calls ql.circuit() internally
```

## Coverage Measurement

### Existing Configuration

`pyproject.toml` already has:
- `[tool.coverage.run]` with `source = ["quantum_language"]`
- `[tool.coverage.report]` with `show_missing = true`
- `[tool.coverage.html]` with output to `reports/coverage/html`

### Baseline

Phase 82-02 established baseline: **48.2% coverage**

### Measurement Command

```bash
pytest tests/python/ -v --cov=quantum_language --cov-report=term-missing --cov-report=html
```

### Success Criteria

Any measurable increase over 48.2%. The new tests in this phase (nested with-blocks, circuit reset, xfail conversions becoming passing tests) should increase coverage since they exercise more code paths.

## Common Pitfalls

### Pitfall 1: Qubit Limit Exceeded
**What goes wrong:** Tests with too many qubits (>17) cause Qiskit simulation to fail or hang.
**Why it happens:** Nested with-blocks and controlled operations use ancilla qubits that are easy to undercount.
**How to avoid:** Use width=2 for nested tests. Count total qubits before simulation. Add comments showing qubit budget.

### Pitfall 2: xfail Conversion Breaks CI
**What goes wrong:** Removing xfail from a test that still fails makes the test suite red.
**Why it happens:** Bug may not be fully fixed for all parameter values (e.g., works at width 2 but not width 6).
**How to avoid:** Run each test individually before removing xfail. If it still fails, re-mark with a note. Use `strict=False` during transition.

### Pitfall 3: GC-Induced Circuit Corruption
**What goes wrong:** Previous test's qint destructors inject uncomputation gates into the new circuit.
**Why it happens:** Python GC is non-deterministic; destructors may fire after `ql.circuit()` reset.
**How to avoid:** Always call `gc.collect()` before `ql.circuit()` in tests. The `verify_circuit` fixture already does this.

### Pitfall 4: C Compilation Differences
**What goes wrong:** C tests compile on developer's machine but fail in CI or different platforms.
**Why it happens:** Makefile uses `gcc` but CI may use `cc` or `clang`.
**How to avoid:** Check for any of `gcc`, `cc`, `clang` in `_has_compiler()`. Use `pytest.skip()` gracefully.

## Sources

### Primary (HIGH confidence)
- Codebase analysis: `tests/conftest.py`, `tests/test_conditionals.py`, `tests/python/test_cross_backend.py`
- Codebase analysis: `tests/c/Makefile`, `tests/c/test_allocator_block.c`, `tests/c/test_reverse_circuit.c`
- Codebase analysis: `pyproject.toml` coverage configuration
- Phase summaries: STATE.md accumulated context for Phases 82-88

### Secondary (MEDIUM confidence)
- xfail marker audit via grep across all test files

## Metadata

**Confidence breakdown:**
- xfail audit: HIGH — direct grep of all test files, cross-referenced with bug fix phases
- C test integration: HIGH — Makefile exists with all build rules
- Nested with-blocks: HIGH — existing pattern in test_conditionals.py, just needs nesting
- Coverage measurement: HIGH — infrastructure already in pyproject.toml

**Research date:** 2026-02-24
**Valid until:** 2026-03-24
