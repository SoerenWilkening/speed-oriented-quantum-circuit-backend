#!/usr/bin/env python3
"""Quantum chess walk demo -- KNK depth-2 framework showcase.

Demonstrates a quantum walk on a simplified chess endgame position
(white king + white knight vs black king) using natural programming
style for quantum algorithms. All quantum logic uses standard ql
constructs -- no raw gate emission.

Circuit-build-only: no Qiskit simulation is performed (~150+ qubits
exceeds the 17-qubit simulation budget).

Usage:
    python demo.py [--visualize]
"""

import argparse
import time

import quantum_language as ql
from chess_encoding import (
    build_move_table,
    encode_position,
    print_position,
)
from chess_walk import (
    create_branch_registers,
    create_height_register,
    prepare_walk_data,
    walk_step,
)

# ---------------------------------------------------------------------------
# Position constants
# ---------------------------------------------------------------------------
WK_SQ = 28  # e4
BK_SQ = 60  # e8
WN_SQUARES = [18]  # c3
MAX_DEPTH = 2  # white move + black response


def main(visualize=False):
    """Run the quantum chess walk demo.

    Builds a quantum circuit for a KNK depth-2 walk and reports
    circuit statistics. No simulation is performed.

    Parameters
    ----------
    visualize : bool
        If True, save a circuit diagram to chess_circuit.png.

    Returns
    -------
    dict
        Circuit statistics with keys: qubit_count, gate_count, depth,
        build_time, gate_counts.
    """
    print("=" * 60)
    print("  QUANTUM CHESS WALK DEMO")
    print("  KNK Depth-2 Framework Showcase")
    print("=" * 60)
    print()

    # ------------------------------------------------------------------
    # Section 1: Position
    # ------------------------------------------------------------------
    print("--- Position ---")
    print(f"White king:    sq {WK_SQ} (e4)")
    print(f"Black king:    sq {BK_SQ} (e8)")
    print(f"White knight:  sq {WN_SQUARES[0]} (c3)")
    print(f"Walk depth:    {MAX_DEPTH} (white move + black response)")
    print()
    print_position(WK_SQ, BK_SQ, WN_SQUARES)
    print()

    # ------------------------------------------------------------------
    # Section 2: Move Enumeration (all-moves tables)
    # ------------------------------------------------------------------
    print("--- Move Enumeration ---")
    white_table = build_move_table([("wk", "king"), ("wn", "knight")])
    black_table = build_move_table([("bk", "king")])
    print(f"  White move table: {len(white_table)} entries (king: 8 offsets + knight: 8 offsets)")
    print(f"  Black move table: {len(black_table)} entries (king: 8 offsets)")
    print("  Edge-of-board filtering handled by quantum legality predicate")
    print()

    # ------------------------------------------------------------------
    # Section 3: Circuit Construction
    # ------------------------------------------------------------------
    print("--- Circuit Construction ---")
    c = ql.circuit()

    # Encode board position as qarrays
    boards = encode_position(WK_SQ, BK_SQ, WN_SQUARES)

    # Precompute walk data (move oracles + quantum predicates per level)
    move_data = prepare_walk_data(WK_SQ, BK_SQ, WN_SQUARES, MAX_DEPTH)

    # Create walk registers
    h_reg = create_height_register(MAX_DEPTH)
    branch_regs = create_branch_registers(MAX_DEPTH, move_data)

    print(f"  Height register:  {h_reg.width} qubits (one-hot, max_depth={MAX_DEPTH})")
    print(f"  Branch registers: {[br.width for br in branch_regs]} bits per level")
    print(f"  Board qarrays:    3 x 8x8 = {3 * 64} qubits")
    print()

    # ------------------------------------------------------------------
    # Section 4: Walk Compilation
    # ------------------------------------------------------------------
    print("--- Walk Compilation ---")
    print("  Compiling walk step U = R_B * R_A ...")

    board_arrs = (boards["white_king"], boards["black_king"], boards["white_knights"])
    oracle_per_level = [md["apply_move"] for md in move_data]

    t0 = time.time()
    walk_step(h_reg, branch_regs, board_arrs, oracle_per_level, move_data, MAX_DEPTH)
    build_time = time.time() - t0

    print(f"  Build time: {build_time:.3f}s")
    print()

    # ------------------------------------------------------------------
    # Section 5: Circuit Statistics
    # ------------------------------------------------------------------
    print("--- Circuit Statistics ---")
    print(f"  Qubits:  {c.qubit_count}")
    print(f"  Gates:   {c.gate_count}")
    print(f"  Depth:   {c.depth}")
    print()

    gate_counts = c.gate_counts
    print("  Gate breakdown:")
    for gate_name, count in sorted(gate_counts.items()):
        if count > 0:
            print(f"    {gate_name:10s}: {count}")
    print()

    # ------------------------------------------------------------------
    # Optional visualization
    # ------------------------------------------------------------------
    if visualize:
        print("--- Visualization ---")
        ql.draw_circuit(c, save="chess_circuit.png")
        print("  Saved circuit diagram to chess_circuit.png")
        print()

    print("=" * 60)
    print("  Demo complete.")
    print("=" * 60)

    return {
        "qubit_count": c.qubit_count,
        "gate_count": c.gate_count,
        "depth": c.depth,
        "build_time": build_time,
        "gate_counts": gate_counts,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Quantum chess walk demo")
    parser.add_argument(
        "--visualize",
        action="store_true",
        help="Save circuit diagram to chess_circuit.png",
    )
    args = parser.parse_args()
    main(visualize=args.visualize)
