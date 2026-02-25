# Phase 93: Depth/Ancilla Tradeoff - Research

**Researched:** 2026-02-25
**Domain:** Quantum circuit depth/qubit tradeoff control, adder selection policy
**Confidence:** HIGH

## Summary

Phase 93 adds a user-facing `ql.option('tradeoff', ...)` API that controls whether the framework optimizes for circuit depth (using CLA adders) or qubit count (using CDKM ripple-carry adders). The existing codebase already has complete CLA (Brent-Kung) and CDKM (ripple-carry) adder implementations with dispatch logic in `hot_path_add_toffoli.c`. The current control mechanism uses two separate options (`fault_tolerant` and `cla`) plus the `qubit_saving` flag that selects BK vs KS CLA variant. Phase 93 replaces this fragmented approach with a unified `tradeoff` option offering three modes: `auto`, `min_depth`, and `min_qubits`.

The primary technical challenge is well-scoped: the dispatch logic already exists in C (`hot_path_add_toffoli.c`), the adder implementations are proven correct (v3.0), and the `ql.option()` mechanism is established. The new work involves: (1) adding the `tradeoff` option with three-mode semantics to the Python `option()` function, (2) mapping the tradeoff policy to the existing C-level `cla_override` field (or a new field), (3) implementing the "set once before ops" enforcement, (4) implementing CLA subtraction via two's complement (X-gate negation + CLA addition), (5) ensuring modular arithmetic always forces RCA regardless of tradeoff policy, and (6) determining the empirical auto-mode threshold.

**Primary recommendation:** Implement the tradeoff option as a Python-level state variable that maps to the existing C-level `cla_override` field. For `min_depth` mode, implement CLA subtraction via two's complement negation (X gates on each bit of the subtrahend, then CLA addition) rather than modifying the BK CLA to support inversion. The auto-mode threshold should be determined by benchmarking depth vs qubit count at widths 2-16.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Global state only via `ql.option('tradeoff', value)` -- no per-operation overrides
- Default mode is `auto` when no option is set
- Must be set before any arithmetic operations; changing after ops raises an error
- Getter: `ql.option('tradeoff')` with no value returns the current setting
- Invalid values (anything other than `auto`, `min_depth`, `min_qubits`) raise an error immediately
- `min_depth`: always pick the method with minimal depth for the given width, even for small widths (2-3 bits)
- `min_qubits`: always use CDKM (fewest ancillas)
- Modular arithmetic: always force RCA regardless of tradeoff policy
- Silent by default -- no logging or output about which adder was selected
- No circuit metadata about adder selection; users infer from circuit properties
- RCA override for modular ops is documented behavior -- no runtime warning
- Invalid tradeoff values raise an error (fail fast)
- Framework implements CLA subtraction via two's complement: X-gate negation + CLA addition
- In `min_depth` mode, subtraction seamlessly uses this approach
- When CLA truly can't be used, auto-fallback to CDKM
- Documented in code docstrings explaining the two's complement approach

### Claude's Discretion
- Exact empirical benchmark methodology and threshold value
- How to implement set-once-before-ops enforcement
- Two's complement negation circuit details
- Error message wording for invalid values and post-ops setting changes

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TRD-01 | User can set `ql.option('tradeoff', 'auto'\|'min_depth'\|'min_qubits')` to control adder selection | Existing `option()` function in `_core.pyx` supports string-keyed options; new `tradeoff` key maps to C-level dispatch control. Set-once enforcement via Python-level flag tracking. |
| TRD-02 | Auto mode selects CLA for width >= threshold, CDKM otherwise | Current `CLA_THRESHOLD` in `hot_path_add_toffoli.c` is 2 (too low). Auto mode needs empirical threshold based on depth benchmarking. The dispatch logic already checks `cla_override` and width. |
| TRD-03 | Modular arithmetic primitives force RCA regardless of tradeoff policy | Already implemented: `ToffoliModReduce.c` calls `toffoli_CQ_add`/`toffoli_cCQ_add` directly, bypassing `hot_path` dispatch entirely. No code changes needed -- just verify and document. |
| TRD-04 | CLA subtraction limitation documented clearly | BK CLA carry-copy ancilla are not uncomputed in reverse, so `invert=1` falls through to RCA. Two's complement approach (X-gate negation + CLA addition) enables CLA-based subtraction in `min_depth` mode. Documentation in code docstrings and user-facing docs. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| quantum_language (`_core.pyx`) | 0.1.0 | Option management, circuit state | Existing option() function with established patterns |
| hot_path_add_toffoli.c | Phase 74 | CLA/RCA dispatch logic | Proven dispatch with threshold, override, variant selection |
| ToffoliAdditionCLA.c | Phase 71-73 | Brent-Kung CLA implementation | O(log n) depth, compute-copy-uncompute pattern |
| ToffoliAdditionCDKM.c | Phase 66-67, 73 | CDKM ripple-carry implementation | O(n) depth, 1 ancilla |
| ToffoliModReduce.c | Phase 92 | Modular arithmetic primitives | Direct RCA calls, bypasses hot_path dispatch |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| pytest | existing | Test framework | All verification tests |
| qiskit_aer | existing | Statevector simulation | Correctness verification of tradeoff modes |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Python-level tradeoff state | C-level tradeoff field in circuit_t | C-level adds complexity for no benefit since dispatch logic already reads cla_override from circuit_t |
| Two's complement CLA subtraction | Fix BK CLA to support inversion | Much harder -- carry-copy uncomputation in reverse is an open research problem |
| Single auto threshold | Per-variant thresholds (BK vs KS) | KS not implemented, BK-only threshold sufficient |

## Architecture Patterns

### Current Option System Flow
```
Python: ql.option('key', value)
  -> _core.pyx option() function
  -> Reads/writes C-level circuit_t fields directly
  -> circuit_t.cla_override, circuit_t.arithmetic_mode, etc.
  -> C dispatch reads these fields at add time
```

### Current CLA Dispatch Flow (hot_path_add_toffoli.c)
```
toffoli_dispatch_qq/cq()
  -> Check: !invert && cla_override==0 && width >= CLA_THRESHOLD
  -> YES: Allocate CLA ancilla, try BK/KS sequence
    -> BK returns sequence? -> run_instruction() + free ancilla -> return
    -> BK returns NULL? -> free ancilla, fall through to RCA
  -> NO (or fallthrough): Allocate 1 ancilla, run CDKM RCA
```

### Recommended Tradeoff Implementation Pattern

**Pattern 1: Python-Level Tradeoff State**
**What:** Store tradeoff policy as Python string in `_core.pyx`, map to C-level `cla_override` field
**When to use:** All tradeoff option changes
**Example:**
```python
# In _core.pyx (new module-level variables)
_tradeoff_policy = 'auto'  # 'auto', 'min_depth', 'min_qubits'
_tradeoff_frozen = False     # True after first arithmetic op
_arithmetic_ops_performed = False

def option(key, value=None):
    global _tradeoff_policy, _tradeoff_frozen, _arithmetic_ops_performed
    if key == 'tradeoff':
        if value is None:
            return _tradeoff_policy
        if value not in ('auto', 'min_depth', 'min_qubits'):
            raise ValueError(f"Invalid tradeoff value: {value!r}. "
                           f"Must be 'auto', 'min_depth', or 'min_qubits'")
        if _arithmetic_ops_performed:
            raise RuntimeError(
                "Cannot change tradeoff policy after arithmetic operations. "
                "Set ql.option('tradeoff', ...) before any +, -, * operations."
            )
        _tradeoff_policy = value
        # Map to C-level cla_override
        circ = <circuit_s*><circuit_t*>_circuit
        if value == 'min_qubits':
            circ.cla_override = 1  # Force RCA
        else:
            circ.cla_override = 0  # Allow CLA dispatch
```

**Pattern 2: Set-Once Enforcement via Flag**
**What:** Track whether any arithmetic op has been performed; block tradeoff changes after that point
**When to use:** Prevent tradeoff changes mid-circuit
**Example:**
```python
# In addition_inplace (qint_preprocessed.pyx), before hot_path call:
from quantum_language._core import _mark_arithmetic_performed
_mark_arithmetic_performed()
```

**Pattern 3: Two's Complement CLA Subtraction**
**What:** For `min_depth` mode, implement subtraction as: (1) X-gate each bit of subtrahend to negate, (2) CLA addition with carry-in=1, (3) X-gate cleanup
**When to use:** `min_depth` mode subtraction (invert=1)
**Example (C-level in hot_path_add_toffoli.c):**
```c
// In toffoli_qq_uncont, when min_depth mode and invert=1:
if (min_depth_mode && invert) {
    // Two's complement: negate other register via X gates
    for (i = 0; i < other_bits; i++) {
        gate_t xg; memset(&xg, 0, sizeof(gate_t));
        x(&xg, other_qubits[i]);
        add_gate(circ, &xg);
    }
    // CLA addition (with +1 for two's complement)
    // ... CLA dispatch with classical +1 ...
    // Undo X gates to restore other register
    for (i = 0; i < other_bits; i++) {
        gate_t xg; memset(&xg, 0, sizeof(gate_t));
        x(&xg, other_qubits[i]);
        add_gate(circ, &xg);
    }
}
```

### Anti-Patterns to Avoid
- **Storing tradeoff policy in C circuit_t struct:** The tradeoff is a Python-level concept that maps to existing C fields. Adding another C field adds unnecessary complexity and ABI changes.
- **Per-operation tradeoff overrides:** Explicitly out of scope per REQUIREMENTS.md. Global policy only.
- **Changing CLA_THRESHOLD at runtime:** The auto-mode threshold should be a named constant, determined empirically, not user-configurable.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| CLA subtraction | Modified BK CLA with reverse carry-copy uncomputation | Two's complement (X-negate + CLA add + X-restore) | BK carry-copy uncomputation in reverse is an open problem; two's complement is proven correct |
| Adder dispatch | New dispatch system | Existing `hot_path_add_toffoli.c` dispatch with `cla_override` | Already handles CLA/RCA selection, threshold, fallback, controlled variants, Clifford+T |
| Option validation | Custom validation | Pattern from existing `option()` in `_core.pyx` | `fault_tolerant`, `cla`, `qubit_saving` already follow the same get/set pattern |
| Modular arithmetic RCA forcing | New modular dispatch | Existing `ToffoliModReduce.c` direct calls | Already calls `toffoli_CQ_add` directly, bypassing hot_path dispatch entirely |

**Key insight:** The existing dispatch infrastructure in `hot_path_add_toffoli.c` already does most of what Phase 93 needs. The `cla_override` field already controls CLA vs RCA selection. The main new work is the Python-level API, the auto threshold, and CLA subtraction via two's complement.

## Common Pitfalls

### Pitfall 1: Forgetting to Reset Tradeoff State on circuit() Creation
**What goes wrong:** Tradeoff policy and frozen flag persist across circuits, causing unexpected behavior when users create a new circuit.
**Why it happens:** `_core.pyx` circuit() constructor resets all state but new variables are easy to miss.
**How to avoid:** Add tradeoff policy reset and `_arithmetic_ops_performed = False` reset to the circuit `__init__` method, right alongside the existing resets for `_int_counter`, `_scope_stack`, etc.
**Warning signs:** Tests pass individually but fail when run in sequence.

### Pitfall 2: Two's Complement Carry-In +1 for CLA Subtraction
**What goes wrong:** X-gate negation of the subtrahend produces one's complement (bitwise NOT). Two's complement requires adding +1, which is easy to forget, producing off-by-one errors.
**Why it happens:** `~x + 1 = -x` in two's complement. X gates give `~x`, not `-x`.
**How to avoid:** After X-gate negation, add +1 via a classical increment (CQ_add with value=1) or integrate the +1 into the CLA addition. The simplest approach: negate via X gates, then do CLA add with the negated value, then handle the +1 as a separate CQ increment.
**Warning signs:** Subtraction results are off by 1 for all inputs.

**Alternative approach (simpler):** Use CQ subtraction path instead. For QQ subtraction `a -= b` in min_depth mode:
1. X-gate all bits of `b` (negate)
2. CLA QQ_add (a += ~b) -- this is a forward CLA addition, not inversion
3. CQ_add (a += 1) -- the +1 for two's complement
4. X-gate all bits of `b` (restore)

This avoids needing to modify the CLA adder at all. The CLA handles the main addition, and a trivial CQ increment handles the +1.

### Pitfall 3: Auto Mode Threshold Too Low (Current CLA_THRESHOLD=2)
**What goes wrong:** CLA is used for 2-bit and 3-bit additions where the overhead of CLA ancilla (generate, tree, carry-copy) exceeds the depth benefit.
**Why it happens:** The current `CLA_THRESHOLD=2` was set during initial CLA development to maximize CLA usage for testing. It was never tuned for the depth/qubit tradeoff.
**How to avoid:** Benchmark circuit depth for CLA vs CDKM at widths 2-16. Set auto threshold at the crossover point where CLA depth advantage justifies the extra ancilla.
**Warning signs:** Auto mode uses CLA at width=2 where CDKM depth is already O(1) (just 6 layers).

### Pitfall 4: Compile Cache Key Missing Tradeoff Policy
**What goes wrong:** `@ql.compile` caches a circuit built with one tradeoff policy, replays it when the policy has changed.
**Why it happens:** Current cache key is `(classical_args, widths, control_count, qubit_saving)`. If tradeoff changes CLA override, the cached circuit uses wrong adder.
**How to avoid:** FIX-04 (Phase 94) requires adding `tradeoff_policy` to cache key. However, since the tradeoff policy is frozen before ops, and the cache is cleared on `circuit()`, this may not be a practical concern for Phase 93. Still, document the dependency.
**Warning signs:** Compiled function produces different results depending on when it was first called.

### Pitfall 5: Modular Arithmetic CLA Override Bypass
**What goes wrong:** If modular arithmetic ever starts using the hot_path dispatch instead of direct calls, the CLA could be selected for modular ops.
**Why it happens:** `ToffoliModReduce.c` calls `toffoli_CQ_add` directly. If someone refactors to use `hot_path_add_cq`, the tradeoff policy would apply.
**How to avoid:** Document that modular arithmetic MUST call adder functions directly, never through hot_path dispatch. Add a comment in `ToffoliModReduce.c` explaining this.
**Warning signs:** Modular arithmetic test failures at widths >= auto threshold.

## Code Examples

### Example 1: Option API Usage
```python
import quantum_language as ql

c = ql.circuit()

# Set tradeoff before any arithmetic
ql.option('tradeoff', 'min_depth')  # Always use CLA
# ql.option('tradeoff', 'min_qubits')  # Always use CDKM/RCA
# ql.option('tradeoff', 'auto')  # CLA for wide, CDKM for narrow (default)

# Query current setting
print(ql.option('tradeoff'))  # 'min_depth'

a = ql.qint(5, width=8)
b = ql.qint(3, width=8)
result = a + b  # Uses CLA in min_depth mode

# Changing after arithmetic raises error
# ql.option('tradeoff', 'min_qubits')  # RuntimeError!
```

### Example 2: Two's Complement CLA Subtraction (C-level pseudocode)
```c
// In hot_path_add_toffoli.c, toffoli_qq_uncont():
// When tradeoff == min_depth AND invert (subtraction):
//   a -= b  ===  a += (~b + 1)  ===  X(b) + CLA(a += b) + CQ(a += 1) + X(b)

// Step 1: Negate b via X gates (one's complement)
for (i = 0; i < other_bits; i++) {
    gate_t xg; memset(&xg, 0, sizeof(gate_t));
    x(&xg, other_qubits[i]);
    add_gate(circ, &xg);
}

// Step 2: CLA addition a += ~b (forward CLA, invert=0)
// [dispatch CLA with invert=0]

// Step 3: CQ increment a += 1 (two's complement correction)
// Use toffoli_CQ_add(result_bits, 1) for +1

// Step 4: Restore b via X gates
for (i = 0; i < other_bits; i++) {
    gate_t xg; memset(&xg, 0, sizeof(gate_t));
    x(&xg, other_qubits[i]);
    add_gate(circ, &xg);
}
```

### Example 3: Tradeoff Policy Mapping to C Override
```python
# Mapping from tradeoff policy to existing C fields:
#
# 'auto':       cla_override=0, CLA_THRESHOLD applied (new empirical value)
# 'min_depth':  cla_override=0, CLA_THRESHOLD=2 (use CLA for all widths >= 2)
# 'min_qubits': cla_override=1  (force RCA everywhere)
```

## Key Architectural Findings

### Finding 1: Existing Dispatch is Nearly Complete (HIGH confidence)
The `hot_path_add_toffoli.c` dispatch already checks `!invert && circ->cla_override == 0 && result_bits >= CLA_THRESHOLD`. The `cla_override` field in `circuit_t` already controls CLA vs RCA selection. The auto threshold `CLA_THRESHOLD=2` is a compile-time constant that can be made policy-dependent.

**Source:** Direct code inspection of `hot_path_add_toffoli.c` lines 24, 112, 153, 240, 288, 389, 453, 534, 609.

### Finding 2: Modular Arithmetic Already Forces RCA (HIGH confidence)
`ToffoliModReduce.c` helper functions (`mod_cq_add`, `mod_cq_sub`, `mod_qq_add`, `mod_qq_sub`, `mod_ccq_add`, etc.) call `toffoli_CQ_add`/`toffoli_QQ_add` directly, completely bypassing the `hot_path` dispatch that checks `cla_override`. Comments explicitly state "Forces RCA (CDKM) -- does NOT use hot_path dispatch."

**Source:** Direct code inspection of `ToffoliModReduce.c` lines 77-80, 131-132.

### Finding 3: BK CLA Subtraction Limitation is Well-Documented (HIGH confidence)
The `!invert` guard in all CLA dispatch paths (8 occurrences in `hot_path_add_toffoli.c`) ensures BK CLA is only used for addition (forward direction). Subtraction (invert=1) always falls through to CDKM RCA. The reason: BK CLA carry-copy ancilla are not properly uncomputed when the sequence runs in reverse.

**Source:** `hot_path_add_toffoli.c` line 151-152 comment: "CLA dispatch: forward only (BK CLA carry-copy not fully uncomputed). Subtraction (invert=1) falls through to RCA."

### Finding 4: Auto Threshold Needs Empirical Tuning (MEDIUM confidence)
Current `CLA_THRESHOLD=2` is too aggressive for an "auto" mode. At width=2, CDKM uses 1 ancilla with O(n) depth (~6 layers), while BK CLA uses 2*(2-1) + merges ancilla with O(log n) depth. For very small widths the CLA overhead (more ancilla, setup/teardown) may not justify the depth reduction. A reasonable threshold is likely 4-8 bits, but needs empirical measurement.

**Source:** Ancilla counts: CDKM = 1 ancilla for all widths. BK CLA = 2*(n-1) + tree_merges ancilla (e.g., width=4: ~7 ancilla, width=8: ~19 ancilla). Depth: CDKM = O(6n) layers, BK CLA = O(log n * constant) layers.

### Finding 5: Two's Complement CLA Subtraction is Straightforward (HIGH confidence)
The `qint.__neg__` already implements two's complement as `result = 0; result -= self`. For CLA subtraction, the approach is: X-gate each bit of the subtrahend (one's complement), then CLA addition (forward direction), then CQ increment by 1 (two's complement correction), then X-gate cleanup to restore the subtrahend. This keeps the subtrahend's quantum state intact.

**Source:** `qint_preprocessed.pyx` lines 1229-1230: `result -= self  # 0 - self = two's complement negation`.

### Finding 6: Circuit Reset Already Clears All State (HIGH confidence)
The `circuit.__init__` method in `_core.pyx` resets `_int_counter`, `_scope_stack`, `_global_creation_counter`, and calls `_clear_compile_caches()`. The tradeoff state variables must be added to this reset sequence.

**Source:** `_core.pyx` lines 329-350.

## Implementation Strategy

### Phase: Tradeoff Option API + Auto Threshold + CLA Subtraction

**Step 1: Add tradeoff option to _core.pyx**
- Add module-level variables: `_tradeoff_policy = 'auto'`, `_arithmetic_ops_performed = False`
- Add `'tradeoff'` case to `option()` function with validation
- Add reset logic to `circuit.__init__`
- Map tradeoff to C-level: `min_qubits` -> `cla_override=1`, others -> `cla_override=0`

**Step 2: Implement set-once enforcement**
- Add `_mark_arithmetic_performed()` function
- Call it from `addition_inplace()` in `qint_preprocessed.pyx` (and multiplication, division)
- The `option('tradeoff', value)` raises `RuntimeError` if `_arithmetic_ops_performed` is True

**Step 3: Implement auto mode with empirical threshold**
- Either: Add a new C-level field `tradeoff_mode` to `circuit_t` that the dispatch checks
- Or: Change `CLA_THRESHOLD` based on tradeoff mode at the Python level
- Best approach: Add `int tradeoff_auto_threshold` to circuit_t, set from Python. In auto mode, dispatch checks `width >= tradeoff_auto_threshold`. In min_depth mode, threshold=2. In min_qubits mode, cla_override=1 (threshold irrelevant).
- Determine empirical threshold by benchmarking.

**Step 4: Implement CLA subtraction via two's complement**
- In `hot_path_add_toffoli.c`, when tradeoff is `min_depth` and `invert=1`:
  - Emit X gates on subtrahend bits
  - Do CLA addition (forward, invert=0) on the negated bits
  - Do CQ increment by 1 (toffoli_CQ_add with value=1)
  - Emit X gates on subtrahend bits (restore)
- This requires reading a new field from circuit_t to know if min_depth mode is active
- Controlled subtraction: same pattern with controlled X gates and controlled CQ add

**Step 5: Document BK CLA subtraction limitation**
- Add docstring to `ql.option('tradeoff', ...)` explaining modes
- Add comment in `hot_path_add_toffoli.c` explaining two's complement approach
- Document in user-facing help that CLA subtraction uses two's complement internally

**Step 6: Verify TRD-03 (modular arithmetic forces RCA)**
- Add test that sets `tradeoff='min_depth'` then performs modular arithmetic
- Verify modular ops produce same results regardless of tradeoff setting
- Verify modular circuit depth is same regardless of tradeoff setting

## C-Level Changes Required

### Option 1: Minimal C Changes (Recommended)
Add one field to `circuit_t`:
```c
int tradeoff_auto_threshold; // Width threshold for auto CLA dispatch (set from Python)
```

Modify `CLA_THRESHOLD` usage in `hot_path_add_toffoli.c`:
```c
// Instead of: result_bits >= CLA_THRESHOLD
// Use: result_bits >= circ->tradeoff_auto_threshold
```

For min_depth CLA subtraction, add a field:
```c
int tradeoff_min_depth; // 1 = min_depth mode (enable CLA subtraction via two's complement)
```

### Option 2: Full C-Level Tradeoff Enum
```c
typedef enum { TRADEOFF_AUTO = 0, TRADEOFF_MIN_DEPTH = 1, TRADEOFF_MIN_QUBITS = 2 } tradeoff_mode_t;
```
This is cleaner but requires more changes to the C header and Cython declarations.

**Recommendation:** Option 2 (enum) for clarity, since we need both the threshold and the min_depth flag anyway.

## Open Questions

1. **Exact auto-mode threshold value**
   - What we know: CDKM uses 1 ancilla, BK CLA uses 2*(n-1)+merges ancilla. CLA has O(log n) depth vs O(n) for CDKM.
   - What's unclear: The exact crossover point where CLA depth advantage justifies ancilla overhead. The current CLA_THRESHOLD=2 is likely too low.
   - Recommendation: Benchmark during implementation. Run width 2-16 with both adders, measure depth. The threshold is where CLA depth < CDKM depth (likely width 4-6).

2. **Controlled CLA subtraction via two's complement**
   - What we know: Controlled QQ subtraction currently uses controlled CDKM. Two's complement approach needs controlled X gates on subtrahend.
   - What's unclear: Whether controlled X gates (CX from control to each subtrahend bit) plus controlled CQ increment (+1) produces correct results.
   - Recommendation: CX gates for controlled negation are standard. The controlled increment can use `toffoli_cCQ_add(bits, 1)`. Should work but needs careful testing.

3. **CQ subtraction in min_depth mode**
   - What we know: CQ subtraction (self -= classical_value) currently uses `toffoli_CQ_add(bits, value)` with `invert=1` which runs the CDKM sequence in reverse.
   - What's unclear: Whether CQ subtraction needs the two's complement approach, or if the simpler path is to just compute `CQ_add(bits, 2^width - value)` (classical two's complement) and use CLA addition.
   - Recommendation: For CQ subtraction, compute the two's complement classically: `negated_value = (1 << width) - value`. Then call CLA CQ addition with the negated value. This avoids any X-gate overhead and is the simplest approach.

## Validation Architecture

> Note: `workflow.nyquist_validation` not found in config.json (only `workflow.research`, `plan_check`, `verifier`, `auto_advance` present). Including validation section based on project test patterns.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | pytest |
| Config file | implicit (pytest conventions) |
| Quick run command | `pytest tests/python/test_tradeoff.py -x -v` |
| Full suite command | `pytest tests/python/ -v` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TRD-01 | Set/get tradeoff option with three modes | unit | `pytest tests/python/test_tradeoff.py::TestTradeoffOption -x` | No - Wave 0 |
| TRD-01 | Invalid values raise ValueError | unit | `pytest tests/python/test_tradeoff.py::TestTradeoffOptionErrors -x` | No - Wave 0 |
| TRD-01 | Set-once enforcement after arithmetic ops | unit | `pytest tests/python/test_tradeoff.py::TestTradeoffFrozen -x` | No - Wave 0 |
| TRD-02 | Auto mode selects CLA for width >= threshold | integration | `pytest tests/python/test_tradeoff.py::TestAutoMode -x` | No - Wave 0 |
| TRD-02 | Auto mode depth verified for small vs large widths | integration | `pytest tests/python/test_tradeoff.py::TestAutoModeDepth -x` | No - Wave 0 |
| TRD-03 | Modular arithmetic forces RCA regardless of tradeoff | integration | `pytest tests/python/test_tradeoff.py::TestModularForceRCA -x` | No - Wave 0 |
| TRD-04 | CLA subtraction via two's complement in min_depth mode | integration | `pytest tests/python/test_tradeoff.py::TestCLASubtraction -x` | No - Wave 0 |
| TRD-04 | BK CLA subtraction limitation documented | manual-only | Code review of docstrings | N/A |

### Sampling Rate
- **Per task commit:** `pytest tests/python/test_tradeoff.py -x -v`
- **Per wave merge:** `pytest tests/python/ -v`
- **Phase gate:** Full suite green before verification

### Wave 0 Gaps
- [ ] `tests/python/test_tradeoff.py` -- covers TRD-01, TRD-02, TRD-03, TRD-04
- [ ] Benchmark script for auto-mode threshold determination

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection: `hot_path_add_toffoli.c` (dispatch logic, CLA_THRESHOLD, cla_override checks)
- Direct codebase inspection: `_core.pyx` (option() function, circuit constructor, state management)
- Direct codebase inspection: `ToffoliModReduce.c` (modular arithmetic direct RCA calls, bypass hot_path)
- Direct codebase inspection: `circuit.h` (circuit_t struct fields: cla_override, arithmetic_mode, qubit_saving)
- Direct codebase inspection: `ToffoliAdditionCLA.c` (BK CLA ancilla count, compute-copy-uncompute pattern)
- Direct codebase inspection: `ToffoliAdditionCDKM.c` (CDKM MAJ/UMA implementation, 1-ancilla design)
- Direct codebase inspection: `qint_preprocessed.pyx` (addition_inplace, __neg__, __sub__)

### Secondary (MEDIUM confidence)
- Phase 92 RESEARCH.md (modular arithmetic patterns, Beauregard references)
- Project STATE.md (BK CLA subtraction fallback decision, carry-copy known limitation)
- Existing test files: `test_cla_addition.py`, `test_cla_verification.py`, `test_cla_bk_algorithm.py`

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - all components are existing project infrastructure
- Architecture: HIGH - existing dispatch, option, and adder systems are well-understood
- Pitfalls: HIGH - based on direct code inspection and documented known limitations
- CLA subtraction approach: HIGH - two's complement is mathematically proven; X-gate negation is standard

**Research date:** 2026-02-25
**Valid until:** 2026-03-25 (stable -- all components are internal to this project)
