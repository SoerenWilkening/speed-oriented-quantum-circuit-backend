# Phase 120: 2D Qarray Support - Research

**Researched:** 2026-03-09
**Domain:** Python data structure (Cython extension type) -- 2D quantum array indexing and construction
**Confidence:** HIGH

## Summary

The existing `qarray` implementation in `src/quantum_language/qarray.pyx` already has comprehensive 2D support. Construction via `dim=(rows, cols)` and nested lists works. Per-element indexing `arr[r, c]` for both read and write (including augmented assignment like `arr[r, c] += 1`) works correctly with proper view semantics (same qubit objects returned, quantum identity preserved). Row and column slicing (`arr[r, :]`, `arr[:, c]`) also work and return views sharing the underlying qubit objects.

**One real bug was identified:** `__setitem__` with a plain `int` key on a multi-dimensional array raises `NotImplementedError("Row assignment for multi-dimensional arrays not yet supported")`. This breaks the pattern `board[0] += 1` on 2D arrays (row-level augmented assignment), because Python's augmented assignment protocol calls `__getitem__(0)` (returns row view), `__iadd__(1)` (modifies view in-place, which works because elements are shared), then `__setitem__(0, modified_view)` (raises). The in-place modification already happened via shared references, but the write-back step fails.

Beyond the bug fix, this phase requires: (a) updating CLAUDE.md to show `ql.qarray()` as the canonical constructor instead of `ql.array()`, (b) adding 2D-specific tests for the `dim=` construction pattern with `dtype=ql.qbool`, and (c) optionally improving error messages when quantum indices (qint) are used.

**Primary recommendation:** Fix the `__setitem__` int-key-on-2D-array case, add 2D qarray tests validating the full chess engine usage pattern, and update CLAUDE.md.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- `ql.qarray()` is the canonical constructor (matches `ql.qint()` / `ql.qbool()` naming pattern)
- `ql.array()` remains as an undocumented alias for backward compatibility -- no deprecation warning, just not the documented API
- `ql.qarray()` accepts data as positional argument: `ql.qarray([1, 2, 3])` and `ql.qarray(dim=(8, 8))`
- Default dtype is `qint` when not specified (consistent with current behavior)
- `arr[r, c]` returns the actual stored qbool/qint object (view semantics, preserves quantum identity)
- `arr[r, c] |= flag` works in-place: `__getitem__` returns stored element, `__ior__` emits quantum gate, `__setitem__` writes back same object
- Direct assignment `arr[r, c] = new_qbool` also supported (replaces stored element)
- Row/column slicing (`board[r, :]`, `board[:, c]`) returns view arrays sharing the same qubit objects
- Classical `int` indexing only -- no quantum indexing with `qint` (raises TypeError)
- Fix bugs only -- no convenience methods (no `.rows()`, `.cols()`, `.reshape()`, `.flatten()`)
- Both nested list construction (`ql.qarray([[1,2],[3,4]])`) and dimension-based (`ql.qarray(dim=(2,2))`) must work
- Tests use small arrays (2x2, 3x3) to stay within 17-qubit simulation limit
- No 8x8 simulation tests -- chess engine is circuit-build-only
- Update CLAUDE.md examples to use `ql.qarray()` instead of `ql.array()`

### Claude's Discretion
- Specific bug identification and fix approach (researcher/planner determines what's actually broken)
- Test organization and naming
- Error message wording for invalid 2D operations

### Deferred Ideas (OUT OF SCOPE)
- Quantum indexing with qint indices (QRAM-select) -- major feature, separate phase
- Convenience iteration methods (.rows(), .cols()) -- add if needed later
- Sparse 2D qarray -- explicitly out of scope per REQUIREMENTS.md
- Reshape/flatten methods -- add if needed later
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ARR-01 | User can create 2D qarrays via `ql.qarray(dim=(rows, cols), dtype=ql.qbool)` | Already works. Verified: `ql.qarray(dim=(2,2), dtype=ql.qbool)` creates a 4-element array with shape `(2,2)`. Class is already exported via `__init__.py` as `ql.qarray`. No code changes needed for construction itself. |
| ARR-02 | User can index 2D qarrays with `arr[r, c]` for read and in-place mutation | Read and per-element mutation work. One bug: `__setitem__` with plain int key on 2D array raises `NotImplementedError`, breaking row-level augmented assignment (`board[0] += 1`). Fix identified. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| quantum_language (qarray.pyx) | internal | Cython extension type for quantum arrays | This is the project's own code; all changes are internal |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| pytest | 9.0.2 | Test framework | Already used for all qarray tests |

### Alternatives Considered
None -- this is internal framework code with no external dependencies to choose between.

**Installation:**
No new dependencies needed. Rebuild with `pip install -e .` after Cython changes.

## Architecture Patterns

### Relevant File Structure
```
src/quantum_language/
  qarray.pyx          # Main class (Cython) - THE file to modify
  _qarray_utils.py    # Pure Python helpers (_detect_shape, _flatten) - no changes needed
  __init__.py          # Exports ql.qarray (class) and ql.array (wrapper function) - CLAUDE.md update
tests/
  test_qarray.py               # Construction, indexing, repr, view semantics (23 tests)
  test_qarray_mutability.py    # Augmented assignment 1D and 2D (34 tests)
  test_qarray_elementwise.py   # Element-wise operations (94 tests)
  test_qarray_reductions.py    # .all(), .any(), .sum(), .parity()
  test_array_verify.py         # Pipeline verification tests
CLAUDE.md                      # User-facing docs (needs ql.array -> ql.qarray update)
```

### Pattern: Augmented Assignment Flow on 2D Element
**What:** How `board[r, c] += 1` works on a 2D qarray
**When to use:** This is the primary chess engine pattern
**Example:**
```python
# Python desugars board[0, 1] += 1 to:
temp = board.__getitem__((0, 1))   # Returns the actual qint/qbool object
temp = temp.__iadd__(1)            # Modifies in-place, emits quantum gates
board.__setitem__((0, 1), temp)    # Writes same object back at flat_idx

# Both __getitem__ and __setitem__ go through _multi_to_flat()
# to convert (row, col) -> flat index into self._elements list
```

### Pattern: Row-Level Augmented Assignment (THE BUG)
**What:** How `board[0] += 1` fails on a 2D qarray
**When to use:** Row-level operations on 2D arrays
**The bug:**
```python
# Python desugars board[0] += 1 to:
temp = board.__getitem__(0)         # Returns qarray VIEW of row 0 (shared elements)
temp = temp.__iadd__(1)             # Modifies view in-place (works! elements are shared)
board.__setitem__(0, temp)          # RAISES NotImplementedError
# The in-place modification already happened, but the write-back fails
```
**Fix:** In `__setitem__`, when key is `int` and `len(self._shape) > 1`, instead of raising `NotImplementedError`, support row assignment by writing the view's elements back to the corresponding flat indices. The simplest fix: if the value being set is a qarray and its elements are already the same objects (identity check), make it a no-op (the in-place modification already worked via shared refs). Otherwise, copy elements from the value qarray into the correct row slice.

### Anti-Patterns to Avoid
- **Copying elements on read:** `__getitem__` must return the actual stored object, not a copy. Copying would break quantum identity (the qbool at `board[r, c]` must be the same qubit object used in `with` blocks).
- **Re-creating qints on write-back:** `__setitem__` must store the exact same Python object. Creating new qint/qbool objects would allocate new qubits.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Multi-dimensional index conversion | Custom stride calculation | Existing `_multi_to_flat()` method | Already correct, handles negative indices and bounds checking |
| Nested list shape detection | Custom recursion | Existing `_detect_shape()` in `_qarray_utils.py` | Already handles jagged array detection with clear error messages |
| View creation | Manual qarray construction | Existing `_create_view()` static method | Correctly shares element references without copying |

## Common Pitfalls

### Pitfall 1: Breaking Quantum Identity
**What goes wrong:** If `__getitem__` or `__setitem__` creates new qint/qbool objects instead of returning/storing the originals, the quantum state gets duplicated to new qubits.
**Why it happens:** Temptation to normalize or validate elements during get/set.
**How to avoid:** Always return `self._elements[flat_idx]` directly. Never wrap in a new constructor.
**Warning signs:** Tests using `is` identity checks fail; qubit count increases unexpectedly.

### Pitfall 2: __setitem__ Row Assignment Semantics
**What goes wrong:** The current code raises `NotImplementedError` for `__setitem__(int_key, value)` on 2D arrays. This blocks augmented assignment patterns like `board[0] += 1`.
**Why it happens:** The original implementation didn't consider that Python's augmented assignment calls `__setitem__` after `__iadd__` even for in-place operations.
**How to avoid:** Handle the row assignment case: compute the row's flat index range, write each element from the value (qarray) into the corresponding position.
**Warning signs:** `NotImplementedError` when using `board[row_idx] += x` on 2D arrays.

### Pitfall 3: 17-Qubit Simulation Limit
**What goes wrong:** Tests with large 2D arrays (e.g., 8x8 qbool = 64 qubits) exceed Qiskit simulation limit.
**Why it happens:** Each qbool is 1 qubit, each qint is N qubits.
**How to avoid:** Use 2x2 or 3x3 arrays for simulation tests. Use circuit-build-only tests (no simulation) for larger arrays.
**Warning signs:** `AerSimulator` crashes or hangs; memory exhaustion.

### Pitfall 4: Cython Rebuild Required
**What goes wrong:** Changes to `qarray.pyx` don't take effect until the Cython extension is recompiled.
**Why it happens:** `.pyx` files are compiled to C then to shared objects.
**How to avoid:** Run `pip install -e .` after any `.pyx` file changes.
**Warning signs:** Old behavior persists despite code changes.

## Code Examples

### Example 1: 2D qbool Array Construction and Indexing (Verified Working)
```python
# Source: Manual testing against current codebase
import quantum_language as ql

ql.circuit()
board = ql.qarray(dim=(2, 2), dtype=ql.qbool)
# board.shape == (2, 2)
# len(board) == 4
# board.dtype == qbool

# Per-element access (returns the actual qbool object)
elem = board[0, 0]  # type: qbool, same object stored in _elements

# Per-element augmented assignment (works, preserves identity)
board[0, 1] += 1    # Sets qbool to True
board[1, 0] |= ql.qbool(True)  # OR-assigns

# View semantics verified
assert board[0, 0] is board[0, 0]  # Same Python object each time
```

### Example 2: Row/Column Slicing (Verified Working)
```python
ql.circuit()
board = ql.qarray([[1,2,3],[4,5,6]], width=8)

row = board[0, :]  # Returns qarray view of row 0, len=3
col = board[:, 1]  # Returns qarray view of column 1, len=2

# View shares underlying qubit objects
assert row[0] is board[0, 0]  # Same qint object
assert col[1] is board[1, 1]  # Same qint object
```

### Example 3: The Bug Fix Target -- __setitem__ for int key on 2D
```python
# CURRENT (broken):
# qarray.__setitem__ line 254:
#   if isinstance(key, int):
#       if len(self._shape) > 1:
#           raise NotImplementedError("Row assignment...")

# FIX: Replace the NotImplementedError with row assignment logic:
#   if isinstance(key, int):
#       if len(self._shape) > 1:
#           # Convert to row slice: arr[0] -> elements[0:cols]
#           row_idx = key
#           if row_idx < 0:
#               row_idx += self._shape[0]
#           if not 0 <= row_idx < self._shape[0]:
#               raise IndexError(...)
#           cols = self._shape[1]
#           start = row_idx * cols
#           if isinstance(value, qarray):
#               if len(value) != cols:
#                   raise ValueError(...)
#               for i in range(cols):
#                   self._elements[start + i] = value._elements[i]
#           else:
#               # Broadcast scalar
#               for i in range(cols):
#                   self._elements[start + i] = value
```

### Example 4: Chess Engine Usage Pattern (Target API)
```python
import quantum_language as ql

ql.circuit()
board = ql.qarray(dim=(8, 8), dtype=ql.qbool)
board[1, 1] += 1  # Place piece at (1,1)

# Conditional on board position (with block)
with board[1, 1]:
    board[1, 1] -= 1  # Remove piece
    board[2, 3] += 1  # Move to (2,3)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `ql.array()` function wrapper | `ql.qarray()` class constructor | Phase 120 (now) | Consistent naming with `ql.qint()`, `ql.qbool()` |
| 2D `__setitem__` raises NotImplementedError for int key | Support row assignment | Phase 120 (now) | Enables `board[row] += 1` pattern |

## Open Questions

1. **Row-level augmented assignment: should it also work for qint 2D arrays?**
   - What we know: The chess engine only uses per-element `board[r, c] += 1`, not row-level `board[r] += 1`. The row-level case is triggered when someone does `board[0] += scalar` on a 2D array.
   - What's unclear: Whether anyone actually needs row-level augmented assignment vs just per-element.
   - Recommendation: Fix it anyway since it's a small change and prevents confusing errors. The `__setitem__` row support benefits any future row-level operations.

2. **Should qint indexing error messages be improved?**
   - What we know: `board[qint_var, 0]` currently raises `NotImplementedError("Complex slicing pattern not yet supported")` which is misleading. Should say "Quantum indexing not supported; use classical int indices."
   - What's unclear: Whether this is in scope for "fix bugs only."
   - Recommendation: Include it as a small error message improvement since the CONTEXT.md gives Claude discretion on error message wording.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | pytest 9.0.2 |
| Config file | pytest.ini |
| Quick run command | `python3 -m pytest tests/test_qarray.py tests/test_qarray_mutability.py -x -v` |
| Full suite command | `python3 -m pytest tests/test_qarray.py tests/test_qarray_mutability.py tests/test_qarray_elementwise.py tests/test_qarray_reductions.py -v` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ARR-01 | `ql.qarray(dim=(r,c), dtype=ql.qbool)` creates 2D array | unit | `pytest tests/test_qarray.py::TestQarrayConstruction::test_create_from_dimensions -x` | Partial (tests `ql.array(dim=)` with qint, needs qbool variant) |
| ARR-01 | `ql.qarray([[1,2],[3,4]])` creates 2D array | unit | `pytest tests/test_qarray.py::TestQarrayConstruction::test_multidimensional -x` | Yes |
| ARR-02 | `arr[r, c]` reads correct element | unit | `pytest tests/test_qarray.py::TestQarrayPythonIntegration::test_multidimensional_index -x` | Yes |
| ARR-02 | `arr[r, c] += 1` in-place mutation | unit | `pytest tests/test_qarray_mutability.py::TestMultiDimensionalAugmentedAssignment -x` | Yes (6 tests) |
| ARR-02 | `arr[r, c] \|= flag` in-place OR on qbool | unit | New test needed | No -- Wave 0 |
| ARR-02 | `arr[row]` on 2D returns row view | unit | `pytest tests/test_qarray.py::TestQarrayPythonIntegration::test_row_access -x` | Yes |
| ARR-02 | `arr[row] += x` row augmented assignment on 2D | unit | New test needed | No -- Wave 0 (blocked by bug) |
| ARR-02 | View identity preserved (`arr[r,c] is arr[r,c]`) | unit | New test needed for 2D dim-constructed arrays | No -- Wave 0 |
| ARR-02 | Quantum (qint) indexing raises TypeError | unit | New test needed | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `python3 -m pytest tests/test_qarray.py tests/test_qarray_mutability.py -x -v`
- **Per wave merge:** `python3 -m pytest tests/test_qarray.py tests/test_qarray_mutability.py tests/test_qarray_elementwise.py tests/test_qarray_reductions.py -v`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_qarray_2d.py` -- new test file for 2D-specific tests covering ARR-01 (dim-based qbool construction) and ARR-02 (qbool |=, row augmented assignment, view identity on dim-constructed arrays, qint index rejection)
- [ ] No framework install needed -- pytest already configured

### Existing Test Baseline
- `test_qarray.py`: 23 tests -- all pass
- `test_qarray_mutability.py`: 34 tests -- all pass
- `test_qarray_elementwise.py`: 93 tests + 1 skipped -- all pass
- `test_qarray_reductions.py`: (included in elementwise count)
- **Total: 151 passed, 1 skipped, 0 failed**

## Bug Analysis

### BUG: `__setitem__` NotImplementedError for int key on multi-dim arrays

**Location:** `src/quantum_language/qarray.pyx`, `__setitem__` method, lines 253-255

**Current code:**
```python
if isinstance(key, int):
    if len(self._shape) > 1:
        raise NotImplementedError("Row assignment for multi-dimensional arrays not yet supported")
```

**Impact:** Blocks `board[row_idx] += scalar` on 2D arrays. The augmented assignment protocol in Python calls `__getitem__(row_idx)` -> `__iadd__(scalar)` -> `__setitem__(row_idx, modified_view)`. The first two steps work (the view shares elements, so in-place modification works), but the final write-back step raises.

**Fix approach:** Replace the `NotImplementedError` with proper row assignment logic:
1. Normalize negative index
2. Bounds check against `self._shape[0]`
3. If value is a qarray, copy its elements to the row's flat index range
4. If value is a scalar, broadcast to all elements in the row

**Complexity:** Low -- straightforward index arithmetic, follows existing patterns in `__setitem__` for slices.

**Note:** For 2D arrays only (not general N-D). This is sufficient for the chess engine use case.

## Sources

### Primary (HIGH confidence)
- `src/quantum_language/qarray.pyx` -- direct code inspection of all methods
- `src/quantum_language/_qarray_utils.py` -- helper functions inspection
- `src/quantum_language/__init__.py` -- export analysis
- Manual testing against running codebase -- all 8 test scenarios verified
- Existing test suite -- 151 tests passing baseline confirmed

### Secondary (MEDIUM confidence)
- `examples/chess_engine.py` -- chess engine usage patterns (Phase 121 target consumer)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- internal code, no external dependencies
- Architecture: HIGH -- direct code inspection and runtime testing
- Pitfalls: HIGH -- bugs identified through actual execution, not speculation
- Bug analysis: HIGH -- root cause confirmed through Python augmented assignment protocol analysis

**Research date:** 2026-03-09
**Valid until:** 2026-04-09 (stable internal code, no external version drift)
