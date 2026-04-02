# PRD: Fix ASCII Circuit Visualization for Overlapping Gates

## Goal

When `qc_circuit_visualize()` renders a layer containing gates whose qubit ranges overlap (e.g., an X on qubit 1 and a CX from qubit 0 to qubit 2), the vertical wire `|` of the multi-qubit gate overwrites the single-qubit gate's symbol. The fix splits overlapping gates within a layer into separate visual sub-columns so that every gate is visible.

## Motivation

The ASCII visualization is the primary debugging tool for circuit authors. When multi-qubit gates with overlapping qubit ranges share a layer, the vertical `|` wires become ambiguous — you cannot tell which gate they belong to, and single-qubit gates on intermediate qubits visually appear "inside" an unrelated multi-qubit gate's wire. This is especially common in QFT-based arithmetic (e.g., `qint <= int` comparisons) where controlled phase gates span wide qubit ranges while other gates operate on intermediate qubits in the same layer. The gate symbols themselves render correctly via the spatial index, but the visual layout is confusing.

## Requirements

### R1: Overlapping gates render in separate sub-columns
When two or more gates in the same layer have intersecting qubit ranges `[qc_min_qubit(g), qc_max_qubit(g)]`, they must be placed into separate visual sub-columns. Each sub-column is printed as its own column in the output, adjacent to each other under the same layer header.

### R2: Non-overlapping gates share a sub-column
Gates in the same layer whose qubit ranges do not intersect must share a single sub-column, preserving current compactness for non-conflicting layouts.

### R3: No modification to stored circuit data
The fix is purely in the rendering path of `qc_circuit_visualize()`. The circuit's `sequence`, `gate_index_of_layer_and_qubits`, layer counts, gate counts, and all other internal state must remain untouched. No new fields are added to `circuit_ctx`.

### R4: Backward-compatible output for non-overlapping circuits
Circuits where no layer has overlapping gates must produce identical output to the current implementation.

### R5: Bounded memory usage
The sub-column grouping must use stack or temporary heap allocation proportional to the number of gates in a single layer (typically small, bounded by `QC_GATES_PER_LAYER_BLOCK`). No persistent allocations.

### R6: Layer header alignment
When a layer expands into multiple sub-columns, the layer number in the header row should appear once and the sub-columns should be visually grouped (e.g., extra width under the same layer label).

## Success Criteria

1. A test circuit with X on qubit 1 and CX(0, 2) in the same layer displays both the `X` and the `+`/`@` symbols, each in its own sub-column.
2. A test circuit with X on qubit 0 and X on qubit 1 in the same layer displays them in a single column (no unnecessary splitting).
3. A CCX(0, 2, 4) alongside H on qubit 1 and Z on qubit 3 in the same layer renders all three gates visibly (H and Z share one sub-column, CCX in another).
4. Existing test circuits produce unchanged output when they have no overlapping gates.
5. No changes to the public API signature of `qc_circuit_visualize()`.
6. No changes to any other public or internal function.

## Out of Scope

- Changing the visualization from `printf` to a string-returning API.
- Adding color or Unicode box-drawing characters.
- Modifying how gates are stored in layers (the scheduler/layer-assignment logic is untouched).
- Changes to `qc_circuit_to_qasm()` or any other output function.
- Changes to sibling packages.
- Performance optimization of the visualization (it is already bounded to 60 displayed layers).

## Key Design Decisions

### D1: Interval graph coloring for sub-column assignment
Each gate in a layer occupies the qubit interval `[qc_min_qubit(g), qc_max_qubit(g)]`. Gates whose intervals overlap must go into different sub-columns. This is an interval graph coloring problem. A greedy left-to-right sweep (sort gates by min qubit, assign first non-conflicting sub-column) produces an optimal coloring in O(n log n) time, where n is the number of gates in the layer.

### D2: Pre-pass over layers before rendering
Before the qubit-row rendering loop, iterate over all displayed layers and compute the sub-column assignment for each. Store results in a temporary array indexed by `[layer][gate_index] -> sub_column_id`, plus `sub_column_count[layer]`.

### D3: Render loop iterates sub-columns within layers
The inner rendering loop changes from iterating layers to iterating `(layer, sub_column)` pairs. For each sub-column, the existing gate-lookup and between-wire logic applies, but restricted to gates assigned to that sub-column.

### D4: Single-qubit gates have degenerate intervals
A single-qubit gate (no controls) has `min_qubit == max_qubit == target`. It only overlaps with a multi-qubit gate if its target falls within that gate's range. Two single-qubit gates on different qubits never overlap and always share a sub-column.
