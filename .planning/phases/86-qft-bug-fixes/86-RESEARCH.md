# Phase 86: QFT Bug Fixes - Research

**Researched:** 2026-02-24
**Domain:** QFT-based quantum arithmetic (addition, controlled addition, division, modulo) in C/Cython backend
**Confidence:** HIGH

## Summary

Phase 86 addresses four QFT arithmetic bugs (BUG-04, BUG-05, BUG-06, BUG-08) with root-cause fixes in dependency order. Research identifies that these bugs stem from three independent root causes: (1) mixed-width qubit alignment in `hot_path_add_qq`, (2) CCP decomposition rotation errors in `cQQ_add`, and (3) widened-temporary ancilla pollution in the `>=` comparison operator used by the division loop. The bugs have a dependency chain: BUG-04 (mixed-width addition) is independent; BUG-05 (cQQ_add) may share root cause with BUG-04 in the `QQ_add` sequence; BUG-06 (MSB ancilla leak) directly causes BUG-08 (QFT division failures) because division uses `>=` comparisons in a loop.

**Primary recommendation:** Fix BUG-04 first (mixed-width QQ_add alignment), then BUG-05 (cQQ_add rotation audit), then BUG-06 (comparison ancilla leak), then verify BUG-08 resolves. Each fix must be followed by the full arithmetic test suite before proceeding.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Bottom-up execution: BUG-04 (mixed-width addition) -> BUG-05 (controlled QQ addition) -> BUG-06 (MSB ancilla leak) -> BUG-08 (QFT division/modulo)
- One plan per bug, strict sequential execution -- each must fully pass verification before the next begins
- If fixing BUG-04 also resolves BUG-05 (same root cause), collapse the plans and verify both together
- Same collapse logic applies to BUG-06/BUG-08 -- fix BUG-06 first, check if BUG-08 resolves
- Minimal targeted patches only -- fix the bug, nothing more. No surrounding code cleanup
- Root-cause diagnosis captured in commit messages, not excessive code comments (comments only where fix isn't self-explanatory)
- If a bug is deeper than expected (e.g., fundamental QFT rotation flaw), attempt the deeper fix rather than escalating to a separate phase
- Remove xfail markers as part of each bug fix -- tests should pass clean immediately after each fix

### Verification Depth
- Test widths match success criteria exactly: up to 8 bits for addition, widths 2-4 for cQQ_add, widths 2-5 for division (width 6 exceeds 17-qubit simulation limit since QQ_div requires 3*width qubits)
- Width combinations for mixed-width addition: representative sampling (same-width, off-by-one, max asymmetry), not exhaustive
- Value coverage: boundary + random values at every width (0, 1, max, and a few random), not exhaustive enumeration
- Full test suite (all arithmetic tests) run after every single bug fix to catch regressions immediately
- Mod tested separately from division even though they share underlying code

### Division Bugs Approach
- BUG-06 (MSB ancilla leak) and BUG-08 (QFT division failures) are likely related -- ancilla leak probably causes incorrect comparisons cascading into division failures
- Fix BUG-06 first, then run division tests to see if BUG-08 resolves
- "Zero xfail" means all tests pass within feasible simulation widths (<=5 bits). Failures at width 6+ due to qubit limits are acceptable
- Mod operations have their own test suite and must be verified independently

### Claude's Discretion
- Exact diagnostic approach for each bug (code analysis, bisection, etc.)
- Test fixture design and parameterization strategy
- Whether to add targeted regression tests beyond existing test files
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BUG-04 | Fix mixed-width QFT addition off-by-one (BUG-WIDTH-ADD) | Root cause identified: `hot_path_add_qq` passes narrower operand's qubits but `QQ_add(result_bits)` expects `result_bits` qubits for both operands. Qubit array layout misaligns when `self_bits != other_bits`. |
| BUG-05 | Fix QFT controlled QQ addition CCP rotation errors at width 2+ (BUG-CQQ-QFT) | Partially fixed in Quick-015 (Block 2 CP control target). Remaining issue: the cQQ_add CCP decomposition half-rotation summation may accumulate incorrectly for widths > 1. The algorithm's Block 1/2/3 structure needs rotation angle audit. |
| BUG-06 | Fix MSB comparison leak in division (BUG-DIV-02, 9 cases per div/mod test file) | Root cause: `__ge__` creates widened temporaries (`temp_self`, `temp_other` at comp_width = max_bits + 1) that are never explicitly cleaned up. In the division loop, each iteration's comparison allocates new widened temps whose qubits conflict with previous iterations' temps, corrupting the MSB sign bit extraction. |
| BUG-08 | Fix QFT division/modulo pervasive failures at all tested widths (BUG-QFT-DIV) | Cascading from BUG-06: division loop calls `remainder >= trial_value` at each bit position. If the comparison's widened temporaries leak state between iterations, all subsequent comparisons and conditional subtractions produce wrong results. Cross-backend tests confirm Toffoli division works correctly -- the bug is QFT-specific. |
</phase_requirements>

## Architecture Patterns

### Bug Dependency Chain

```
BUG-04 (mixed-width add)  ─── independent, fix first
     │
     └── BUG-05 (cQQ_add)  ─── may share root cause with BUG-04
                                 if QQ_add alignment fix propagates to cQQ_add

BUG-06 (MSB ancilla leak) ─── fix after BUG-04/05
     │
     └── BUG-08 (QFT div)  ─── likely resolves when BUG-06 is fixed
```

### QFT Addition Architecture (IntegerAddition.c)

The QFT addition pipeline:
1. **QFT transform** on target register (qubits 0..bits-1)
2. **Phase rotations** from source register (QQ) or classical value (CQ)
3. **Inverse QFT** on target register

Key sequence types:
- `QQ_add(bits)`: qubit layout `[0..bits-1]=target, [bits..2*bits-1]=source`
- `cQQ_add(bits)`: qubit layout `[0..bits-1]=target, [bits..2*bits-1]=source, [2*bits]=control`
- `CQ_add(bits, value)`: qubit layout `[0..bits-1]=target`
- `cCQ_add(bits, value)`: qubit layout `[0..bits-1]=target, [bits]=control`

### Mixed-Width Addition Path (BUG-04)

The `__add__` operator in `qint_arithmetic.pxi` (line 102-120):
```python
result_width = max(self.bits, other.bits)
a = qint(width=result_width)
a ^= self     # Copy self to a (via __ixor__)
a += other    # In-place add: a.addition_inplace(other)
```

In `addition_inplace` -> `hot_path_add_qq`:
```c
int result_bits = self_bits > other_bits ? self_bits : other_bits;
// self qubits at positions 0..self_bits-1
// other qubits at positions self_bits..self_bits+other_bits-1
seq = QQ_add(result_bits);
run_instruction(seq, qa, invert, circ);
```

**Root cause:** `QQ_add(result_bits)` generates a sequence expecting `result_bits` qubits for BOTH target and source. But `other_qubits` may only occupy `other_bits` positions. When `other_bits < result_bits`, the remaining positions in the qubit array contain uninitialized or wrong qubit indices, causing phase rotations on wrong qubits.

### cQQ_add Algorithm (BUG-05)

The Beauregard-style controlled addition decomposes CCP gates into CP + CNOT sequences:
- **Block 1**: Unconditional half-rotations (CP with external control)
- **Block 2**: CNOT + negative half-rotations + CNOT (CCP decomposition)
- **Block 3**: Positive half-rotations from b-register

Quick-015 fixed the Block 2 CP control target but the algorithm may still have rotation accumulation errors at widths > 1.

### Division Loop Pattern (BUG-06, BUG-08)

```python
for bit_pos in range(self.bits - 1, -1, -1):
    trial_value = divisor << bit_pos
    can_subtract = remainder >= trial_value  # Creates widened temps!
    with can_subtract:
        remainder -= trial_value
        quotient += (1 << bit_pos)
```

The `>=` operator (via `~(self < other)`) creates `temp_self` and `temp_other` at `comp_width = max_bits + 1`. These are Python objects that get garbage collected, but their qubits remain allocated until GC runs. In a tight loop, this means:
1. Iteration 0: allocates temp qubits A, B for comparison
2. Iteration 1: allocates temp qubits C, D for comparison
3. The widened subtraction `temp_self -= temp_other` in iteration 1 may re-use qubit indices from iteration 0 if GC freed them, OR may get fresh qubits. Either way, the comparison result can be corrupted.

The 9 known-failing cases in `KNOWN_DIV_MSB_LEAK` all involve `a >= 2^(w-1)`, confirming the MSB boundary is where the widened-temporary sign bit extraction breaks.

## Common Pitfalls

### Pitfall 1: Cached Sequence Mutation
**What goes wrong:** `QQ_add` returns cached `sequence_t*` pointers. If a fix modifies the sequence in-place (e.g., adjusting rotation angles), it corrupts ALL subsequent calls.
**How to avoid:** Never modify a returned sequence. If the fix requires different sequences for different width combinations, generate new sequences keyed by both widths.

### Pitfall 2: Hardcoded vs Dynamic Mismatch
**What goes wrong:** Widths 1-16 use hardcoded sequences from `c_backend/src/sequences/add_seq_*.c`. Widths > 16 use the dynamic generator in `IntegerAddition.c`. A fix to the dynamic path won't fix the hardcoded path, and vice versa.
**How to avoid:** Apply fixes to BOTH the C generator AND the Python generation scripts (`scripts/generate_seq_*.py`), then regenerate hardcoded sequences.

### Pitfall 3: Qubit Array Right-Alignment
**What goes wrong:** Python stores qubits in a 64-element array right-aligned (index 63 = LSB for width-1). The C backend expects packed arrays starting at index 0. The extraction in `addition_inplace` handles this (`self_offset = 64 - self_bits`), but mixed-width operations must align both operands consistently.
**How to avoid:** Verify qubit extraction for both `self` and `other` in `addition_inplace` when widths differ.

### Pitfall 4: Division Comparison Ancilla Allocation
**What goes wrong:** The allocator (`allocator_alloc`/`allocator_free` in `qubit_allocator.c`) tracks blocks. Widened temporaries from `__lt__`/`__ge__` allocate qubits that may overlap with qubits allocated by the division's `quotient` and `remainder` registers if not properly sequenced.
**How to avoid:** The fix should ensure comparison temporaries are fully cleaned up (qubits returned to allocator) before the next loop iteration begins. May require explicit `del` or scope management.

## Code Examples

### Current Mixed-Width QQ Add Qubit Layout (BUGGY)
```c
// hot_path_add.c:hot_path_add_qq
// When self_bits=5, other_bits=3, result_bits=5:
// qa[0..4] = self_qubits (5 qubits, correct)
// qa[5..7] = other_qubits (3 qubits, correct)
// qa[8..9] = UNINITIALIZED (QQ_add(5) expects 5 source qubits at [5..9])
```

### Required Fix Pattern for BUG-04
The fix should either:
1. Pad the narrower operand with zero-qubits in the qubit array, OR
2. Use `QQ_add(other_bits)` and apply to the matching LSB portion, OR
3. Zero-extend the narrower operand before calling `hot_path_add_qq`

Option 3 (zero-extension at the Python level) is safest because it reuses the proven `__ixor__` copy path already used in `__lt__`.

### Existing Widened-Copy Pattern (from __lt__)
```python
# qint_comparison.pxi:__lt__
comp_width = max(self.bits, other.bits) + 1
temp_self = qint(0, width=comp_width)
temp_other = qint(0, width=comp_width)
# LSB-aligned CNOT copy:
for i_bit in range(operand_bits):
    qubit_array[0] = temp.qubits[64 - comp_width + i_bit]
    qubit_array[1] = self.qubits[64 - operand_bits + i_bit]
    seq = Q_xor(1)
    run_instruction(seq, &arr[0], False, _circuit)
```

## Test Infrastructure

### Existing Test Files Relevant to Phase 86
| File | What it tests | xfail markers |
|------|--------------|---------------|
| `tests/test_add.py` | QQ and CQ addition, widths 1-8 | None (same-width only) |
| `tests/test_div.py` | Floor division, widths 1-4 | 9 cases in KNOWN_DIV_MSB_LEAK |
| `tests/test_mod.py` | Modulo, widths 1-4 | 9 cases in KNOWN_MOD_MSB_LEAK |
| `tests/test_copy_binops.py` | Mixed-width add/sub | 2 xfails (BUG-WIDTH-ADD) |
| `tests/quick/test_014_cqq_add_bug.py` | cQQ_add circuit building | None (doesn't test arithmetic correctness) |
| `tests/python/test_cross_backend.py` | QFT vs Toffoli comparison | Many xfails for QFT-specific failures |

### Verification Commands
```bash
# Full arithmetic test suite (run after every fix)
pytest tests/test_add.py tests/test_div.py tests/test_mod.py tests/test_copy_binops.py tests/test_compare.py tests/test_sub.py -v --timeout=300

# Quick smoke test
pytest tests/test_add.py -k "width_1 or width_2" -v

# Cross-backend (confirms Toffoli still works)
pytest tests/python/test_cross_backend.py -v --timeout=600

# Full regression suite
pytest tests/python/ -v --timeout=600
```

### Simulation Constraints
- **Max qubits: 17** (from MEMORY.md)
- **Max threads: 4** (AerSimulator `max_parallel_threads=4`)
- QQ_div at width w requires ~3w qubits -> max simulation width is 5 (15 qubits)
- QQ_add at width w requires 2w qubits -> max simulation width is 8 (16 qubits)
- cQQ_add at width w requires 2w+1 qubits -> max simulation width is 8 (17 qubits)

## Open Questions

1. **cQQ_add rotation angle correctness**
   - What we know: Quick-015 fixed the CP control target in Block 2. Post-fix, all 61 hardcoded sequence tests pass and 31 Python addition tests pass.
   - What's unclear: Whether the half-rotation accumulation formula in Block 1 (`value += 2*PI / 2^(i+1) / 2`) is mathematically correct for the Beauregard CCP decomposition. The `/2` factor creates half-angles, but the summation over `i` may produce incorrect accumulated phases.
   - Recommendation: Mathematical audit of the cQQ_add blocks against Beauregard's paper. Compare gate-by-gate output with a reference implementation.

2. **Division comparison cleanup mechanism**
   - What we know: The `>=` comparison creates widened temps that are Python objects subject to GC. The division loop runs `self.bits` iterations.
   - What's unclear: Whether explicit cleanup (e.g., `del can_subtract` after the `with` block) is sufficient, or whether the underlying qubit allocator state needs manual management.
   - Recommendation: Investigate whether `gc.collect()` inside the loop fixes the issue. If so, add explicit cleanup. If not, the fix may need to use a different comparison strategy that avoids widened temporaries.

## Sources

### Primary (HIGH confidence)
- `c_backend/src/IntegerAddition.c` - QQ_add, cQQ_add, CQ_add, cCQ_add implementations
- `c_backend/src/hot_path_add.c` - Qubit array layout for QFT addition dispatch
- `src/quantum_language/qint_arithmetic.pxi` - Python-level addition operators
- `src/quantum_language/qint_division.pxi` - Division loop implementation
- `src/quantum_language/qint_comparison.pxi` - `__lt__`, `__ge__` widened-temp pattern
- `.planning/quick/015-fix-cqq-add-algorithm-bugs/015-SUMMARY.md` - Prior cQQ_add fix history
- `.planning/research/PITFALLS-QUALITY-EFFICIENCY.md` - Identified pitfalls and prevention strategies

### Secondary (MEDIUM confidence)
- Test files (`test_div.py`, `test_mod.py`, `test_copy_binops.py`) - Known failure patterns
- `tests/python/test_cross_backend.py` - QFT vs Toffoli comparison failures

## Metadata

**Confidence breakdown:**
- BUG-04 root cause: HIGH - qubit alignment mismatch in hot_path_add_qq is clear from code analysis
- BUG-05 root cause: MEDIUM - partially fixed, remaining issue needs mathematical audit
- BUG-06 root cause: HIGH - widened-temporary pattern in comparison is documented in code comments
- BUG-08 root cause: HIGH - cascading from BUG-06 confirmed by KNOWN_DIV_MSB_LEAK pattern

**Research date:** 2026-02-24
**Valid until:** 2026-03-24 (stable codebase, no external dependencies changing)
