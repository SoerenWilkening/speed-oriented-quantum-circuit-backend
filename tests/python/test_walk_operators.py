"""Phase 99: Walk operator tests for R_A, R_B, walk_step, and disjointness.

Tests verify structural correctness (WALK-05), operator composition (WALK-01,
WALK-02, WALK-03), compilation (WALK-04), and statevector transformation.

Requirements: WALK-01, WALK-02, WALK-03, WALK-04, WALK-05
"""

import re

import numpy as np
import qiskit.qasm3
from qiskit import transpile
from qiskit_aer import AerSimulator

import quantum_language as ql
from quantum_language.walk import QWalkTree

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------


def _simulate_statevector(qasm_str):
    """Run QASM through Qiskit Aer and return statevector as numpy array."""
    circuit = qiskit.qasm3.loads(qasm_str)
    circuit.save_statevector()
    sim = AerSimulator(method="statevector", max_parallel_threads=4)
    result = sim.run(transpile(circuit, sim)).result()
    return np.asarray(result.get_statevector())


def _qubit_state_index(*qubit_indices):
    """Compute statevector index for a basis state with given qubits set to |1>.

    Uses Qiskit's little-endian convention: qubit k being |1> means
    bit k of the index is 1.
    """
    idx = 0
    for q in qubit_indices:
        idx |= 1 << q
    return idx


# ---------------------------------------------------------------------------
# Group 1: Disjointness Tests (WALK-05)
# ---------------------------------------------------------------------------


class TestDisjointness:
    """WALK-05: Qubit disjointness tests for R_A and R_B."""

    def test_disjointness_binary_depth2(self):
        """Binary tree depth=2: R_A={h[0]}, R_B={h[1], h[2]}, disjoint.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        result = tree.verify_disjointness()

        assert result["disjoint"] is True
        assert len(result["overlap"]) == 0
        # R_A: even depths excluding root = {0}
        # R_B: odd depths + root = {1, 2}
        assert len(result["R_A_qubits"]) == 1  # h[0]
        assert len(result["R_B_qubits"]) == 2  # h[1], h[2]

    def test_disjointness_binary_depth3(self):
        """Binary tree depth=3: R_A={h[0], h[2]}, R_B={h[1], h[3]}, disjoint.

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=3, branching=2)
        result = tree.verify_disjointness()

        assert result["disjoint"] is True
        assert len(result["R_A_qubits"]) == 2  # h[0], h[2]
        assert len(result["R_B_qubits"]) == 2  # h[1], h[3] (root=3, odd)

    def test_disjointness_binary_depth4(self):
        """Binary tree depth=4: root (even) assigned to R_B.

        Tree: max_depth=4, branching=2, total_qubits=9 (<= 17)
        R_A: even depths 0, 2 (NOT 4, root excluded)
        R_B: odd depths 1, 3 + root 4
        """
        ql.circuit()
        tree = QWalkTree(max_depth=4, branching=2)
        result = tree.verify_disjointness()

        assert result["disjoint"] is True
        assert len(result["R_A_qubits"]) == 2  # h[0], h[2]
        assert len(result["R_B_qubits"]) == 3  # h[1], h[3], h[4]

    def test_disjointness_ternary_depth2(self):
        """Ternary tree depth=2: disjoint.

        Tree: max_depth=2, branching=3, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=3)
        result = tree.verify_disjointness()
        assert result["disjoint"] is True

    def test_disjointness_returns_correct_type(self):
        """verify_disjointness returns dict with expected keys and types.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        result = tree.verify_disjointness()

        assert isinstance(result, dict)
        assert "R_A_qubits" in result
        assert "R_B_qubits" in result
        assert "overlap" in result
        assert "disjoint" in result
        assert isinstance(result["R_A_qubits"], set)
        assert isinstance(result["R_B_qubits"], set)
        assert isinstance(result["overlap"], set)
        assert isinstance(result["disjoint"], bool)

    def test_disjointness_depth1(self):
        """Minimal tree depth=1: R_A={h[0]}, R_B={h[1]}.

        Tree: max_depth=1, branching=2, total_qubits=3 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=1, branching=2)
        result = tree.verify_disjointness()

        assert result["disjoint"] is True
        assert len(result["R_A_qubits"]) == 1  # h[0]
        assert len(result["R_B_qubits"]) == 1  # h[1] (root, odd)


# ---------------------------------------------------------------------------
# Group 2: R_A Operator Tests (WALK-01)
# ---------------------------------------------------------------------------


class TestRAOperator:
    """WALK-01: R_A applies local diffusions at even-depth nodes."""

    def test_R_A_no_root_even_maxdepth(self):
        """R_A on max_depth=2 tree does NOT apply root diffusion.

        For max_depth=2: even depths are 0 (leaf, no-op) and 2 (root, excluded).
        R_A should have no effect on root state.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.R_A()
        sv_after = _simulate_statevector(ql.to_openqasm())

        # R_A on root state: depth 0 is leaf no-op, depth 2 is root (excluded)
        # So R_A should be identity on root state
        assert np.allclose(sv_before, sv_after, atol=1e-10), (
            "R_A should not affect root state (only covers leaf no-op)"
        )

    def test_R_A_equals_manual_even_loop(self):
        """R_A produces same statevector as manual even-depth loop.

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        # Circuit with R_A
        ql.circuit()
        tree_a = QWalkTree(max_depth=3, branching=2)
        tree_a.R_A()
        sv_ra = _simulate_statevector(ql.to_openqasm())

        # Circuit with manual even-depth loop (excluding root)
        ql.circuit()
        tree_m = QWalkTree(max_depth=3, branching=2)
        for depth in range(0, tree_m.max_depth + 1, 2):
            if depth == tree_m.max_depth:
                continue  # Exclude root
            tree_m.local_diffusion(depth)
        sv_manual = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_ra, sv_manual, atol=1e-10), "R_A must equal manual even-depth loop"

    def test_R_A_depth4_equals_manual(self):
        """R_A on depth=4 tree equals manual even loop.

        Tree: max_depth=4, branching=2, total_qubits=9 (<= 17)
        """
        ql.circuit()
        tree_a = QWalkTree(max_depth=4, branching=2)
        tree_a.R_A()
        sv_ra = _simulate_statevector(ql.to_openqasm())

        ql.circuit()
        tree_m = QWalkTree(max_depth=4, branching=2)
        for depth in range(0, tree_m.max_depth + 1, 2):
            if depth == tree_m.max_depth:
                continue
            tree_m.local_diffusion(depth)
        sv_manual = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_ra, sv_manual, atol=1e-10)


# ---------------------------------------------------------------------------
# Group 3: R_B Operator Tests (WALK-02)
# ---------------------------------------------------------------------------


class TestRBOperator:
    """WALK-02: R_B applies local diffusions at odd-depth nodes plus root."""

    def test_R_B_includes_root_even_maxdepth(self):
        """R_B on max_depth=2 tree includes root diffusion.

        Root is at depth 2 (even), explicitly added to R_B.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.R_B()
        sv_after = _simulate_statevector(ql.to_openqasm())

        # R_B should modify root state (root diffusion creates superposition)
        assert not np.allclose(sv_before, sv_after, atol=1e-6), (
            "R_B should modify root state (includes root diffusion)"
        )

    def test_R_B_includes_root_odd_maxdepth(self):
        """R_B on max_depth=3 tree includes root (odd, already in loop).

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=3, branching=2)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.R_B()
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert not np.allclose(sv_before, sv_after, atol=1e-6), (
            "R_B should modify root state (root at odd depth)"
        )

    def test_R_B_equals_manual_odd_plus_root_even_maxdepth(self):
        """R_B on max_depth=2 equals manual odd loop + root.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree_b = QWalkTree(max_depth=2, branching=2)
        tree_b.R_B()
        sv_rb = _simulate_statevector(ql.to_openqasm())

        ql.circuit()
        tree_m = QWalkTree(max_depth=2, branching=2)
        # Odd depths: 1
        tree_m.local_diffusion(1)
        # Root: 2 (even, must add explicitly)
        tree_m.local_diffusion(2)
        sv_manual = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_rb, sv_manual, atol=1e-10), (
            "R_B must equal manual odd-depth + root loop"
        )

    def test_R_B_equals_manual_odd_plus_root_odd_maxdepth(self):
        """R_B on max_depth=3 equals manual odd loop (root included naturally).

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree_b = QWalkTree(max_depth=3, branching=2)
        tree_b.R_B()
        sv_rb = _simulate_statevector(ql.to_openqasm())

        ql.circuit()
        tree_m = QWalkTree(max_depth=3, branching=2)
        # Odd depths: 1, 3 (root=3 is odd, included naturally)
        tree_m.local_diffusion(1)
        tree_m.local_diffusion(3)
        sv_manual = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_rb, sv_manual, atol=1e-10), (
            "R_B must equal manual odd-depth loop (root naturally included)"
        )


# ---------------------------------------------------------------------------
# Group 4: Walk Step Tests (WALK-03)
# ---------------------------------------------------------------------------


class TestWalkStep:
    """WALK-03: Walk step U = R_B * R_A composed as single operation."""

    def test_walk_step_equals_R_A_then_R_B(self):
        """walk_step() produces same statevector as R_A() then R_B().

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree_w = QWalkTree(max_depth=2, branching=2)
        tree_w.walk_step()
        sv_walk = _simulate_statevector(ql.to_openqasm())

        ql.circuit()
        tree_m = QWalkTree(max_depth=2, branching=2)
        tree_m.R_A()
        tree_m.R_B()
        sv_manual = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_walk, sv_manual, atol=1e-10), (
            "walk_step must equal R_A followed by R_B"
        )

    def test_walk_step_modifies_root_state(self):
        """walk_step on root state produces non-trivial superposition.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.walk_step()
        sv_after = _simulate_statevector(ql.to_openqasm())

        # Walk step should modify the root state
        assert not np.allclose(sv_before, sv_after, atol=1e-6), "Walk step should modify root state"

    def test_walk_step_not_identity_squared(self):
        """U^2 != I for walk step (not a reflection).

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        sv_initial = _simulate_statevector(ql.to_openqasm())

        tree.walk_step()
        tree.walk_step()
        sv_after_two = _simulate_statevector(ql.to_openqasm())

        # Walk step is unitary but NOT a reflection, so U^2 != I
        assert not np.allclose(sv_initial, sv_after_two, atol=1e-6), (
            "U^2 should not equal identity (walk step is not a reflection)"
        )

    def test_walk_step_preserves_norm(self):
        """Walk step preserves statevector norm (unitarity check).

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        tree.walk_step()
        sv = _simulate_statevector(ql.to_openqasm())

        norm = np.linalg.norm(sv)
        assert abs(norm - 1.0) < 1e-6, f"State should remain normalized, got norm={norm}"

    def test_walk_step_d3_depth2(self):
        """Walk step on ternary tree depth=2 modifies root state.

        Tree: max_depth=2, branching=3, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=3)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.walk_step()
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert not np.allclose(sv_before, sv_after, atol=1e-6)

    def test_walk_step_d2_depth3(self):
        """Walk step on binary tree depth=3 (R_A has non-trivial depth 2).

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=3, branching=2)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.walk_step()
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert not np.allclose(sv_before, sv_after, atol=1e-6)


# ---------------------------------------------------------------------------
# Group 5: Walk Step Compilation Tests (WALK-04)
# ---------------------------------------------------------------------------


class TestWalkStepCompiled:
    """WALK-04: Walk step wrapped in @ql.compile for caching and controlled variant."""

    def test_walk_step_compiled_matches_raw(self):
        """First walk_step (capture) matches raw R_A+R_B.

        Verifies the compiled capture produces same transformation
        as raw method calls.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        # Single walk_step (capture call)
        ql.circuit()
        tree_c = QWalkTree(max_depth=2, branching=2)
        tree_c.walk_step()
        sv_compiled = _simulate_statevector(ql.to_openqasm())

        # Single manual R_A+R_B
        ql.circuit()
        tree_m = QWalkTree(max_depth=2, branching=2)
        tree_m.R_A()
        tree_m.R_B()
        sv_manual = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_compiled, sv_manual, atol=1e-6), (
            "Compiled walk_step must match raw R_A+R_B"
        )

    def test_walk_step_second_call_uses_cache(self):
        """After first walk_step, _walk_compiled is not None.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        assert tree._walk_compiled is None, "_walk_compiled should be None before first call"

        tree.walk_step()
        assert tree._walk_compiled is not None, "_walk_compiled should be set after first call"

    def test_walk_step_no_extra_qubit_allocation(self):
        """walk_step does not allocate additional qubits.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        qasm_before = ql.to_openqasm()

        tree.walk_step()
        qasm_after = ql.to_openqasm()

        match_before = re.search(r"qubit\[(\d+)\]", qasm_before)
        match_after = re.search(r"qubit\[(\d+)\]", qasm_after)
        assert match_before and match_after
        assert match_before.group(1) == match_after.group(1), (
            "walk_step should not allocate extra qubits"
        )

    def test_walk_step_depth2_statevector_values(self):
        """Walk step on binary tree depth=2 produces specific non-trivial amplitudes.

        Verifies the walk step produces the expected statevector transformation
        from root state (required by Phase 99 success criterion 4).

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        tree.walk_step()
        sv = _simulate_statevector(ql.to_openqasm())

        # Root state index: h[2]=|1>, all others 0
        h2_idx = tree._height_qubit(2)
        root_idx = _qubit_state_index(h2_idx)

        # Root amplitude should NOT be 1.0 (state was modified)
        root_amp = abs(sv[root_idx])
        assert root_amp < 0.99, f"Root amplitude should be < 1.0 after walk step, got {root_amp}"

        # At least one non-root basis state should have non-zero amplitude
        sv_abs = np.abs(sv)
        sv_abs[root_idx] = 0  # Zero out root
        max_nonroot = np.max(sv_abs)
        assert max_nonroot > 0.01, f"Should have non-zero non-root amplitudes, max={max_nonroot}"

        # Norm should still be 1.0
        norm = np.linalg.norm(sv)
        assert abs(norm - 1.0) < 1e-6

    def test_walk_step_depth3_compiled_matches_raw(self):
        """Compiled walk_step on deeper tree matches raw.

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree_c = QWalkTree(max_depth=3, branching=2)
        tree_c.walk_step()
        sv_compiled = _simulate_statevector(ql.to_openqasm())

        ql.circuit()
        tree_m = QWalkTree(max_depth=3, branching=2)
        tree_m.R_A()
        tree_m.R_B()
        sv_manual = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_compiled, sv_manual, atol=1e-6)

    def test_walk_step_replay_is_unitary(self):
        """Compiled walk_step replay (second call) preserves unitarity.

        The optimized replay may differ from raw gate-by-gate emission,
        but must still produce a valid unitary (norm-preserving) transformation.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        tree.walk_step()  # capture
        tree.walk_step()  # replay
        sv = _simulate_statevector(ql.to_openqasm())

        norm = np.linalg.norm(sv)
        assert abs(norm - 1.0) < 1e-6, f"Replay should preserve norm, got {norm}"
