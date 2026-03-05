#!/usr/bin/env python3
"""Quantum chess walk comparison: Manual vs QWalkTree API.

Runs the same chess endgame position through both the manual quantum walk
implementation (demo.py) and the QWalkTree framework API, then prints a
side-by-side comparison of circuit statistics (qubits, gates, depth).

No simulation is performed -- only circuit generation.

Usage:
    python chess_comparison.py
"""

import time

import chess_walk
import quantum_language as ql
from chess_walk import prepare_walk_data
from quantum_language.walk import QWalkTree

# ---------------------------------------------------------------------------
# Position constants (same as demo.py for fair comparison)
# ---------------------------------------------------------------------------
WK_SQ = 28  # e4
BK_SQ = 60  # e8
WN_SQUARES = [18]  # c3
MAX_DEPTH = 1


def print_comparison(manual_stats, api_stats):
    """Print a formatted comparison table of circuit statistics.

    Parameters
    ----------
    manual_stats : dict
        Circuit stats from the manual approach (demo.py).
    api_stats : dict
        Circuit stats from the QWalkTree API approach.
    """
    metrics = [
        ("Qubits", "qubit_count"),
        ("Gates", "gate_count"),
        ("Depth", "depth"),
    ]

    header = f"  {'Metric':<12} {'Manual':>10} {'QWalkTree':>10} {'Delta':>10}"
    sep = "  " + "-" * 46

    print(header)
    print(sep)
    for label, key in metrics:
        m_val = manual_stats.get(key, 0)
        a_val = api_stats.get(key, 0)
        delta = a_val - m_val
        sign = "+" if delta > 0 else "" if delta == 0 else ""
        delta_str = f"{sign}{delta:d}" if delta != 0 else "0"
        print(f"  {label:<12} {m_val:>10d} {a_val:>10d} {delta_str:>10}")


def get_api_stats():
    """Build the quantum walk circuit using the QWalkTree API.

    Returns
    -------
    dict
        Circuit statistics: qubit_count, gate_count, depth.
    """
    # Reset compiled function cache for clean state
    chess_walk._walk_compiled_fn = None

    # Fresh circuit
    c = ql.circuit()

    # Compute branching factors from prepare_walk_data
    move_data = prepare_walk_data(WK_SQ, BK_SQ, WN_SQUARES, MAX_DEPTH)
    branching_list = [md["move_count"] for md in move_data]

    # Trivial predicate: always accept (allocates fresh qbools each call)
    def trivial_predicate(node):
        return (ql.qbool(), ql.qbool())

    # Build the QWalkTree with matching branching factors
    tree = QWalkTree(
        max_depth=MAX_DEPTH,
        branching=branching_list,
        predicate=trivial_predicate,
        max_qubits=200,
    )

    # Compile walk step only (no detect)
    tree.walk_step()

    return {
        "qubit_count": c.qubit_count,
        "gate_count": c.gate_count,
        "depth": c.depth,
    }


def main():
    """Run manual vs QWalkTree comparison and print results.

    Returns
    -------
    dict
        Keys: 'manual' and 'api', each containing circuit stats.
    """
    print()
    print("=" * 60)
    print("  QUANTUM CHESS WALK: Manual vs QWalkTree Comparison")
    print("=" * 60)
    print()
    print(f"  Position: WK={WK_SQ} (e4), BK={BK_SQ} (e8), WN={WN_SQUARES} (c3)")
    print(f"  Max depth: {MAX_DEPTH}")
    print()

    # --- Manual approach (demo.py) ---
    print("--- Manual Approach (demo.py) ---")
    chess_walk._walk_compiled_fn = None
    import demo

    t0 = time.time()
    manual_stats = demo.main(visualize=False)
    t_manual = time.time() - t0
    print(f"\n  [Manual: {t_manual:.3f}s]")
    print()

    # --- QWalkTree API approach ---
    print("--- QWalkTree API Approach ---")
    t0 = time.time()
    api_stats = get_api_stats()
    t_api = time.time() - t0
    print(f"  Qubits: {api_stats['qubit_count']}")
    print(f"  Gates:  {api_stats['gate_count']}")
    print(f"  Depth:  {api_stats['depth']}")
    print(f"\n  [QWalkTree API: {t_api:.3f}s]")
    print()

    # --- Comparison table ---
    print("--- Comparison: Manual vs QWalkTree ---")
    print_comparison(manual_stats, api_stats)
    print()

    print("=" * 60)
    print("  Comparison complete.")
    print("=" * 60)

    return {"manual": manual_stats, "api": api_stats}


if __name__ == "__main__":
    main()
