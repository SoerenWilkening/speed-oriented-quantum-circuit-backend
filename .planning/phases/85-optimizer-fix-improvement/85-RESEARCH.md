# Phase 85: Optimizer Fix & Improvement - Research

**Researched:** 2026-02-23
**Domain:** C backend optimizer correctness and performance; Python compile decorator replay overhead
**Confidence:** HIGH

## Summary

Phase 85 targets three specific improvements to the circuit optimizer and compile replay system: (1) fixing a latent loop direction bug in `smallest_layer_below_comp`, (2) upgrading gate placement from O(L) linear scan to O(log L) binary search, and (3) reducing `@ql.compile` replay overhead through profiling-driven optimization.

The bug in `smallest_layer_below_comp` (optimizer.c line 32) is confirmed: the loop `for (int i = last_index; i > 0; ++i)` increments when it should decrement, creating undefined behavior by reading past the end of the `occupied_layers_of_qubit` array. This works by accident in most cases because of how the occupied layers array is structured and because `last_index` is typically small, but it is a latent memory safety issue that could produce incorrect gate placement on larger circuits.

The binary search upgrade targets the `occupied_layers_of_qubit[qubit]` array, which stores layer indices in insertion order (monotonically increasing). This property makes it amenable to standard `bisect_left` / lower-bound binary search. The compile replay path (`inject_remapped_gates`) is a Python-to-C bridge that creates and injects gate dicts one at a time; profiling will reveal whether the bottleneck is dict iteration, gate allocation, or the add_gate call itself.

**Primary recommendation:** Fix the loop direction bug first with golden-master verification, then implement binary search with a temporary debug linear-scan fallback, then profile and optimize compile replay.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Golden-master verification: Curated set of 15-20 representative circuits covering different operation types (add, mul, grover, QFT, bitwise, etc.), stored as committed JSON files in the test directory, JSON format with gate type, qubit indices, and layer number, kept permanently as ongoing regression tests
- Performance benchmarks: Both synthetic large circuits (10K+ gates) AND real workloads, any measurable improvement satisfactory, benchmark results stored in docs with tables AND scaling plots, combined end-to-end circuit generation time, test at 100/1K/10K/50K gates, on-demand only, 5 runs median
- Compile replay optimization: Profile-driven plus low-hanging fruit, if dict iteration is bottleneck swap to more efficient data structure, prefer C-level replacement, target wall-clock time and hot-path elimination, up to 5% regression acceptable on non-optimized paths, caching for repeated gate patterns acceptable, internal data structures not public API, profiling infrastructure kept as opt-in
- Binary search implementation: Standard bisect pattern in C, include debug fallback during development, remove debug fallback after golden-master verification
- Rollout order: Plan 1 (PERF-01) fix loop direction bug, Plan 2 (PERF-02) golden-master + binary search, Plan 3 (PERF-03) compile replay optimization -- 3 separate plans independently verified

### Claude's Discretion
- Profiling activation mechanism (env var vs Python flag)
- Which specific compiled functions to profile
- Exact golden-master circuit selection (within the 15-20 curated set guideline)
- Compression algorithm for gate pattern caching (if implemented)
- Specific data structure choice for dict replacement (sorted array, B-tree, etc.)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PERF-01 | Fix optimizer latent loop direction bug (++i should be --i in smallest_layer_below_comp) | Bug confirmed in optimizer.c line 32: `for (int i = last_index; i > 0; ++i)` should be `--i`. Array is `occupied_layers_of_qubit[qubit]` with `used_occupation_indices_per_qubit[qubit]` entries. Fix is single character change but requires golden-master verification. |
| PERF-02 | Replace optimizer linear scan with binary search for gate placement (O(L) to O(log L)) | `occupied_layers_of_qubit[qubit]` stores layer indices in monotonically increasing order (layers are assigned in ascending order via `apply_layer`). Standard lower-bound binary search on this sorted array finds the largest layer < compar. C stdlib `bsearch` is not suitable (returns exact match); implement custom bisect_left. |
| PERF-03 | Reduce @ql.compile replay overhead (profile gate injection, optimize dict iteration) | `inject_remapped_gates` in _core.pyx iterates Python list of dicts, malloc's a `gate_t` per gate, copies fields via Python dict access, then calls `add_gate`. Hot path candidates: dict key lookups (5 per gate), malloc/memset per gate (could use stack allocation), Python-to-C type conversions. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C stdlib (stdlib.h) | C11 | Binary search implementation | Project uses C11 throughout; no external dependencies needed for bisect |
| Python cProfile/timeit | 3.13 | Profiling compile replay | Built-in, zero-dependency profiling for Python-level hotspots |
| pytest-benchmark (optional) | latest | Structured benchmarking | Already in dev dependencies pattern; provides median/percentile stats |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| line_profiler | latest | Line-by-line profiling of inject_remapped_gates | If cProfile shows hot function but not which lines |
| matplotlib | existing | Scaling plots for benchmark results | Already in project dependencies for circuit visualization |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom C bisect | C stdlib bsearch | bsearch finds exact match, not lower-bound; not suitable |
| Python cProfile | py-spy | py-spy shows C extension time but harder to integrate |
| Manual benchmarks | pytest-benchmark | pytest-benchmark adds dependency but gives statistical rigor |

## Architecture Patterns

### Pattern 1: Binary Search for Sorted Layer Arrays
**What:** The `occupied_layers_of_qubit[qubit]` array stores layer indices in monotonically increasing order. Each entry is the layer number where a gate occupies this qubit. The array grows as gates are added via `apply_layer()`.

**When to use:** Whenever searching for the largest layer index below a comparison value.

**Example (C):**
```c
// Binary search: find largest occupied layer < compar
// Returns 0 if no such layer exists
layer_t smallest_layer_below_comp(circuit_t *circ, qubit_t qubit, layer_t compar) {
    int count = (int)circ->used_occupation_indices_per_qubit[qubit];
    if (count <= 0)
        return 0;

    layer_t *arr = circ->occupied_layers_of_qubit[qubit];

    // Binary search for largest value < compar (upper bound - 1)
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] < compar)
            lo = mid + 1;
        else
            hi = mid;
    }
    // lo is now the index of the first element >= compar
    // The largest element < compar is at lo - 1
    if (lo > 0)
        return arr[lo - 1];
    return 0;
}
```

### Pattern 2: Golden-Master Circuit Snapshot
**What:** Capture the complete gate-level output of representative circuits as JSON, commit as test fixtures, and compare after every optimizer change.

**Format:**
```json
{
  "circuit_name": "add_4bit",
  "total_gates": 42,
  "total_layers": 12,
  "gates": [
    {"layer": 0, "type": "X", "target": 3, "controls": [], "angle": 0.0},
    {"layer": 0, "type": "P", "target": 5, "controls": [3], "angle": 1.5707963}
  ]
}
```

### Pattern 3: Debug Fallback Verification
**What:** During binary search development, run both binary search and corrected linear scan on each call, assert results match, remove after golden-master verification passes.

```c
#ifdef OPTIMIZER_DEBUG
    layer_t bs_result = binary_search_layer(circ, qubit, compar);
    layer_t ls_result = linear_scan_layer(circ, qubit, compar);
    assert(bs_result == ls_result);
    return bs_result;
#else
    return binary_search_layer(circ, qubit, compar);
#endif
```

### Anti-Patterns to Avoid
- **Changing gate ordering semantics:** Binary search must return the exact same result as corrected linear scan. The function finds the largest `occupied_layers_of_qubit[qubit][i]` that is `< compar`. Any deviation changes circuit depth and gate ordering.
- **Stack-allocating gate_t in inject_remapped_gates without size check:** `gate_t` contains a fixed `Control[2]` array but MCX gates use `large_control` heap allocation. Cannot simply stack-allocate all gates.
- **Profiling with production circuit sizes:** Profiling adds overhead; use representative but moderate sizes (1K-10K gates) for profiling, then verify scaling at 50K.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Statistical benchmarking | Manual timing loops with averages | 5-run median with explicit reporting | Averages hide outliers; median is robust |
| JSON serialization for golden-masters | Custom format parser | Python json module | Standard, readable, diffable in PRs |
| Binary search correctness | Assume correct by testing edge cases | Debug fallback parallel linear scan | Binary search off-by-one errors are notoriously subtle |

## Common Pitfalls

### Pitfall 1: Binary Search Off-by-One
**What goes wrong:** Binary search returns wrong element (one too high or one too low), causing gates to be placed in wrong layers. This changes circuit semantics for non-commuting gates.
**Why it happens:** The search target is "largest value strictly less than compar" -- this is a non-standard binary search variant (not exact match, not lower_bound of compar, but upper_bound minus one).
**How to avoid:** Use standard lower_bound pattern (find first element >= compar), then take index - 1. Verify with edge cases: empty array, single element, all elements < compar, all elements >= compar, compar equals first/last element.
**Warning signs:** Golden-master diffs showing changed layer assignments.

### Pitfall 2: Loop Direction Bug Fix Changing Circuit Output
**What goes wrong:** The current buggy `++i` loop happens to produce correct results for most circuits because it reads memory past the array but typically finds 0 (from calloc'd memory). Fixing to `--i` might produce different (correct) results for circuits where the past-end memory was not zero.
**Why it happens:** The bug has been present since the original implementation. All existing tests pass with the buggy code, meaning either the bug never triggers in practice OR the tests encode the buggy behavior.
**How to avoid:** Take golden-master snapshots BEFORE the fix, apply fix, compare. If differences appear, verify the new output is correct by manual inspection of small circuits (2-3 qubit add/sub).

### Pitfall 3: inject_remapped_gates Optimization Breaking Controlled Variants
**What goes wrong:** Optimizing the gate injection path (e.g., batch allocation, pre-sorting) breaks the control qubit handling for MCX gates (NumControls > 2) which use heap-allocated `large_control` arrays.
**Why it happens:** The inject path has two branches: `NumControls <= 2` (uses `Control[2]` array) and `NumControls > 2` (malloc's `large_control`). Optimization that assumes fixed-size control arrays will corrupt multi-controlled gates.
**How to avoid:** Any optimization to inject_remapped_gates must handle both branches. Test with circuits containing Toffoli gates (3+ controls) from Grover's algorithm.

### Pitfall 4: Profiling Overhead Masking Real Bottleneck
**What goes wrong:** Profiling the compile replay path shows `inject_remapped_gates` as the bottleneck, but the real bottleneck is `add_gate` (C-level optimizer) which is invisible to Python profilers.
**Why it happens:** Python profilers (cProfile) attribute C extension call time to the Python function that called the extension. The actual time split between Python dict operations and C optimizer logic is hidden.
**How to avoid:** Use wall-clock timing of `inject_remapped_gates` with varying gate counts to measure scaling. If time scales with gate count but Python operations are O(1) per gate, the bottleneck is in C. Use `time.perf_counter_ns` around the inject call with 1K, 5K, 10K gates to see scaling.

## Code Examples

### Current Bug (optimizer.c line 26-38)
```c
layer_t smallest_layer_below_comp(circuit_t *circ, qubit_t qubit, layer_t compar) {
    int last_index = (int)circ->used_occupation_indices_per_qubit[qubit];
    if (last_index < 0)
        return 0;
    // BUG: ++i should be --i (scans forward past array end instead of backward)
    for (int i = last_index; i > 0; ++i) {
        if (circ->occupied_layers_of_qubit[qubit][i - 1] < compar) {
            return circ->occupied_layers_of_qubit[qubit][i - 1];
        }
    }
    return 0;
}
```

The intent is to scan backward from `last_index` to find the largest occupied layer below `compar`. The loop condition `i > 0` with `++i` means i goes from last_index upward indefinitely (reading out of bounds) until it wraps around or finds a stale value in uninitialized memory that happens to be < compar. The fix is to change `++i` to `--i`.

### Corrected Linear Scan
```c
layer_t smallest_layer_below_comp(circuit_t *circ, qubit_t qubit, layer_t compar) {
    int last_index = (int)circ->used_occupation_indices_per_qubit[qubit];
    if (last_index <= 0)
        return 0;
    for (int i = last_index; i > 0; --i) {
        if (circ->occupied_layers_of_qubit[qubit][i - 1] < compar) {
            return circ->occupied_layers_of_qubit[qubit][i - 1];
        }
    }
    return 0;
}
```

### inject_remapped_gates Current Implementation (_core.pyx lines 809-858)
```cython
def inject_remapped_gates(list gates, dict qubit_map):
    circ = <circuit_t*>_circuit
    for gate_dict in gates:
        g = <gate_t*>malloc(sizeof(gate_t))  # Heap alloc per gate
        memset(g, 0, sizeof(gate_t))
        g.Gate = <Standardgate_t>gate_dict['type']      # Dict lookup 1
        g.Target = <qubit_t>qubit_map[gate_dict['target']]  # Dict lookup 2+3
        g.GateValue = gate_dict['angle']                 # Dict lookup 4
        g.NumControls = <unsigned int>gate_dict['num_controls']  # Dict lookup 5
        controls = gate_dict['controls']                 # Dict lookup 6
        # ... control qubit remapping ...
        add_gate(circ, g)  # C optimizer call
```

Key optimization opportunities:
1. **malloc per gate:** Could use stack allocation for gates with <= 2 controls (the common case), only heap-allocate for MCX gates
2. **Dict lookups:** 6 Python dict lookups per gate; could pre-extract keys once or use tuples instead of dicts
3. **add_gate per gate:** Cannot be batched (each gate placement depends on previous), but the gate_t could be reused instead of malloc'd fresh each time

### Golden-Master Snapshot Generation (Python)
```python
def capture_golden_master(name, circuit_func, *args):
    """Capture circuit output as golden-master JSON."""
    import json
    ql.circuit()
    circuit_func(*args)
    gates = extract_gate_range(0, get_current_layer())
    snapshot = {
        "circuit_name": name,
        "total_gates": len(gates),
        "gates": gates  # Already list of dicts with type/target/controls/angle
    }
    with open(f"tests/golden_masters/{name}.json", "w") as f:
        json.dump(snapshot, f, indent=2)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Linear scan in optimizer | Still linear (with bug) | Never upgraded | O(L) per gate placement |
| malloc per gate in inject | Still malloc per gate | Phase 61 removed malloc in colliding_gates | Gate injection unchanged |
| No compile caching | OrderedDict LRU cache | Phase 48 (v2.0) | Replay avoids re-execution |

**Deprecated/outdated:**
- The `++i` loop in `smallest_layer_below_comp` is buggy since initial implementation, never caught because circuits are typically small enough that out-of-bounds reads hit zeroed memory

## Open Questions

1. **Memory layout of occupied_layers_of_qubit after qubit deallocation**
   - What we know: When qubits are deallocated (`_deallocate_qubits`), the `used_occupation_indices_per_qubit` counter is NOT decremented. The occupied layers array retains stale entries.
   - What's unclear: Whether binary search must handle stale entries (non-monotonic after deallocation?)
   - Recommendation: Verify monotonicity invariant holds by adding assertion during development. If broken, binary search needs to operate only on the first `used_occupation_indices_per_qubit[qubit]` entries which should still be monotonic since they were added in order.

2. **Actual performance impact of inject_remapped_gates optimization**
   - What we know: The function is called once per replay with N gates. Each call does N mallocs + N dict iterations.
   - What's unclear: Whether the bottleneck is Python overhead or C optimizer time
   - Recommendation: Profile first (PERF-03), then optimize based on data

## Sources

### Primary (HIGH confidence)
- `c_backend/src/optimizer.c` -- Direct code inspection, bug confirmed at line 32
- `c_backend/include/circuit.h` -- Data structure definitions for circuit_t
- `c_backend/include/types.h` -- Type definitions (layer_t = size_t, qubit_t = unsigned int)
- `src/quantum_language/compile.py` -- Full compile decorator implementation
- `src/quantum_language/_core.pyx` lines 809-858 -- inject_remapped_gates implementation
- `c_backend/src/circuit_allocations.c` -- Memory layout of occupied_layers arrays

### Secondary (MEDIUM confidence)
- `.planning/research/PITFALLS-QUALITY-EFFICIENCY.md` -- Prior research on optimizer refactoring risks (Pitfall 4)
- `.planning/codebase/CONCERNS.md` -- Known issues catalog including optimizer bug

## Metadata

**Confidence breakdown:**
- Bug fix (PERF-01): HIGH -- Bug confirmed by direct code inspection, single character fix
- Binary search (PERF-02): HIGH -- Data structure properties verified (sorted array), standard algorithm
- Compile replay (PERF-03): MEDIUM -- Profiling needed before optimization decisions; candidates identified but bottleneck not yet measured
- Golden-master approach: HIGH -- Standard regression testing pattern, well-suited to this codebase

**Research date:** 2026-02-23
**Valid until:** 2026-03-23 (stable domain, no external dependencies changing)
