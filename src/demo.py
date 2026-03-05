#!/usr/bin/env python3
"""Quantum chess walk demo -- progressive walkthrough of circuit construction.

Demonstrates a manual quantum walk on a simplified chess endgame position:
white king + white knight(s) vs black king. Shows board encoding, legal move
generation, quantum register construction, walk step compilation, and circuit
statistics.

No simulation is performed -- only circuit generation (~150 qubits exceeds
the 17-qubit Qiskit simulation budget).

Usage:
    python demo.py [--visualize]
"""

import argparse
import time

import chess_walk
import quantum_language as ql
from chess_encoding import (
    encode_position,
    legal_moves,
    print_moves,
    print_position,
)
from chess_walk import (
    all_walk_qubits,
    create_branch_registers,
    create_height_register,
    prepare_walk_data,
    walk_step,
)

# ---------------------------------------------------------------------------
# Position constants (configurable at module level)
# ---------------------------------------------------------------------------
WK_SQ = 28  # e4
BK_SQ = 60  # e8
WN_SQUARES = [18]  # c3
MAX_DEPTH = 1


def main(visualize=False):
    """Run the quantum chess walk demo walkthrough.

    Parameters
    ----------
    visualize : bool
        If True, save a circuit diagram to chess_circuit.png.

    Returns
    -------
    dict
        Circuit statistics with keys: qubit_count, gate_count, depth, gate_counts.
    """
    # Reset module-level compiled function cache for clean state
    chess_walk._walk_compiled_fn = None

    print("=" * 60)
    print("  QUANTUM CHESS WALK DEMO")
    print("=" * 60)
    print()

    # ------------------------------------------------------------------
    # Section 1: Position
    # ------------------------------------------------------------------
    t0 = time.time()
    print("--- Position ---")
    print(f"White king: sq {WK_SQ}  |  Black king: sq {BK_SQ}")
    print(f"White knight(s): {WN_SQUARES}")
    print()
    print_position(WK_SQ, BK_SQ, WN_SQUARES)
    t_position = time.time() - t0
    print(f"\n  [Position: {t_position:.3f}s]")
    print()

    # ------------------------------------------------------------------
    # Section 2: Legal Moves
    # ------------------------------------------------------------------
    t0 = time.time()
    print("--- Legal Moves ---")
    white_moves = legal_moves(WK_SQ, BK_SQ, WN_SQUARES, "white")
    print_moves(white_moves, label="White moves")
    print(f"\n  Move count: {len(white_moves)}")
    t_moves = time.time() - t0
    print(f"  [Legal moves: {t_moves:.3f}s]")
    print()

    # ------------------------------------------------------------------
    # Section 3: Quantum Registers
    # ------------------------------------------------------------------
    t0 = time.time()
    print("--- Quantum Registers ---")

    # Initialize circuit before any quantum operations
    c = ql.circuit()

    # Encode board position as qarrays
    boards = encode_position(WK_SQ, BK_SQ, WN_SQUARES)

    # Prepare walk data (move oracles per level)
    move_data = prepare_walk_data(WK_SQ, BK_SQ, WN_SQUARES, MAX_DEPTH)

    # Create registers
    h_reg = create_height_register(MAX_DEPTH)
    branch_regs = create_branch_registers(MAX_DEPTH, move_data)

    # Compute walk qubit wrapper
    walk_reg = all_walk_qubits(h_reg, branch_regs, MAX_DEPTH)

    print(f"  Height register: {h_reg.width} qubits (one-hot, max_depth={MAX_DEPTH})")
    print(f"  Branch registers: {[br.width for br in branch_regs]} bits per level")
    print(f"  Walk register total: {walk_reg.width} qubits (height + branches)")
    print(f"  Board qarrays: 3 x 8x8 qbool = {3 * 64} qubits")
    t_registers = time.time() - t0
    print(f"\n  [Registers: {t_registers:.3f}s]")
    print()

    # ------------------------------------------------------------------
    # Section 4: Tree Structure
    # ------------------------------------------------------------------
    print("--- Tree Structure ---")
    for level, md in enumerate(move_data):
        side = "white" if level % 2 == 0 else "black"
        print(
            f"  Level {level} ({side}): {md['move_count']} moves, {md['branch_width']} branch bits"
        )
    print()

    # ------------------------------------------------------------------
    # Section 5: Walk Step Compilation
    # ------------------------------------------------------------------
    t0 = time.time()
    print("--- Walk Step Compilation ---")
    print("  Compiling U = R_B * R_A ...")

    board_arrs = (boards["white_king"], boards["black_king"], boards["white_knights"])
    oracle_per_level = [md["apply_move"] for md in move_data]

    walk_step(h_reg, branch_regs, board_arrs, oracle_per_level, move_data, MAX_DEPTH)

    t_compile = time.time() - t0
    print(f"  [Walk step compilation: {t_compile:.3f}s]")
    print()

    # ------------------------------------------------------------------
    # Section 6: Circuit Statistics
    # ------------------------------------------------------------------
    print("--- Circuit Statistics ---")
    print(f"  Qubits:     {c.qubit_count}")
    print(f"  Gates:      {c.gate_count}")
    print(f"  Depth:      {c.depth}")
    print()

    # Per-gate-type breakdown
    gate_counts = c.gate_counts
    print("  Gate breakdown:")
    for gate_name, count in sorted(gate_counts.items()):
        if count > 0:
            print(f"    {gate_name:10s}: {count}")
    print()

    # Allocator stats
    alloc_stats = ql.circuit_stats()
    print("  Allocator info:")
    for key, val in sorted(alloc_stats.items()):
        print(f"    {key}: {val}")
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
