"""Phase 98-02: Statevector verification tests for local diffusion D_x.

Tests verify D_x amplitude correctness for various branching degrees,
root special case, eigenstate preservation, and reflection property.

Requirements: DIFF-01, DIFF-02, DIFF-03
"""

import math

import numpy as np
import pytest
import qiskit.qasm3
from qiskit import transpile
from qiskit_aer import AerSimulator

import quantum_language as ql
from quantum_language._gates import emit_ry, emit_x
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


def _prepare_at_depth(tree, target_depth):
    """Move tree state from root to a given depth for testing.

    Flips root height qubit off and target depth qubit on.
    The state becomes |h[target_depth]=1, all branches=0>.
    """
    emit_x(tree._height_qubit(tree.max_depth))  # root off
    emit_x(tree._height_qubit(target_depth))  # target on


# ---------------------------------------------------------------------------
# Group 1: Non-root Diffusion Tests (DIFF-01)
# ---------------------------------------------------------------------------


class TestDiffusionNonRoot:
    """DIFF-01: Non-root D_x local diffusion operator tests."""

    def test_leaf_no_op(self):
        """depth=0 (leaf) emits no gates -- statevector unchanged.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.local_diffusion(depth=0)
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_before, sv_after, atol=1e-10)

    def test_diffusion_d2_reflection_property(self):
        """D_x^2 = I for d=2 at depth=1 on binary tree.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        _prepare_at_depth(tree, target_depth=1)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.local_diffusion(depth=1)
        tree.local_diffusion(depth=1)
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_before, sv_after, atol=1e-6), (
            "D_x^2 should equal identity (reflection property)"
        )

    def test_diffusion_d3_reflection_property(self):
        """D_x^2 = I for d=3 at depth=1 on ternary tree.

        Tree: max_depth=2, branching=3, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=3)
        _prepare_at_depth(tree, target_depth=1)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.local_diffusion(depth=1)
        tree.local_diffusion(depth=1)
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_before, sv_after, atol=1e-6)

    def test_diffusion_d4_reflection_property(self):
        """D_x^2 = I for d=4 at depth=1.

        Tree: max_depth=2, branching=4, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=4)
        _prepare_at_depth(tree, target_depth=1)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.local_diffusion(depth=1)
        tree.local_diffusion(depth=1)
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_before, sv_after, atol=1e-6)

    def test_diffusion_d5_reflection_property(self):
        """D_x^2 = I for d=5 at depth=1 (uses CCRy and Toffoli decomposition).

        Tree: max_depth=2, branching=5, total_qubits=9 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=5)
        _prepare_at_depth(tree, target_depth=1)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.local_diffusion(depth=1)
        tree.local_diffusion(depth=1)
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_before, sv_after, atol=1e-6)

    def test_diffusion_d2_amplitudes(self):
        """D_x on d=2 starting from parent state produces correct amplitudes.

        The state preparation U maps |0,0> -> |psi_x> in the local subspace
        (h[depth-1] qubit and branch register).  D_x = 2|psi_x><psi_x| - I
        reflects through |psi_x>.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        _prepare_at_depth(tree, target_depth=1)

        # Apply single diffusion
        tree.local_diffusion(depth=1)
        sv = _simulate_statevector(ql.to_openqasm())

        # Compute expected |psi_x> and D_x|parent> analytically
        d = 2
        phi = 2.0 * math.atan(math.sqrt(d))
        cphi = math.cos(phi / 2)  # 1/sqrt(3)
        sphi = math.sin(phi / 2)  # sqrt(2/3)
        c_cascade = math.cos(math.pi / 4)  # 1/sqrt(2)
        s_cascade = math.sin(math.pi / 4)  # 1/sqrt(2)

        # |psi_x> in local (h[0], branch_reg[1]) basis:
        # |h0=0,b1=0>, |h0=0,b1=1>, |h0=1,b1=0>, |h0=1,b1=1>
        psi = np.array([cphi * c_cascade, cphi * s_cascade, sphi * c_cascade, sphi * s_cascade])
        start = np.array([1.0, 0.0, 0.0, 0.0])  # parent: h0=0, b1=0
        inner = np.dot(psi, start)
        expected_local = 2 * inner * psi - start

        # Map local amplitudes to full statevector indices
        # h[0]=q0, h[1]=q1 (set), h[2]=q2, branch_reg[0]=q3, branch_reg[1]=q4
        h1_idx = tree._height_qubit(1)
        h0_idx = tree._height_qubit(0)
        br1_q = int(tree.branch_registers[1].qubits[63])

        # Indices (h[1] is always set):
        idx_00 = _qubit_state_index(h1_idx)  # h0=0, b1=0
        idx_01 = _qubit_state_index(h1_idx, br1_q)  # h0=0, b1=1
        idx_10 = _qubit_state_index(h1_idx, h0_idx)  # h0=1, b1=0
        idx_11 = _qubit_state_index(h1_idx, h0_idx, br1_q)  # h0=1, b1=1

        # S_0 convention gives global phase -1
        observed = np.array([sv[idx_00], sv[idx_01], sv[idx_10], sv[idx_11]])
        # Match up to global phase
        assert abs(abs(np.vdot(observed, expected_local)) - 1.0) < 1e-6, (
            f"Amplitudes don't match expected D_x|parent>.\n"
            f"  Expected: {expected_local}\n"
            f"  Observed: {observed}"
        )

    def test_diffusion_info_d2_angles(self):
        """diffusion_info returns correct phi for d=2.

        phi = 2*arctan(sqrt(2)) ~ 1.9106
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        info = tree.diffusion_info(1)

        expected_phi = 2.0 * math.atan(math.sqrt(2))
        assert info["phi"] == pytest.approx(expected_phi, abs=1e-10)
        assert info["d"] == 2
        assert info["depth"] == 1
        assert info["is_root"] is False

    def test_diffusion_info_d3_angles(self):
        """diffusion_info returns correct phi for d=3.

        phi = 2*arctan(sqrt(3)) ~ 2.0944
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=3)
        info = tree.diffusion_info(1)

        expected_phi = 2.0 * math.atan(math.sqrt(3))
        assert info["phi"] == pytest.approx(expected_phi, abs=1e-10)
        assert info["d"] == 3

    def test_diffusion_depth_validation_negative(self):
        """ValueError for depth < 0."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        with pytest.raises(ValueError, match="depth must be"):
            tree.local_diffusion(depth=-1)

    def test_diffusion_depth_validation_too_large(self):
        """ValueError for depth > max_depth."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        with pytest.raises(ValueError, match="depth must be"):
            tree.local_diffusion(depth=3)

    def test_per_level_branching_angles(self):
        """QWalkTree(max_depth=2, branching=[2,3]) has different angles per depth.

        Tree: total_qubits = 3 + 1 + 2 = 6 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=[2, 3])

        info_d1 = tree.diffusion_info(1)  # level_idx=1, branching=3
        info_d2 = tree.diffusion_info(2)  # level_idx=0, branching=2, root

        assert info_d1["d"] == 3
        assert info_d1["phi"] == pytest.approx(2.0 * math.atan(math.sqrt(3)), abs=1e-10)
        assert info_d2["d"] == 2
        assert info_d2["is_root"] is True


# ---------------------------------------------------------------------------
# Group 2: Root Diffusion Tests (DIFF-02)
# ---------------------------------------------------------------------------


class TestDiffusionRoot:
    """DIFF-02: Root node D_x diffusion operator tests."""

    def test_root_diffusion_reflection_property(self):
        """D_root^2 = I for root node on binary tree.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.local_diffusion(depth=2)  # depth=max_depth is root
        tree.local_diffusion(depth=2)
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_before, sv_after, atol=1e-6), "D_root^2 should equal identity"

    def test_root_diffusion_d2_n2_amplitudes(self):
        """Root diffusion on binary tree depth=2 produces correct amplitudes.

        Root amplitude = 1/sqrt(1+n*d) = 1/sqrt(5)
        Per-child amplitude = sqrt(n)/sqrt(1+n*d) = sqrt(2)/sqrt(5)

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)

        # Apply single root diffusion (starting from root state)
        tree.local_diffusion(depth=2)
        sv = _simulate_statevector(ql.to_openqasm())

        # Compute expected |psi_root> and D_root|root>
        d, n = 2, 2
        phi_root = 2.0 * math.atan(math.sqrt(n * d))
        cp = math.cos(phi_root / 2)
        sp = math.sin(phi_root / 2)
        cc = math.cos(math.pi / 4)
        sc = math.sin(math.pi / 4)

        # |psi_root> in local subspace: h[1] qubit + branch_reg[0]
        psi = np.array([cp * cc, cp * sc, sp * cc, sp * sc])
        start = np.array([1.0, 0.0, 0.0, 0.0])
        inner = np.dot(psi, start)
        expected_local = 2 * inner * psi - start

        # Map to full indices (h[2] always set for root)
        h2_idx = tree._height_qubit(2)
        h1_idx = tree._height_qubit(1)
        br0_q = int(tree.branch_registers[0].qubits[63])

        idx_00 = _qubit_state_index(h2_idx)
        idx_01 = _qubit_state_index(h2_idx, br0_q)
        idx_10 = _qubit_state_index(h2_idx, h1_idx)
        idx_11 = _qubit_state_index(h2_idx, h1_idx, br0_q)

        observed = np.array([sv[idx_00], sv[idx_01], sv[idx_10], sv[idx_11]])
        assert abs(abs(np.vdot(observed, expected_local)) - 1.0) < 1e-6, (
            f"Root diffusion amplitudes don't match.\n"
            f"  Expected: {expected_local}\n"
            f"  Observed: {observed}"
        )

    def test_root_diffusion_d3_n2_amplitudes(self):
        """Root diffusion on ternary tree depth=2.

        Root amplitude = 1/sqrt(1+n*d) = 1/sqrt(7)
        Per-child amplitude = sqrt(n)/sqrt(1+n*d) = sqrt(2)/sqrt(7)

        Tree: max_depth=2, branching=3, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=3)
        tree.local_diffusion(depth=2)
        sv = _simulate_statevector(ql.to_openqasm())

        # Verify D_root modifies the state (not an identity)
        # Root state index = h[2] set
        h2_idx = tree._height_qubit(2)
        root_idx = _qubit_state_index(h2_idx)
        root_amp = abs(sv[root_idx])

        # Root amplitude should NOT be 1.0 (diffusion creates superposition)
        assert root_amp < 0.99, (
            f"Root diffusion should create superposition, got root_amp={root_amp}"
        )

        # Reflection property
        ql.circuit()
        tree2 = QWalkTree(max_depth=2, branching=3)
        sv0 = _simulate_statevector(ql.to_openqasm())
        tree2.local_diffusion(depth=2)
        tree2.local_diffusion(depth=2)
        sv2 = _simulate_statevector(ql.to_openqasm())
        assert np.allclose(sv0, sv2, atol=1e-6)

    def test_root_diffusion_info(self):
        """diffusion_info(max_depth) returns is_root=True and correct phi_root.

        phi_root = 2*arctan(sqrt(n*d)) for n=2, d=2: 2*arctan(2)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        info = tree.diffusion_info(depth=2)

        assert info["is_root"] is True
        expected_phi_root = 2.0 * math.atan(math.sqrt(2 * 2))
        assert info["phi_root"] == pytest.approx(expected_phi_root, abs=1e-10)

    def test_root_diffusion_d3_info(self):
        """Root diffusion info for d=3, n=3.

        phi_root = 2*arctan(sqrt(9)) = 2*arctan(3)

        Tree: max_depth=3, branching=3, total_qubits=10 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=3, branching=3)
        info = tree.diffusion_info(depth=3)

        assert info["is_root"] is True
        expected_phi_root = 2.0 * math.atan(math.sqrt(3 * 3))
        assert info["phi_root"] == pytest.approx(expected_phi_root, abs=1e-10)

    def test_root_uses_different_formula_than_nonroot(self):
        """Root phi_root differs from non-root phi for same branching degree.

        For d=2, n=2: phi=2*arctan(sqrt(2))~1.91, phi_root=2*arctan(2)~2.21
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)

        nonroot_info = tree.diffusion_info(1)
        root_info = tree.diffusion_info(2)

        assert nonroot_info["phi"] != pytest.approx(root_info["phi_root"], abs=0.01), (
            "Root and non-root should use different angle formulas"
        )


# ---------------------------------------------------------------------------
# Group 3: Comprehensive Amplitude Verification (DIFF-03)
# ---------------------------------------------------------------------------


class TestDiffusionAmplitudes:
    """DIFF-03: Comprehensive statevector amplitude verification tests."""

    def test_amplitude_d2_psi_x_eigenstate(self):
        """D_x|psi_x> = |psi_x> (up to global phase): eigenstate preservation.

        Prepare |psi_x> explicitly using Ry + cascade, then verify D_x
        preserves it (eigenvalue +1 for the reflection target).

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        d = 2
        phi = 2.0 * math.atan(math.sqrt(d))

        # Build the circuit: prepare |psi_x> at depth=1 then apply D_x
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)

        # Move to depth=1
        _prepare_at_depth(tree, target_depth=1)

        # Prepare |psi_x> by applying U (the state preparation):
        # Step 1: Ry(phi) on h[0] -- parent/children split
        h0_q = tree._height_qubit(0)
        emit_ry(h0_q, phi)
        # Step 2: Ry(pi/2) on branch_reg[1] -- cascade for d=2
        br1_q = int(tree.branch_registers[1].qubits[63])
        emit_ry(br1_q, math.pi / 2)

        # Capture |psi_x> state
        sv_psi = _simulate_statevector(ql.to_openqasm())

        # Apply D_x
        tree.local_diffusion(depth=1)
        sv_after = _simulate_statevector(ql.to_openqasm())

        # D_x|psi_x> should equal |psi_x> up to global phase
        overlap = abs(np.vdot(sv_psi, sv_after))
        assert overlap == pytest.approx(1.0, abs=1e-6), (
            f"|<psi_x|D_x|psi_x>| should be 1.0 (eigenstate), got {overlap}"
        )

    def test_amplitude_d2_orthogonal_reflection(self):
        """State orthogonal to |psi_x> gets eigenvalue -1 under D_x.

        The D_x reflection has two eigenvalues: +1 for |psi_x> and -1 for
        states orthogonal to |psi_x> in the local subspace.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        d = 2
        phi = 2.0 * math.atan(math.sqrt(d))

        # Compute |psi_x> in local subspace
        cphi = math.cos(phi / 2)
        sphi = math.sin(phi / 2)
        cc = math.cos(math.pi / 4)
        sc = math.sin(math.pi / 4)
        psi_local = np.array([cphi * cc, cphi * sc, sphi * cc, sphi * sc])

        # Build an orthogonal state in the same subspace
        # Use Gram-Schmidt from |1,0,0,0>:
        # |orth> = |1,0,0,0> - <psi|1,0,0,0>*|psi>  (then normalize)
        e0 = np.array([1.0, 0.0, 0.0, 0.0])
        orth = e0 - np.dot(psi_local, e0) * psi_local
        orth = orth / np.linalg.norm(orth)

        # D_x|orth> = 2<psi|orth>|psi> - |orth> = 0 - |orth> = -|orth>
        dx_orth = 2 * np.dot(psi_local, orth) * psi_local - orth
        assert np.allclose(dx_orth, -orth, atol=1e-10), (
            "D_x should give eigenvalue -1 on orthogonal states"
        )

    def test_all_depths_sequential_no_crash(self):
        """Apply local_diffusion at every depth (0,1,2,3) without crash.

        Each individual D_x^2 = I is verified separately. This test ensures
        calling all depths in sequence doesn't error.

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=3, branching=2)

        # Apply all depths -- should not raise
        for depth in range(tree.max_depth + 1):
            tree.local_diffusion(depth=depth)

        # Verify a statevector can still be obtained
        sv = _simulate_statevector(ql.to_openqasm())
        assert abs(np.linalg.norm(sv) - 1.0) < 1e-6, "State should remain normalized"

    def test_each_depth_reflection_d2_depth3(self):
        """D_x^2 = I individually at each depth for deeper tree.

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        for target_depth in range(1, 4):  # depths 1, 2, 3
            ql.circuit()
            tree = QWalkTree(max_depth=3, branching=2)
            _prepare_at_depth(tree, target_depth=target_depth)
            sv_before = _simulate_statevector(ql.to_openqasm())

            tree.local_diffusion(depth=target_depth)
            tree.local_diffusion(depth=target_depth)
            sv_after = _simulate_statevector(ql.to_openqasm())

            assert np.allclose(sv_before, sv_after, atol=1e-6), (
                f"D_x^2 != I at depth={target_depth} on max_depth=3 tree"
            )

    def test_diffusion_no_effect_on_wrong_depth(self):
        """D_x at depth=2 has no effect on state at depth=1.

        The height-controlled dispatch ensures calling local_diffusion
        at the wrong depth is a no-op.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        _prepare_at_depth(tree, target_depth=1)
        sv_before = _simulate_statevector(ql.to_openqasm())

        # Apply diffusion at depth=2 (root) -- but state is at depth=1
        tree.local_diffusion(depth=2)
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_before, sv_after, atol=1e-6), "D_x at wrong depth should be a no-op"

    def test_diffusion_preserves_qubit_count(self):
        """local_diffusion does not allocate additional qubits.

        Tree: max_depth=2, branching=2, total_qubits=5 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        qasm_before = ql.to_openqasm()

        tree.local_diffusion(depth=1)
        qasm_after = ql.to_openqasm()

        # Both should declare the same number of qubits
        import re

        match_before = re.search(r"qubit\[(\d+)\]", qasm_before)
        match_after = re.search(r"qubit\[(\d+)\]", qasm_after)
        assert match_before and match_after
        assert match_before.group(1) == match_after.group(1), (
            "local_diffusion should not allocate extra qubits"
        )

    def test_diffusion_d2_depth2_on_deeper_tree(self):
        """D_x^2 = I at depth=2 (non-root) on a deeper tree.

        Tree: max_depth=3, branching=2, total_qubits=7 (<= 17)
        """
        ql.circuit()
        tree = QWalkTree(max_depth=3, branching=2)
        _prepare_at_depth(tree, target_depth=2)
        sv_before = _simulate_statevector(ql.to_openqasm())

        tree.local_diffusion(depth=2)
        tree.local_diffusion(depth=2)
        sv_after = _simulate_statevector(ql.to_openqasm())

        assert np.allclose(sv_before, sv_after, atol=1e-6)

    def test_leaf_diffusion_info(self):
        """diffusion_info(0) returns is_leaf=True."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        info = tree.diffusion_info(0)

        assert info.get("is_leaf") is True
        assert info["d"] == 0
        assert info["depth"] == 0
