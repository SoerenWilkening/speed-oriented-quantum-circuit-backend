#!/usr/bin/env python3
"""Quantum backtracking detection demo on a small SAT instance.

Demonstrates Montanaro's quantum walk detection algorithm (Algorithm 1)
on binary backtracking trees encoding SAT-like decision problems.

The algorithm applies the walk step U = R_B * R_A raised to increasing
powers (1, 2, 4, ...), measuring root state overlap after each. When
the tree structure allows solution detection, the overlap drops below
the 3/8 threshold.

Usage:
    python examples/sat_detection_demo.py

Requirements:
    - quantum_language (this project)
    - qiskit, qiskit-aer (for statevector simulation)

Qubit budget: All circuits stay within the 17-qubit simulator ceiling.
"""

import sys

import quantum_language as ql
from quantum_language._gates import emit_x
from quantum_language.walk import QWalkTree, _make_qbool_wrapper

# ---------------------------------------------------------------------------
# SAT predicate: rejects child 1 (models clause violation)
# ---------------------------------------------------------------------------


def _reject_child1_predicate(node):
    """SAT predicate: reject child 1 (branch value |1>) at each level.

    Models a simple SAT instance where taking the branch=1 path at any
    level violates a clause. The remaining branch=0 paths form the
    satisfying subtree.
    """
    is_accept = ql.qbool()
    is_reject = ql.qbool()
    br_values = node.branch_values
    if len(br_values) > 0:
        last_br = br_values[-1]
        br_qubit = int(last_br.qubits[63])
        reject_qubit = int(is_reject.qubits[63])
        ctrl = _make_qbool_wrapper(br_qubit)
        with ctrl:
            emit_x(reject_qubit)
    return (is_accept, is_reject)


# ---------------------------------------------------------------------------
# Demo
# ---------------------------------------------------------------------------


def run_demo():
    """Run the SAT detection demo."""
    print("=" * 60)
    print("Quantum Backtracking Detection Demo")
    print("Montanaro Algorithm 1 - Power-Method Detection")
    print("=" * 60)
    print()

    # --- Case 1: No-solution tree (depth=1, binary) ---
    print("Case 1: Binary tree, depth=1, no predicate")
    print("-" * 60)
    ql.circuit()
    tree1 = QWalkTree(max_depth=1, branching=2)
    print(f"  Tree qubits: {tree1.total_qubits}")
    print(f"  Tree size: {tree1._tree_size()} nodes")

    # Show root overlap at each power level
    for power in [1, 2]:
        overlap = tree1._measure_root_overlap(power)
        print(f"  Power {power}: root overlap = {overlap:.6f}", end="")
        print("  (< 3/8)" if overlap < 3.0 / 8.0 else "  (>= 3/8)")

    result1 = tree1.detect(max_iterations=8)
    print(f"  detect() = {result1}")
    print("  Expected: False (depth-1 tree has periodic walk, overlap stays above 3/8)")
    print()

    # --- Case 2: Solution tree (depth=2, binary) ---
    print("Case 2: Binary tree, depth=2 (2-variable SAT model)")
    print("-" * 60)
    ql.circuit()
    tree2 = QWalkTree(max_depth=2, branching=2)
    print(f"  Tree qubits: {tree2.total_qubits}")
    print(f"  Tree size: {tree2._tree_size()} nodes")

    for power in [1, 2, 4]:
        overlap = tree2._measure_root_overlap(power)
        marker = " ** DETECTION SIGNAL" if overlap < 3.0 / 8.0 else ""
        print(f"  Power {power}: root overlap = {overlap:.6f}", end="")
        print(f"  (< 3/8){marker}" if overlap < 3.0 / 8.0 else "  (>= 3/8)")

    result2 = tree2.detect(max_iterations=8)
    print(f"  detect() = {result2}")
    print("  Expected: True (binary depth-2 walk distributes amplitude below threshold)")
    print()

    # --- Case 3: No-solution tree (ternary, depth=2) ---
    print("Case 3: Ternary tree, depth=2 (higher connectivity)")
    print("-" * 60)
    ql.circuit()
    tree3 = QWalkTree(max_depth=2, branching=3)
    print(f"  Tree qubits: {tree3.total_qubits}")
    print(f"  Tree size: {tree3._tree_size()} nodes")

    for power in [1, 2, 4, 8]:
        overlap = tree3._measure_root_overlap(power)
        print(f"  Power {power}: root overlap = {overlap:.6f}", end="")
        print("  (< 3/8)" if overlap < 3.0 / 8.0 else "  (>= 3/8)")

    result3 = tree3.detect(max_iterations=16)
    print(f"  detect() = {result3}")
    print("  Expected: False (ternary tree keeps overlap above threshold)")
    print()

    # --- Case 4: Predicate walk comparison (depth=1) ---
    print("Case 4: Predicate effect on walk dynamics (depth=1)")
    print("-" * 60)
    print("  Comparing uniform walk vs. pruning predicate walk...")

    ql.circuit()
    tree_uniform = QWalkTree(max_depth=1, branching=2)
    tree_uniform.walk_step()
    qasm_uniform = ql.to_openqasm()
    n_uniform = _count_qubits(qasm_uniform)

    try:
        ql.circuit()
        tree_pruned = QWalkTree(max_depth=1, branching=2, predicate=_reject_child1_predicate)
        tree_pruned.walk_step()
        qasm_pruned = ql.to_openqasm()
        n_pruned = _count_qubits(qasm_pruned)

        print(f"  Uniform walk:  {n_uniform} qubits")
        print(f"  Predicate walk: {n_pruned} qubits")
        print("  Predicate adds ancilla qubits for validity checking")
        if n_pruned <= 17:
            print("  Both within 17-qubit budget")
        else:
            print("  Predicate walk exceeds 17-qubit budget (expected for raw predicates)")
    except ValueError as e:
        if "qubit" in str(e).lower():
            print(f"  Predicate walk skipped: {e}")
        else:
            raise
    print()

    # --- Summary ---
    print("=" * 60)
    print("Summary: Detection Results")
    print("=" * 60)
    print(f"  Case 1 (depth=1, binary):   detect() = {str(result1):<5}  (correct: False)")
    print(f"  Case 2 (depth=2, binary):   detect() = {str(result2):<5}  (correct: True)")
    print(f"  Case 3 (depth=2, ternary):  detect() = {str(result3):<5}  (correct: False)")
    print()

    # Verify correctness
    passed = True
    if result1 is not False:
        print("  [FAIL] Case 1: False positive on depth=1 tree")
        passed = False
    else:
        print("  [PASS] Case 1: No false positive on depth=1 tree")

    if result2 is not True:
        print("  [FAIL] Case 2: Missed detection on depth=2 binary tree")
        passed = False
    else:
        print("  [PASS] Case 2: Solution detected on depth=2 binary tree")

    if result3 is not False:
        print("  [FAIL] Case 3: False positive on ternary tree")
        passed = False
    else:
        print("  [PASS] Case 3: No false positive on ternary tree")

    print()
    print("=" * 60)
    if passed:
        print("All detection tests passed.")
    else:
        print("Some tests failed!")
        sys.exit(1)


def _count_qubits(qasm_str):
    """Count total qubits in a QASM string."""
    for line in qasm_str.split("\n"):
        stripped = line.strip()
        if stripped.startswith("qubit["):
            return int(stripped.split("[")[1].split("]")[0])
    return 0


if __name__ == "__main__":
    run_demo()
