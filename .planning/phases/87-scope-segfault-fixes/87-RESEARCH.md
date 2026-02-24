# Phase 87: Scope & Segfault Fixes - Research

**Researched:** 2026-02-24
**Domain:** C backend buffer overflow, Cython scope/uncomputation, quantum array operations
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **BUG-MOD-REDUCE (BUG-09):** Defer with documented rationale -- do NOT attempt a fix. Document why modular reduction needs a Beauregard-style algorithm redesign. Update REQUIREMENTS.md to mark BUG-09 as explicitly deferred. No specific future phase assignment.
- **BUG-01:** Static increase of MAXLAYERINSEQUENCE -- do not implement dynamic calculation. Set limit high enough to accommodate 64-bit operations as headroom. Separate plan from BUG-02.
- **BUG-02:** Investigate from scratch -- Claude debugs, identifies root cause, fixes. Separate plan from BUG-01. Test all widths 1-8.
- **BUG-07:** Targeted patch -- fix the specific case where controlled multiplication results get corrupted by scope uncomputation. Keep pushing even if root cause is deeply entangled with uncomputation architecture -- find a workaround rather than deferring. Investigate corruption mechanism from scratch.
- **Testing:** BUG-01 is circuit generation only (32-bit exceeds 17-qubit sim limit). BUG-02 tests all widths 1-8, simulate where feasible within 17-qubit limit. BUG-07 uses Qiskit simulation for correctness. Add regression tests for each fix. Per-bug verification, no full regression suite.
- Each bug gets its own separate plan with atomic commits.

### Claude's Discretion
- BUG-07 test case selection (representative controlled multiplication scenarios)
- Exact MAXLAYERINSEQUENCE value (as long as it supports 64-bit)
- BUG-02 root cause investigation approach
- BUG-09 deferral rationale wording (based on Phase 86 and prior findings)

### Deferred Ideas (OUT OF SCOPE)
- BUG-MOD-REDUCE (BUG-09): Needs Beauregard-style algorithm redesign -- documented and deferred from this phase
- BUG-06/BUG-08 (division ancilla leak, QFT division failures): Already deferred from Phase 86, require uncomputation architecture redesign
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BUG-01 | Fix 32-bit multiplication segfault (buffer overflow in C backend) | MAXLAYERINSEQUENCE=10000 is insufficient; QQ_mul(32) needs 36,494 layers, QQ_mul(64) needs 276,766. Increase to 300,000. Single #define change in types.h. |
| BUG-02 | Fix qarray `*=` in-place multiplication segfault | Tests currently skipped assuming "inherits C backend segfault". Likely separate issue since 8-bit QQ_mul only needs 770 layers (within 10,000 limit). Root cause must be investigated from scratch -- could be qint.__imul__ qubit-swap pattern, GC interaction, or width inference issue. |
| BUG-07 | Fix controlled multiplication scope uncomputation corruption (BUG-COND-MUL-01) | Root cause identified in Phase 69-02: __mul__ creates result qint inside `with` scope, __exit__ uncomputes it. Workaround exists (scope_depth=0). Proper fix: prevent result registration in scope stack OR mark result as non-uncomputable. |
| BUG-09 | Fix _reduce_mod result corruption (BUG-MOD-REDUCE) or explicitly defer with documented rationale | Decision: DEFER. Write rationale referencing Phase 38 research (comparison ancilla entanglement accumulation), Phase 86-03 (uncomputation architecture limitation), and Beauregard algorithm requirement. Update REQUIREMENTS.md. |
</phase_requirements>

## Summary

Phase 87 addresses three active bug fixes and one explicit deferral. The bugs span two distinct categories: (1) buffer overflow crashes (BUG-01, BUG-02) and (2) scope/lifecycle correctness (BUG-07). BUG-09 is deferred with documentation.

**BUG-01** is the simplest fix: `MAXLAYERINSEQUENCE` in `c_backend/include/types.h` is set to 10,000 but QQ_mul at 32-bit requires 36,494 layers. At 64-bit it requires 276,766. The fix is a single constant change to 300,000. The memory impact at high bit widths is significant (the pre-allocated sequence for QQ_mul(64) would use ~7 GB), but this is acceptable because 64-bit quantum multiplication is inherently a massive operation and the allocation only occurs when explicitly requested.

**BUG-02** requires fresh investigation. The qarray multiplication tests are skipped with "inherits C backend segfault issue," but 8-bit QQ_mul only needs 770 layers -- well within the current MAXLAYERINSEQUENCE limit. The bug is likely in the `qint.__imul__` qubit-swap pattern (swapping qubit arrays between original and result qint objects), GC-triggered uncomputation interacting with iteration, or a Cython/C boundary issue specific to the qarray iteration context.

**BUG-07** has a known root cause from Phase 69-02: `__mul__` creates a new result qint that gets registered in the scope frame. When `__exit__` runs, it uncomputes all scope-registered qints including the multiplication result. The workaround (setting `current_scope_depth` to 0) proves the C backend is correct. The proper fix must prevent the result from being uncomputed while still allowing scope cleanup of genuine intermediates.

**Primary recommendation:** Fix BUG-01 first (trivial constant change, unblocks 32-bit multiplication). Fix BUG-02 second (investigate and fix, potentially unblocks qarray multiplication tests). Fix BUG-07 third (scope fix, most architecturally delicate). Document BUG-09 deferral last or in parallel.

## Standard Stack

### Core
| Component | Location | Purpose | Relevance |
|-----------|----------|---------|-----------|
| `types.h` | `c_backend/include/types.h:50` | MAXLAYERINSEQUENCE constant | BUG-01 fix target |
| `IntegerMultiplication.c` | `c_backend/src/` | QQ_mul, CQ_mul, cQQ_mul, cCQ_mul sequence generators | Uses MAXLAYERINSEQUENCE for allocation |
| `qint_arithmetic.pxi` | `src/quantum_language/` | `__mul__`, `__imul__`, `multiplication_inplace` | BUG-02 and BUG-07 fix targets |
| `qint.pyx` | `src/quantum_language/` | `__init__` (scope registration), `__enter__`/`__exit__` (scope cleanup), `_do_uncompute` | BUG-07 fix target |
| `_core.pyx` | `src/quantum_language/` | `current_scope_depth`, `_scope_stack` | BUG-07 scope infrastructure |
| `qarray.pyx` | `src/quantum_language/` | `_inplace_binary_op`, `__imul__` | BUG-02 entry point |
| `hot_path_mul.c` | `c_backend/src/` | C hot path for multiplication dispatch | BUG-02 potential involvement |
| `qint_mod.pyx` | `src/quantum_language/` | `_reduce_mod` method | BUG-09 deferral documentation |

### Supporting
| Component | Location | Purpose | When to Use |
|-----------|----------|---------|-------------|
| `test_conditionals.py` | `tests/` | BUG-COND-MUL-01 xfail tests (lines 280, 298) | Remove xfail after BUG-07 fix |
| `test_qarray_elementwise.py` | `tests/` | Skipped qarray multiplication tests | Remove skip after BUG-02 fix |
| `test_qarray_mutability.py` | `tests/` | Skipped qarray imul test (line 115) | Remove skip after BUG-02 fix |
| `test_toffoli_multiplication.py` | `tests/` | BUG-COND-MUL-01 workaround tests | Reference for BUG-07 fix verification pattern |
| `test_cross_backend.py` | `tests/python/` | BUG-COND-MUL-01 workaround in both backends | Remove workaround after BUG-07 fix |
| `test_mul.py` | `tests/` | QFT multiplication verification (widths 1-5) | Regression verification |
| `test_mul_addsub.py` | `tests/` | Controlled QQ multiplication test | Regression verification |

## Architecture Patterns

### BUG-01: Buffer Overflow Fix Pattern

**What:** Single constant increase in `c_backend/include/types.h`.

**Current state:**
```c
#define MAXLAYERINSEQUENCE 10000
```

**Layer requirements by width (QQ_mul, exact calculation):**

| Width | Layers Needed | Within 10,000? | Within 300,000? |
|-------|--------------|----------------|-----------------|
| 8 | 770 | Yes | Yes |
| 16 | 5,062 | Yes | Yes |
| 32 | 36,494 | **NO** (segfault) | Yes |
| 64 | 276,766 | **NO** | Yes |

**Fix:**
```c
#define MAXLAYERINSEQUENCE 300000
```

The value 300,000 provides headroom for 64-bit QQ_mul (276,766 layers) with ~8% margin. All other multiplication variants (CQ_mul, cQQ_mul, cCQ_mul) require fewer layers than QQ_mul at the same width.

**Memory impact:** Pre-allocation is `MAXLAYERINSEQUENCE * sizeof(gate_t*) + MAXLAYERINSEQUENCE * 10*bits * sizeof(gate_t)`. At 64-bit this is ~7 GB. At 32-bit it is ~446 MB. At 8-bit it is only ~2.3 MB. The memory is only allocated when the specific width is first used (cached for reuse). Users requesting 64-bit quantum multiplication are implicitly accepting large resource requirements.

**Verification:** Generate a 32-bit QQ_mul circuit (no simulation needed since 32*3=96 qubits far exceeds 17-qubit limit). Verify it completes without segfault and produces a non-NULL sequence.

### BUG-02: qarray *= Scalar Investigation Pattern

**What:** Debug segfault in `qarray *= int` for arrays of qint at widths 1-8.

**Entry point:** `qarray.__imul__` (line 854 of `qarray.pyx`) calls `_inplace_binary_op(other, "__imul__")` which iterates elements calling `qint.__imul__`.

**`qint.__imul__` flow (lines 695-728 of `qint_arithmetic.pxi`):**
1. Calls `self * other` (creates new result qint via `__mul__`)
2. Swaps qubit arrays: `self.qubits, result.qubits = result.qubits, self.qubits`
3. Swaps `allocated_start`
4. Copies `result.bits` to `self.bits`
5. Returns `self`

**Potential root causes (in order of likelihood):**
1. **GC interaction during iteration:** When `result = self * other` creates intermediate qints, GC may trigger uncomputation of stale qints from previous iterations, corrupting circuit state. The `gc.collect()` before `ql.circuit()` in test fixtures may not cover this case.
2. **Qubit swap side effects:** After swap, `result_qint` holds the old qubit references. When `result_qint` goes out of scope and gets GC'd, `_do_uncompute` fires on it -- potentially reversing gates for qubits that `self` now owns.
3. **`allocated_qubits` flag not swapped:** `__imul__` swaps `qubits` and `allocated_start` but does NOT swap `allocated_qubits`. If `result_qint` has `allocated_qubits=True` and gets GC'd, it will try to free qubits that `self` is still using.
4. **Width change breaking array consistency:** `self.bits = result_qint.bits` could change the width of one element while the qarray still tracks the original width.

**Investigation approach:** Enable the currently-skipped tests one at a time. Start with single-element qarray at width 1 to minimize variables. Add defensive checks in `__imul__` for qubit ownership transfer. Check if the issue is specific to qarray iteration (loop over elements) or also affects single `qint.__imul__`.

### BUG-07: Scope Uncomputation Fix Pattern

**What:** Prevent `__exit__` from uncomputing out-of-place multiplication results inside `with` blocks.

**Root cause chain (confirmed in Phase 69-02):**
1. User writes: `with cond: result *= 2`
2. `__imul__` calls `self * 2` which calls `__mul__`
3. `__mul__` creates `result = qint(width=result_width)` -- this calls `qint.__init__`
4. `qint.__init__` (line 314): `self.creation_scope = current_scope_depth.get()`
5. `qint.__init__` (line 324): registers `self` in `_scope_stack[-1]` since scope depth > 0
6. `__mul__` sets `result.operation_type = 'MUL'`
7. When `with cond:` exits, `__exit__` pops scope frame and calls `_do_uncompute` on each registered qint
8. The multiplication result has `operation_type='MUL'` (not None), so cascade uncomputation proceeds
9. `_do_uncompute` calls `reverse_circuit_range(_start_layer, _end_layer)` -- reversing all multiplication gates
10. Result register is now in original state (pre-multiplication), producing incorrect output

**Known workaround (from tests):**
```python
saved = current_scope_depth.get()
current_scope_depth.set(0)    # Prevent scope registration
result = a * b                 # Result NOT registered in scope
current_scope_depth.set(saved) # Restore
```

**Fix approaches (ordered by risk):**

**Approach A: Mark imul result as scope-exempt (RECOMMENDED)**
In `qint.__imul__`, after the qubit swap, the `result_qint` (which holds the OLD qubits and will be GC'd) should be marked to prevent uncomputation of the NEW result:
- Set `result_qint.operation_type = None` before returning -- this prevents cascade uncomputation
- Or: Set `result_qint._is_uncomputed = True` to skip all cleanup

This is minimal blast radius because it only affects the discarded intermediate in `__imul__`.

However, this only fixes `result *= scalar` (in-place). Out-of-place `result = a * b` inside a `with` block still has the issue because the result qint IS registered in the scope.

**Approach B: Skip scope registration for operation results (TARGETED)**
In `qint.__init__`, skip scope registration when the caller is `__mul__` or when `operation_type` will be set. Since `operation_type` is set AFTER `__init__`, this requires a different mechanism:
- Add a flag parameter to `qint.__init__`: `scope_exempt=False`
- `__mul__` passes `scope_exempt=True` when creating the result qint
- `__init__` skips scope registration when `scope_exempt=True`

This is cleaner but requires modifying `__init__` signature.

**Approach C: Don't uncompute results with dependencies on scope-external variables (SAFEST but complex)**
In `__exit__`, when iterating scope_qbools for uncomputation, skip qints whose dependency parents include variables created outside the current scope. This is the most correct approach but adds complexity to the uncomputation logic.

**Approach D: Temporarily disable scope registration around __mul__ (SIMPLEST)**
Apply the test workaround at the source: in `__mul__` and `__rmul__`, save `current_scope_depth`, set it to 0, create the result, restore depth. The result will have `creation_scope=0` and will not be registered in any scope frame. This is essentially the test workaround formalized into the implementation.

**Recommendation:** Approach D is the simplest and has proven correct in tests. It matches the "targeted patch, minimal blast radius" directive. The key insight: multiplication results should NEVER be auto-uncomputed by scope exit because the user explicitly assigns them to a variable. The old qubits from `__imul__`'s swap should be handled by the GC path, not the scope path.

### Anti-Patterns to Avoid

- **Changing `__exit__` uncomputation logic broadly:** The uncomputation system is already fragile (see Phase 86-03 findings). Broad changes risk breaking subtraction, comparison, and other operations that correctly use scope cleanup.
- **Dynamic MAXLAYERINSEQUENCE calculation in sequence generators:** The user explicitly locked this as "static increase only." Do not compute the value at runtime.
- **Testing 32-bit multiplication via simulation:** 32-bit multiplication uses 96+ qubits, far exceeding the 17-qubit simulation limit. Verify circuit generation only.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Layer count formula | Manual formula for each multiplication variant | Static increase to 300,000 | User decided static; formula maintenance is error-prone across 4 variants |
| Scope bypass mechanism | New scope registration API | `current_scope_depth.set(0)` workaround | Already proven in tests; minimal code change |
| Modular reduction fix | New `_reduce_mod` algorithm | Documented deferral | User decided to defer; Beauregard redesign is out of scope |

## Common Pitfalls

### Pitfall 1: MAXLAYERINSEQUENCE Memory Explosion
**What goes wrong:** Setting MAXLAYERINSEQUENCE too high wastes memory for small widths since ALL layers are pre-allocated regardless of actual usage.
**Why it happens:** `calloc(MAXLAYERINSEQUENCE, sizeof(gate_t*))` plus per-layer allocations happen upfront for cached sequences.
**How to avoid:** 300,000 is the sweet spot -- covers 64-bit (276,766 actual) without excessive overhead at common widths (8-bit only uses 770 of 300,000 layers). The per-layer allocation is `10 * bits * sizeof(gate_t)` so small widths still have small per-layer cost.
**Warning signs:** Out-of-memory errors when creating multiple cached sequences at large widths.

### Pitfall 2: __imul__ Qubit Ownership Double-Free
**What goes wrong:** After `__imul__` swaps qubit arrays, the discarded `result_qint` may GC and try to free/uncompute qubits now owned by `self`.
**Why it happens:** `__imul__` swaps `qubits` and `allocated_start` but does not swap `allocated_qubits` flag or `_is_uncomputed`.
**How to avoid:** After swap in `__imul__`, mark the discarded object appropriately (e.g., `result_qint._is_uncomputed = True` or `result_qint.allocated_qubits = False`).
**Warning signs:** Segfault during GC, intermittent test failures depending on GC timing.

### Pitfall 3: Scope Fix Breaking Other Operations
**What goes wrong:** Changing scope registration for multiplication results inadvertently prevents cleanup of comparison/bitwise intermediates.
**Why it happens:** The scope system handles both "temporary comparison qbools" (SHOULD be cleaned up) and "multiplication results" (should NOT be cleaned up) through the same mechanism.
**How to avoid:** Only modify the scope registration path in `__mul__`/`__rmul__`, not in `__init__` broadly. Test that comparison uncomputation still works after the fix.
**Warning signs:** Qubit leaks in comparison operations, test_conditionals.py regressions.

### Pitfall 4: Testing BUG-01 Fix via Simulation
**What goes wrong:** Attempting to simulate 32-bit multiplication (96+ qubits) causes Qiskit to crash or hang.
**Why it happens:** 32-bit QQ_mul uses 3*32=96 qubits minimum, far above the 17-qubit Aer limit.
**How to avoid:** Test BUG-01 by verifying circuit generation completes without segfault. Check that `QQ_mul(32)` returns a non-NULL sequence and that OpenQASM export succeeds. No Qiskit simulation.
**Warning signs:** Test hangs, memory exhaustion, simulator timeout.

### Pitfall 5: BUG-02 Fix Masking BUG-07
**What goes wrong:** Fixing the qarray __imul__ crash without addressing the scope issue means controlled multiplication still fails.
**Why it happens:** BUG-02 (crash) and BUG-07 (incorrect results in controlled context) may share root cause in __imul__, but BUG-07 requires scope-aware fix.
**How to avoid:** Test BUG-02 fix outside controlled context first, then test BUG-07 fix in controlled context separately.
**Warning signs:** Uncontrolled multiplication works but controlled multiplication returns wrong results.

## Code Examples

### BUG-01 Fix: MAXLAYERINSEQUENCE Increase
```c
// c_backend/include/types.h line 50
// Before:
#define MAXLAYERINSEQUENCE 10000
// After:
#define MAXLAYERINSEQUENCE 300000
```

### BUG-01 Test Pattern: Circuit Generation Without Simulation
```python
def test_32bit_mul_no_segfault():
    """Verify 32-bit QQ_mul generates circuit without segfault (no simulation)."""
    _c = ql.circuit()
    a = ql.qint(0, width=32)
    b = ql.qint(0, width=32)
    c = a * b
    qasm_str = ql.to_openqasm()
    assert len(qasm_str) > 0  # Circuit generated successfully
    assert 'qubit' in qasm_str.lower()  # Has qubit declarations
```

### BUG-07 Fix: Scope Depth Bypass in __mul__
```python
# In qint_arithmetic.pxi __mul__:
def __mul__(self, other):
    # ... (existing setup code) ...

    # Save scope depth and temporarily set to 0 to prevent
    # result registration in scope frame (BUG-COND-MUL-01 fix)
    saved_scope = current_scope_depth.get()
    current_scope_depth.set(0)

    result = qint(width=result_width)

    # Restore scope depth
    current_scope_depth.set(saved_scope)

    self.multiplication_inplace(other, result)
    # ... (rest of method) ...
```

### BUG-07 Test Pattern: Controlled Multiplication Verification
```python
def test_cond_mul_true(verify_circuit):
    """Controlled multiplication when condition is True."""
    def build():
        a = ql.qint(3, width=3)
        cond = a > 1  # True
        result = ql.qint(1, width=3)
        with cond:
            result *= 2  # Should execute: 1*2 = 2
        return (2, [a, cond, result])
    actual, expected = verify_circuit(build, width=3)
    assert actual == expected
```

### BUG-09 Deferral Rationale Pattern
```markdown
### BUG-09 (BUG-MOD-REDUCE): Deferred

**Status:** Explicitly deferred from v4.1 milestone.

**Root cause:** The `_reduce_mod` method uses iterative `>=` comparison
and conditional subtraction. Each comparison allocates temporary widened
qints that entangle with the value register. Across multiple iterations,
un-uncomputed comparison qbools accumulate entanglement, corrupting the
remainder. (Phase 38 research, Phase 86-03 uncomputation architecture findings.)

**Why a simple fix is insufficient:** The uncomputation architecture cannot
handle orphan temporaries from comparison operators (operation_type=None,
creation_scope=0, _start_layer=_end_layer=0). These temporaries are invisible
to both cascade uncomputation and lazy GC cleanup.

**Required approach:** Beauregard-style modular arithmetic (2003) which uses
a fundamentally different circuit topology: (a+b) mod N implemented as
add(a,b), sub(N), add(N) controlled by sign bit, sub(b) controlled by sign bit.
This requires careful ancilla management and a purpose-built circuit structure
rather than the current composition of generic comparison and conditional operations.
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Formula-based num_layer | MAXLAYERINSEQUENCE=10000 | Phase 07 (v1.0) | Prevented segfault at 8-bit, still fails at 32+ |
| Full Python multiplication_inplace | C hot_path_mul dispatch | Phase 60-02 (v2.2) | Performance improvement, same buffer issue |
| No scope management | Scope stack with creation_scope tracking | Phase 19 (v1.2) | Enabled auto-uncomputation, introduced BUG-COND-MUL-01 |
| No workaround for BUG-COND-MUL-01 | current_scope_depth.set(0) in tests | Phase 69-02 (v3.0) | Proved C backend correct, workaround not in production code |

## Open Questions

1. **BUG-02 Root Cause**
   - What we know: Tests skip qarray multiplication with "inherits C backend segfault." 8-bit QQ_mul (770 layers) is within MAXLAYERINSEQUENCE limit, so this is likely NOT the same as BUG-01.
   - What's unclear: Whether the crash is in qint.__imul__ qubit swap, GC-triggered uncomputation during qarray iteration, or something else entirely.
   - Recommendation: Investigate from scratch as CONTEXT.md directs. Start by enabling the skipped test with minimal width (width=1, single element) and incrementally increase complexity. Check if single `qint.__imul__` works outside qarray context.

2. **Scope Fix Interaction with __iadd__, __isub__**
   - What we know: BUG-07 fix (scope depth bypass in __mul__) should only affect multiplication.
   - What's unclear: Whether __iadd__ and __isub__ have the same scope issue (they also create intermediate results).
   - Recommendation: After fixing BUG-07, run the existing test_conditionals.py suite to verify addition/subtraction inside `with` blocks still work correctly. Addition and subtraction use a different code path (they modify in-place via C hot_path_add) so they may not have this issue.

3. **BUG-02 May Be BUG-01 in Disguise (for non-default widths)**
   - What we know: `ql.array([1,2,3])` defaults to width 8 (well within limit). But explicit `ql.array([...], width=16)` at width 16 would also be within limit (5,062 layers).
   - What's unclear: Whether any test or user code creates qarrays at widths that exceed MAXLAYERINSEQUENCE. The tests skip ALL qarray multiplication regardless of width.
   - Recommendation: After BUG-01 fix (MAXLAYERINSEQUENCE increase), re-test BUG-02. If the crash disappears, BUG-02 may have been a BUG-01 manifestation. If it persists, investigate the qubit swap/GC path.

## Sources

### Primary (HIGH confidence)
- `c_backend/include/types.h` -- MAXLAYERINSEQUENCE definition (line 50)
- `c_backend/src/IntegerMultiplication.c` -- QQ_mul, CQ_mul, cQQ_mul, cCQ_mul implementations (542 lines)
- `src/quantum_language/qint_arithmetic.pxi` -- __mul__, __imul__, multiplication_inplace (lines 508-728)
- `src/quantum_language/qint.pyx` -- __init__ scope registration (lines 314-325), __enter__/__exit__ (lines 800-879), _do_uncompute (lines 474-532), __del__ (lines 740-776)
- `src/quantum_language/_core.pyx` -- current_scope_depth, _scope_stack (lines 36-44, 142-144)
- `src/quantum_language/qarray.pyx` -- _inplace_binary_op, __imul__ (lines 767-856)
- `.planning/milestones/v3.0-phases/69-controlled-multiplication-division/69-02-SUMMARY.md` -- BUG-COND-MUL-01 root cause analysis
- `.planning/milestones/v1.7-phases/38-modular-reduction-fix/38-RESEARCH.md` -- BUG-MOD-REDUCE root cause analysis
- `.planning/phases/86-qft-bug-fixes/86-03-SUMMARY.md` -- Uncomputation architecture limitation (BUG-06 deferred findings)
- Layer count calculation: Mathematical analysis of QQ_mul loop structure (verified against code at IntegerMultiplication.c lines 158-299)

### Secondary (MEDIUM confidence)
- `hot_path_mul.c` -- C dispatch for multiplication (confirmed qubit layout, NULL check behavior)
- `.planning/research/ARCHITECTURE.md` -- Bug fix pattern classification (lines 70-150)
- Test files: `test_conditionals.py`, `test_qarray_elementwise.py`, `test_qarray_mutability.py`, `test_toffoli_multiplication.py` -- Current xfail/skip markers for these bugs

## Metadata

**Confidence breakdown:**
- BUG-01 fix: HIGH -- exact layer calculation confirmed, single #define change
- BUG-02 investigation: MEDIUM -- root cause hypotheses identified but not confirmed, investigation from scratch required
- BUG-07 fix: HIGH -- root cause confirmed in Phase 69-02, workaround proven in tests, fix approach clear
- BUG-09 deferral: HIGH -- documented across Phase 38 and Phase 86, decision locked by user

**Research date:** 2026-02-24
**Valid until:** 2026-03-24 (stable domain -- C backend and Cython code change infrequently)
