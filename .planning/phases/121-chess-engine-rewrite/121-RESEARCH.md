# Phase 121: Chess Engine Rewrite - Research

**Researched:** 2026-03-10
**Domain:** Quantum chess engine example + compile infrastructure (DAG-only opt=1 replay)
**Confidence:** HIGH

## Summary

Phase 121 is a two-part phase: (1) rewrite `examples/chess_engine.py` as a readable quantum chess engine demonstrating v9.0 features (nested `with` blocks, 2D qarrays, `@ql.compile`), and (2) fix the opt=1 compile behavior so replay records a DAG node instead of flat-expanding all gates (which causes OOM). Both parts are tightly coupled -- the chess engine cannot compile without the opt=1 fix.

The existing `examples/chess_engine.py` is a draft with several bugs (wrong board variable on line 39, broken unpacking syntax on line 54/56, incorrect unmake logic on line 80). The `chess_encoding.py` module in `src/` provides classical move generation utilities (knight/king attack tables, legal move filtering, ASCII board display) that can be reused for move tables. The framework already has all quantum primitives needed: 2D qarrays (Phase 120), nested `with` blocks (Phase 117-118), `@ql.compile` with DAG tracking (Phase 107-111), and `ql.diffusion()`.

**Primary recommendation:** Split into two plans: Plan 01 implements the opt=1 DAG-only replay fix in compile.py and circuit_stats DAG-aware path in _core.pyx; Plan 02 rewrites the chess engine example using the fixed infrastructure. The compile fix is prerequisite for the engine to work without OOM.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- King + 2 Knights (white) vs King (black) endgame on 8x8 board
- Hardcoded initial position (specific squares baked into example)
- Black to move only (1 ply) -- 2-ply is a future phase
- Goal: build the quantum walk search tree, not evaluate or count yet
- 1-ply tree: root -> 8 possible king moves (one per direction)
- Fixed 8-direction index: 3-bit branch register encoding N/NE/E/SE/S/SW/W/NW
- Feasibility register: `ql.qarray(dim=8, dtype=ql.qbool)` -- one qbool per direction
- Height register: one-hot 2-qubit qarray (height[0]=leaf, height[1]=root)
- Walk operators (R_A/R_B) written inline using `ql.diffusion()` and `with` blocks -- NOT using QWalkTree class
- Diffusion over all 8 branches with feasibility masking
- Walk step compiled with `@ql.compile(opt=1)` for replay
- Configurable iteration count: `NUM_STEPS = 3` variable
- Full legality check: piece-exists at source, no friendly capture, not in check after move
- Check detection: scan all 64 squares for white attackers
- White attack map: `ql.qarray(dim=(8,8), dtype=ql.qbool)`
- Friendly-capture: combined white occupancy board, check `~white_occ[to_r, to_c]`
- Black representation: `black_king` board only
- Attack computation: compiled sub-predicate `@ql.compile(opt=1, inverse=True)`
- Uncompute attacks after each direction check
- opt=1 fix: replay skips `inject_remapped_gates()`, only records DAG node with block reference
- First call (capture) still injects gates; subsequent replays are DAG-only
- Call graph is operation-level DAG (each node = one quantum operation)
- `circuit_stats()` reads from DAG (sum gate counts, compute depth, sum T-count)
- Opt level semantics: opt=0 flat, opt=1 call graph no expansion (NEW), opt=2 call graph + merge, opt=3 no DAG
- Reuse and extend existing `CallGraphDAG`
- Out of scope: `to_openqasm()` and `draw_circuit()` from DAG
- Incremental refactor of existing `examples/chess_engine.py`
- Single file with clear sections: (1) move tables, (2) board setup, (3) compiled predicates, (4) walk operators, (5) main execution
- Section header comments plus brief comments on key quantum decisions
- Brief module docstring (2-3 sentences)
- Output when run: position display (ASCII), section progress messages, circuit stats
- No circuit diagram output
- Chess notation in comments alongside array indices
- XOR with `ql.qbool(True)` for placing pieces
- Piece boards: one `ql.qarray(dim=(8,8), dtype=ql.qbool)` per piece type

### Claude's Discretion
- Exact walk step implementation details within the decided structure
- Error handling and edge cases
- Internal helper function organization
- gc.collect() placement if needed alongside the compile fix

### Deferred Ideas (OUT OF SCOPE)
- 2-ply search (black moves, white responds) -- future phase
- Position evaluation / move counting via `ql.count_solutions` or `ql.grover` -- future phase
- Detection step (checking if walk found a marked node) -- future phase
- `to_openqasm()` from DAG -- future enhancement
- `draw_circuit()` from DAG with block boundaries -- future enhancement
- Sliding piece attack generation (rook/bishop ray-casting) -- future phase
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CHESS-01 | Chess engine in `examples/chess_engine.py` follows readable natural-programming format | Existing draft has bugs; rewrite with section-based structure, nested `with` blocks, 2D qarrays, chess notation comments |
| CHESS-02 | Engine compiles successfully with `@ql.compile(opt=1)` without OOM | Requires opt=1 DAG-only replay fix in compile.py; current replay calls `inject_remapped_gates()` which flat-expands all gates |
| CHESS-03 | Engine includes move legality checking (piece-exists, no-friendly-capture, check detection) | Use nested `with` blocks for conditionals, compiled sub-predicates for attack computation, uncompute after each direction |
| CHESS-04 | Engine includes walk operators (R_A/R_B) and diffusion in readable style | `ql.diffusion()` exists and works on qarrays; inline walk operators using `with` blocks instead of QWalkTree |
| CHESS-05 | Engine is circuit-build-only (no simulation required) | `circuit_stats()` needs DAG-aware path; current implementation reads only flat C-backend allocation stats |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| quantum_language | 0.1.0 | Quantum DSL framework | Project's own framework -- all quantum ops through this |
| `@ql.compile(opt=1)` | current | DAG-only compile | Prevents OOM by not expanding gates on replay |
| `ql.qarray(dim=(8,8), dtype=ql.qbool)` | Phase 120 | 2D board representation | Native framework construct for chess boards |
| `ql.diffusion()` | current | Grover diffusion operator | X-MCZ-X pattern, works on qint/qbool/qarray |
| `CallGraphDAG` | current | Operation-level DAG | Already tracks nodes with qubit sets, gate counts, depth |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `chess_encoding.py` | v6.1 | Classical move tables | Reuse `potential_king_moves`/`potential_knight_moves` patterns |
| `itertools` | stdlib | Board iteration | `itertools.product(range(8), range(8))` for 64-square scans |
| `gc` | stdlib | Memory management | `gc.collect()` after uncomputation to free ancilla memory |

## Architecture Patterns

### Recommended File Structure (chess_engine.py)
```
examples/chess_engine.py
├── Module docstring (2-3 sentences)
├── === Configuration ===
│   └── NUM_STEPS, piece positions
├── === Move Tables ===
│   └── Direction offsets, DIRECTIONS list
├── === Board Setup ===
│   └── ql.circuit(), piece boards, position display
├── === Compiled Predicates ===
│   └── @ql.compile(opt=1, inverse=True) attack computation
│   └── Legality checking helpers
├── === Walk Operators ===
│   └── R_A (make/check/unmake per direction)
│   └── R_B (diffusion with feasibility masking)
│   └── @ql.compile(opt=1) walk step
├── === Main Execution ===
│   └── Walk loop, circuit stats output
```

### Pattern 1: Board Initialization via XOR
**What:** Place pieces on 2D qbool boards using XOR with `ql.qbool(True)`
**When to use:** All piece placement at circuit setup
**Example:**
```python
# Source: CONTEXT.md decision + framework API
white_king = ql.qarray(dim=(8, 8), dtype=ql.qbool)
white_king[0, 4] ^= ql.qbool(True)  # e1

white_knight = ql.qarray(dim=(8, 8), dtype=ql.qbool)
white_knight[1, 1] ^= ql.qbool(True)  # b2
white_knight[2, 5] ^= ql.qbool(True)  # f3

black_king = ql.qarray(dim=(8, 8), dtype=ql.qbool)
black_king[6, 4] ^= ql.qbool(True)  # e7
```

### Pattern 2: Nested With-Block Legality Check
**What:** Multi-condition quantum conditional using nested `with` blocks
**When to use:** Check piece-exists AND no-friendly-capture AND not-in-check
**Example:**
```python
# Source: Phase 117-118 nested with-block implementation
with black_king[from_r, from_c]:           # piece exists at source
    with ~white_occ[to_r, to_c]:           # no friendly capture
        # make move
        black_king[from_r, from_c] ^= ql.qbool(True)
        black_king[to_r, to_c] ^= ql.qbool(True)
        # check detection ...
        with ~in_check:
            feasibility[direction] ^= ql.qbool(True)  # mark legal
        # unmake move
        black_king[to_r, to_c] ^= ql.qbool(True)
        black_king[from_r, from_c] ^= ql.qbool(True)
```

### Pattern 3: Compiled Attack Computation with Uncomputation
**What:** Attack map built as compiled sub-predicate with automatic inverse
**When to use:** White attack computation for check detection
**Example:**
```python
# Source: CONTEXT.md decision on sub-predicates
@ql.compile(opt=1, inverse=True)
def compute_white_attacks(white_knight, white_king, attack_map):
    for r, c in itertools.product(range(8), range(8)):
        with white_knight[r, c]:
            for nr, nc in knight_targets(r, c):
                attack_map[nr, nc] ^= ql.qbool(True)
        with white_king[r, c]:
            for nr, nc in king_targets(r, c):
                attack_map[nr, nc] ^= ql.qbool(True)
    return attack_map

# Usage: compute, check, then uncompute
attack_map = compute_white_attacks(white_knight, white_king, attack_map)
in_check = attack_map[bk_to_r, bk_to_c]
# ... use in_check ...
compute_white_attacks.inverse(white_knight, white_king, attack_map)
```

### Pattern 4: opt=1 DAG-Only Replay
**What:** On replay (cache hit), skip `inject_remapped_gates()` and only record DAG node
**When to use:** All `@ql.compile(opt=1)` replay paths
**Key change in compile.py:**
```python
# In _replay() method, around line 1491:
if self._opt == 1 and is_replay:
    # DAG-only: skip gate injection, just record node
    # (allocate ancillas for qubit tracking but don't inject gates)
    pass
else:
    inject_remapped_gates(block.gates, virtual_to_real)
```

### Pattern 5: DAG-Aware circuit_stats()
**What:** `circuit_stats()` checks for active DAG and returns aggregate metrics from DAG nodes
**When to use:** When the chess engine calls `ql.circuit_stats()` for output
**Key concept:** The top-level compiled function stores `self._call_graph` (a `CallGraphDAG`). The stats come from `dag.aggregate()` which sums gate counts, computes critical-path depth, counts unique qubits, and sums T-counts.

### Anti-Patterns to Avoid
- **Manual gate manipulation:** Never use `emit_x`, `emit_ry`, etc. -- the chess engine uses only the DSL (arithmetic, comparisons, `with` blocks)
- **Using QWalkTree class:** Walk operators are inline with `ql.diffusion()` and `with` blocks per CONTEXT decision
- **Flat circuit for opt=1 replay:** The whole point of the fix is that opt=1 replays do NOT inject gates
- **Single monolithic compiled function:** The attack computation must be a separate compiled sub-predicate to avoid excessive ancilla accumulation
- **Simulating the circuit:** This is build-only; 8x8 boards use 192+ qubits, far beyond the 17-qubit simulation limit

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Diffusion operator | Custom X-MCZ-X sequence | `ql.diffusion(*registers)` | Already compiled with key caching, handles qarray flattening |
| Move tables (classical) | New move generation code | Adapt patterns from `chess_encoding.py` | Existing `_KNIGHT_OFFSETS`, `_KING_OFFSETS`, boundary checking are correct |
| DAG structure | New graph data structure | Extend existing `CallGraphDAG` | Already has rustworkx backing, overlap edges, parallel groups, aggregate metrics |
| Board printing | Custom board display | Adapt `chess_encoding.print_position()` pattern | Proven ASCII board display logic |
| Gate statistics | Manual gate counting | `CallGraphDAG.aggregate()` | Already computes gates, depth, qubits, t_count from DAG nodes |

**Key insight:** The framework and chess_encoding already provide 80% of what's needed. The novel work is (a) the opt=1 DAG-only replay fix, (b) wiring the chess logic into the quantum framework using nested `with` blocks, and (c) making `circuit_stats()` DAG-aware.

## Common Pitfalls

### Pitfall 1: OOM from Gate Flat-Expansion on Replay
**What goes wrong:** `inject_remapped_gates()` copies all captured gates into the flat circuit on every replay. With chess engine doing thousands of compiled calls, this exhausts memory.
**Why it happens:** Current opt=1 records a DAG node AND injects gates. The DAG recording is correct but the injection is redundant for the purpose of stats.
**How to avoid:** Make opt=1 replay skip `inject_remapped_gates()` entirely. The DAG node already stores gate count, depth, and T-count.
**Warning signs:** Process killed by OOM, memory usage grows linearly with number of compiled replays.

### Pitfall 2: Wrong Board Variable in Existing Chess Engine
**What goes wrong:** Line 39 of existing `chess_engine.py` writes to `white_king[6, 4]` instead of `black_king[6, 4]`.
**Why it happens:** Copy-paste error during initial draft.
**How to avoid:** During rewrite, carefully verify all piece placement lines match the intended piece board.
**Warning signs:** Black king has no pieces placed; white king has two entries.

### Pitfall 3: Unmake Move Ordering
**What goes wrong:** Existing line 80 uses `+= 1` (same as make) instead of toggling back (XOR reversal).
**Why it happens:** XOR-based toggle (`^= ql.qbool(True)`) is self-inverse, so the same operation makes and unmakes. But `+= 1` is not self-inverse; `+= 1` followed by `+= 1` gives `+2`, not `0`.
**How to avoid:** Use XOR for all board state changes: `board[r, c] ^= ql.qbool(True)` for both make and unmake.
**Warning signs:** Board state not restored to original after unmake, causing cascading corruption.

### Pitfall 4: Ancilla Qubit Accumulation Without Uncomputation
**What goes wrong:** Each direction check allocates ancilla qubits for the attack map. Without uncomputing, qubits accumulate across 8 directions.
**Why it happens:** Attack computation creates intermediate qubits; failing to call `.inverse()` leaves them allocated.
**How to avoid:** After checking `in_check` for each direction, call `compute_white_attacks.inverse()` to free ancillas. Also consider `gc.collect()` periodically.
**Warning signs:** Qubit count grows with each direction check instead of staying constant.

### Pitfall 5: circuit_stats() Returns Allocation Stats, Not Gate Stats
**What goes wrong:** The current `circuit_stats()` in `_core.pyx` returns qubit allocation statistics (peak_allocated, total_allocations, etc.), not gate counts or circuit depth.
**Why it happens:** `circuit_stats()` was designed for qubit allocation tracking, not gate-level metrics.
**How to avoid:** For CHESS-05, either (a) add a new function that reads from `CallGraphDAG.aggregate()`, or (b) modify `circuit_stats()` to also return gate-level metrics from the DAG. The DAG's `aggregate()` already provides `{'gates': N, 'depth': D, 'qubits': Q, 't_count': T}`.
**Warning signs:** Circuit stats output shows allocation counts instead of gate counts.

### Pitfall 6: Star-Unpacking in with Statements
**What goes wrong:** The existing draft uses `black_king[*index]` which is invalid Python for `__getitem__`/`__setitem__` -- star-unpacking in subscript position is not allowed.
**Why it happens:** Confusion with function call `*args` unpacking.
**How to avoid:** Use `black_king[index[0], index[1]]` or unpack the tuple first: `r, c = index; black_king[r, c]`.
**Warning signs:** `SyntaxError` at runtime.

### Pitfall 7: Compile Replay with Nested With-Blocks and Controls
**What goes wrong:** When a compiled function is replayed inside a `with` block, the replay must correctly compose the outer control with the replayed gates.
**Why it happens:** The replay path reads the current control stack and maps the control placeholder. If the compiled function was captured without controls but replayed inside a `with` block, the control composition must be handled.
**How to avoid:** Phase 119 already verified this works. But be aware: compiled inverse functions inside nested `with` blocks have documented pre-existing issues (skipped tests in Phase 119).
**Warning signs:** Incorrect gate emission inside nested compiled calls. If encountered, document as known limitation.

## Code Examples

### Existing Bug Fixes Needed in chess_engine.py

```python
# BUG 1: Line 39 -- wrong board variable
# WRONG:  white_king[6, 4] += 1
# RIGHT:  black_king[6, 4] ^= ql.qbool(True)  # e7

# BUG 2: Lines 54/56 -- star unpacking in subscript
# WRONG:  with black_king[*index] == 1:
#         black_king[*index] -= 1
# RIGHT:  r, c = 6, 4  # known position
#         with black_king[r, c]:

# BUG 3: Line 80 -- unmake uses += instead of toggle
# WRONG:  black_king[pp, np] += 1
# RIGHT:  black_king[to_r, to_c] ^= ql.qbool(True)
```

### Direction Offset Table
```python
# 8 king move directions: N, NE, E, SE, S, SW, W, NW
# Index = 3-bit branch register value
DIRECTIONS = [
    (-1,  0),  # 0: N  (South in chess, decreasing rank)
    (-1,  1),  # 1: NE
    ( 0,  1),  # 2: E
    ( 1,  1),  # 3: SE
    ( 1,  0),  # 4: S
    ( 1, -1),  # 5: SW
    ( 0, -1),  # 6: W
    (-1, -1),  # 7: NW
]
```

### Walk Step Structure (Conceptual)
```python
@ql.compile(opt=1)
def walk_step(black_king, white_knight, white_king, branch, feasibility, height):
    """One step of quantum walk: evaluate -> diffuse -> apply."""
    # Phase 1: Evaluate all 8 directions
    for d, (dr, dc) in enumerate(DIRECTIONS):
        to_r, to_c = bk_r + dr, bk_c + dc
        if not (0 <= to_r < 8 and 0 <= to_c < 8):
            continue  # off-board: feasibility stays False

        # Make move
        black_king[bk_r, bk_c] ^= ql.qbool(True)
        black_king[to_r, to_c] ^= ql.qbool(True)

        # Check legality (no friendly capture + not in check)
        attack_map = ql.qarray(dim=(8, 8), dtype=ql.qbool)
        compute_white_attacks(white_knight, white_king, attack_map)
        in_check = attack_map[to_r, to_c]
        with ~in_check:
            feasibility[d] ^= ql.qbool(True)
        compute_white_attacks.inverse(white_knight, white_king, attack_map)

        # Unmake move
        black_king[to_r, to_c] ^= ql.qbool(True)
        black_king[bk_r, bk_c] ^= ql.qbool(True)

    # Phase 2: Diffusion conditioned on feasibility
    with height[0]:  # at leaf level
        ql.diffusion(branch)

    # Phase 3: Controlled move application (conditioned on branch)
    for d, (dr, dc) in enumerate(DIRECTIONS):
        branch_match = (branch == d)
        with branch_match:
            with feasibility[d]:
                # apply selected move
                to_r, to_c = bk_r + dr, bk_c + dc
                if 0 <= to_r < 8 and 0 <= to_c < 8:
                    black_king[bk_r, bk_c] ^= ql.qbool(True)
                    black_king[to_r, to_c] ^= ql.qbool(True)

    return black_king, feasibility
```

### opt=1 DAG-Only Replay Fix (compile.py)
```python
# In _replay() method, key modification:
def _replay(self, block, quantum_args, track_forward=True):
    """Replay cached gates with qubit remapping."""
    virtual_to_real = {}
    vidx = 0
    for qa in quantum_args:
        indices = _get_quantum_arg_qubit_indices(qa)
        for real_q in indices:
            virtual_to_real[vidx] = real_q
            vidx += 1

    # ... existing validation and ancilla allocation ...

    saved_floor = _get_layer_floor()
    start_layer = get_current_layer()
    _set_layer_floor(start_layer)

    # NEW: opt=1 skips gate injection on replay
    if self._opt != 1 or not hasattr(self, '_has_captured'):
        inject_remapped_gates(block.gates, virtual_to_real)
    # If opt=1 and this is a replay, the DAG node recording
    # (done in _call_inner) is sufficient

    end_layer = get_current_layer()
    _set_layer_floor(saved_floor)
    # ... rest of _replay unchanged ...
```

### DAG-Aware Stats Output
```python
# In chess_engine.py main section:
# After walk loop completes
if hasattr(walk_step, '_call_graph') and walk_step._call_graph is not None:
    dag = walk_step._call_graph
    agg = dag.aggregate()
    print(f"\n=== Circuit Statistics ===")
    print(f"Total gates:  {agg['gates']:,}")
    print(f"Circuit depth: {agg['depth']:,}")
    print(f"Total qubits: {agg['qubits']:,}")
    print(f"T-count:      {agg['t_count']:,}")
    print(f"DAG nodes:    {dag.node_count}")
    print(dag.report())  # Formatted table
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Flat gate list for all opt levels | opt=1 still flat-expands on replay | Phase 107-111 (v7.0) | OOM for large circuits -- Phase 121 fixes this |
| Single-level `with` blocks only | Nested `with` blocks via AND-ancilla | Phase 117-118 (v9.0) | Enables multi-condition legality checks |
| 1D qarrays only | 2D qarrays via `dim=(rows, cols)` | Phase 120 (v9.0) | Native 8x8 board representation |
| chess_encoding.py classical only | Quantum board + walk tree | Phase 103-106 (v6.1) -> Phase 121 | Full quantum walk over chess moves |

**Key framework capabilities verified:**
- `ql.qarray(dim=(8,8), dtype=ql.qbool)` -- creates 64-element flat array with (8,8) shape (HIGH confidence, read qarray.pyx)
- `arr[r, c]` indexing -- returns single qbool element for 2D arrays (HIGH confidence, `_handle_multi_index` in qarray.pyx)
- `arr[r, c] ^= ql.qbool(True)` -- in-place XOR on element via `__ixor__` delegation (HIGH confidence, `_inplace_binary_op` in qarray.pyx)
- `with qbool:` nesting -- AND-ancilla composition at arbitrary depth (HIGH confidence, Phase 117-118 tests pass)
- `@ql.compile(opt=1)` -- builds DAG on first call, records nodes on replay (HIGH confidence, read compile.py)
- `CallGraphDAG.aggregate()` -- returns `{gates, depth, qubits, t_count}` (HIGH confidence, read call_graph.py)

## Integration Points

### compile.py Modifications
| Location | Current Behavior | New Behavior | Lines |
|----------|------------------|-------------|-------|
| `_replay()` line ~1491 | Always calls `inject_remapped_gates()` | Skip when `self._opt == 1` and replaying (cache hit) | compile.py:1447-1525 |
| `_call_inner()` cache hit path | Calls `_replay()` then records DAG node | Same DAG recording, but `_replay()` now conditionally skips injection | compile.py:894-923 |

### _core.pyx Modifications
| Location | Current Behavior | New Behavior |
|----------|------------------|-------------|
| `circuit_stats()` line 757 | Returns qubit allocation stats only | Also check for DAG on last top-level compiled function and include gate stats |

**Alternative approach for stats:** Instead of modifying the C-level `circuit_stats()`, the chess engine could directly access `walk_step._call_graph.aggregate()` in Python. This avoids touching _core.pyx and is simpler. The CONTEXT says "circuit_stats() reads from DAG" but the practical outcome -- getting stats printed -- can be achieved either way.

### Ancilla Lifecycle
- Each direction check: allocates attack map (64 qbools = 64 qubits), plus ancillas for comparisons
- `.inverse()` call: deallocates attack map qubits
- Between directions: `gc.collect()` advisable to flush deferred destructors
- Total qubit budget (rough estimate): 3 piece boards (192) + branch (3) + feasibility (8) + height (2) + attack map (64 transient) + comparison ancillas (~20 transient) = ~225 peak qubits
- This is fine for circuit BUILD (no 17-qubit limit on build-only)

## Open Questions

1. **How should the top-level script access DAG stats?**
   - What we know: `CallGraphDAG.aggregate()` exists and works. The DAG is stored on `compiled_func._call_graph`.
   - What's unclear: Should `ql.circuit_stats()` be extended, or should the chess engine just access `_call_graph` directly?
   - Recommendation: Access `_call_graph` directly in the chess engine (simpler, no _core.pyx changes). Add a public `ql.compile` accessor if time permits.

2. **Does the opt=1 fix affect existing tests?**
   - What we know: The test_compile.py suite already has 14-15 pre-existing failures and OOM issues. The opt_level fixture parametrizes all tests over opt=1,2,3.
   - What's unclear: How many tests depend on opt=1 producing a flat circuit?
   - Recommendation: The fix should be guarded so that opt=1 capture (first call) still injects gates normally. Only replay (cache hit) changes. This minimizes test impact since tests typically call a function once or twice.

3. **Exact mechanism for skipping injection in _replay**
   - What we know: `_replay()` is called from `_call_inner()` on cache hit (line 897). The DAG node is recorded after `_replay()` returns (lines 899-923).
   - What's unclear: Whether ancilla allocation in `_replay()` can be skipped too, or if it's needed for qubit tracking in the DAG.
   - Recommendation: Still allocate ancillas (needed for correct qubit set in DAG node), but skip `inject_remapped_gates()`. The ancillas won't consume memory since no gates reference them in the flat circuit.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | pytest |
| Config file | tests/python/conftest.py |
| Quick run command | `pytest tests/python/test_chess.py -x -v` |
| Full suite command | `pytest tests/python/ -v --timeout=300` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CHESS-01 | Chess engine reads like pseudocode | smoke | `python examples/chess_engine.py` (runs without error) | chess_engine.py exists but broken |
| CHESS-02 | Compiles with opt=1 without OOM | smoke | `python examples/chess_engine.py` (completes without kill) | depends on opt=1 fix |
| CHESS-03 | Move legality (piece-exists, no capture, check) | unit | `pytest tests/python/test_chess.py -x -v` | exists (classical tests) |
| CHESS-04 | Walk operators R_A/R_B and diffusion present | smoke | `python examples/chess_engine.py` (prints stats) | depends on engine rewrite |
| CHESS-05 | Circuit-build-only, produces stats | smoke | `python examples/chess_engine.py` (prints gate count/depth/qubits) | depends on DAG stats |

### Sampling Rate
- **Per task commit:** `python examples/chess_engine.py` (smoke test)
- **Per wave merge:** `pytest tests/python/test_chess.py tests/python/test_compile_nested_with.py tests/python/test_call_graph.py -v`
- **Phase gate:** Full suite green (excluding known pre-existing failures) before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/python/test_compile_dag_only.py` -- test that opt=1 replay does not inject gates (new)
- [ ] Smoke test: `python examples/chess_engine.py` runs to completion with stats output
- [ ] No new test framework install needed (pytest already configured)

## Sources

### Primary (HIGH confidence)
- `src/quantum_language/compile.py` -- read full file (~1900 lines), verified replay path, DAG building, opt level handling
- `src/quantum_language/call_graph.py` -- read full file (512 lines), verified CallGraphDAG.aggregate(), DAGNode attributes, builder stack
- `src/quantum_language/diffusion.py` -- read full file (119 lines), verified X-MCZ-X pattern, qarray support
- `src/quantum_language/qarray.pyx` -- read full file (1021 lines), verified 2D construction, __getitem__, __setitem__, __ixor__
- `src/quantum_language/_core.pyx:757` -- verified circuit_stats() returns allocation stats only
- `examples/chess_engine.py` -- read full file (84 lines), identified 3 bugs
- `src/chess_encoding.py` -- read full file (361 lines), verified move generation utilities
- `.planning/debug/test-compile-oom.md` -- verified OOM root cause analysis

### Secondary (MEDIUM confidence)
- `tests/python/test_chess.py` -- verified classical chess encoding tests exist and pass
- `tests/python/test_nested_with_blocks.py` -- verified nested with-block test patterns
- Phase 117-120 decisions in STATE.md -- verified nested controls and 2D qarrays are complete

### Tertiary (LOW confidence)
- Exact qubit count estimate (~225 peak) -- based on manual counting, may vary with framework overhead
- Whether `.inverse()` on compiled functions works correctly inside nested `with` blocks -- Phase 119 documented pre-existing issues with skipped tests

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all components read and verified in source code
- Architecture: HIGH -- CONTEXT.md is extremely detailed with locked decisions; code paths verified
- Pitfalls: HIGH -- identified from reading existing buggy code and OOM debug doc
- Compile fix: MEDIUM -- the concept is clear but exact implementation details for skipping injection while maintaining correct return values needs careful implementation

**Research date:** 2026-03-10
**Valid until:** 2026-04-10 (project-specific, stable within milestone)
