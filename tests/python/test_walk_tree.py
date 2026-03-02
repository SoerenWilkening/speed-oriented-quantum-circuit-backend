"""Phase 97-01: Tree encoding and root state verification tests.

Tests QWalkTree class register allocation, root state preparation,
TreeNode accessors, and qubit budget enforcement.

Requirements: TREE-01, TREE-02, TREE-03
"""

import pytest
import qiskit.qasm3
from qiskit import transpile
from qiskit_aer import AerSimulator

import quantum_language as ql
from quantum_language.walk import QWalkTree, TreeNode

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------


def _simulate_statevector(qasm_str):
    """Run QASM through Qiskit Aer and return statevector."""
    circuit = qiskit.qasm3.loads(qasm_str)
    circuit.save_statevector()
    sim = AerSimulator(method="statevector", max_parallel_threads=4)
    result = sim.run(transpile(circuit, sim)).result()
    return result.get_statevector()


# ---------------------------------------------------------------------------
# Group 1: Tree Encoding Register Tests
# ---------------------------------------------------------------------------


class TestQWalkTreeEncoding:
    """TREE-01: Register allocation and structure tests."""

    def test_binary_tree_registers(self):
        """depth=2, branching=2 -> height width=3, 2 branch regs of width 1, total=5."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)

        assert tree.height_register.width == 3
        assert len(tree.branch_registers) == 2
        assert tree.branch_registers[0].width == 1
        assert tree.branch_registers[1].width == 1
        assert tree.total_qubits == 5

    def test_ternary_tree_registers(self):
        """depth=3, branching=3 -> height width=4, 3 branch regs of width 2, total=10."""
        ql.circuit()
        tree = QWalkTree(max_depth=3, branching=3)

        assert tree.height_register.width == 4
        assert len(tree.branch_registers) == 3
        for reg in tree.branch_registers:
            assert reg.width == 2
        assert tree.total_qubits == 10

    def test_per_level_branching(self):
        """depth=2, branching=[2,3] -> branch widths [1, 2], total=3+1+2=6."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=[2, 3])

        assert tree.branch_registers[0].width == 1
        assert tree.branch_registers[1].width == 2
        assert tree.total_qubits == 6

    def test_single_child_branching(self):
        """depth=2, branching=1 -> branch widths [1, 1] (log2(1)=0 handled), total=5."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=1)

        assert tree.branch_registers[0].width == 1
        assert tree.branch_registers[1].width == 1
        assert tree.total_qubits == 5

    def test_invalid_branching_length(self):
        """depth=2, branching=[2] raises ValueError (length mismatch)."""
        ql.circuit()
        with pytest.raises(ValueError, match="branching list length"):
            QWalkTree(max_depth=2, branching=[2])

    def test_large_tree_construction(self):
        """depth=5, branching=4 can be constructed without error (total > 17 allowed)."""
        ql.circuit()
        tree = QWalkTree(max_depth=5, branching=4)
        # 6 height qubits + 5 * 2 branch qubits = 16
        assert tree.total_qubits == 16

    def test_very_large_tree_construction(self):
        """A tree exceeding 17 qubits can be constructed (budget is simulation-only)."""
        ql.circuit()
        # depth=5, branching=8 -> 6 height + 5*3 branch = 21 qubits
        tree = QWalkTree(max_depth=5, branching=8)
        assert tree.total_qubits == 21
        # No error at construction

    def test_named_attributes(self):
        """tree.height_register is qint, tree.branch_registers is list, etc."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)

        assert isinstance(tree.height_register, ql.qint)
        assert isinstance(tree.branch_registers, list)
        assert all(isinstance(r, ql.qint) for r in tree.branch_registers)
        assert tree.max_depth == 2
        assert tree.branching == [2, 2]

    def test_tree_node_accessor(self):
        """tree.node returns TreeNode with .depth and .branch_values."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)

        node = tree.node
        assert isinstance(node, TreeNode)
        # depth should be the height register
        assert node.depth is tree.height_register
        # branch_values should be a list of qints matching branch_registers
        bv = node.branch_values
        assert len(bv) == 2
        assert all(isinstance(v, ql.qint) for v in bv)

    def test_branching_normalization_int(self):
        """Integer branching is expanded to per-level list."""
        ql.circuit()
        tree = QWalkTree(max_depth=3, branching=2)
        assert tree.branching == [2, 2, 2]

    def test_branching_normalization_tuple(self):
        """Tuple branching is accepted and normalized to list."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=(2, 3))
        assert tree.branching == [2, 3]

    def test_invalid_branching_type(self):
        """Non-int/list branching raises ValueError."""
        ql.circuit()
        with pytest.raises(ValueError, match="branching must be int or list"):
            QWalkTree(max_depth=2, branching="invalid")


# ---------------------------------------------------------------------------
# Group 2: Root State Preparation Tests
# ---------------------------------------------------------------------------


class TestRootStatePreparation:
    """TREE-03: Root state preparation and statevector verification."""

    def test_root_state_statevector(self):
        """After construction, only the root height qubit is |1>.

        For QWalkTree(max_depth=2, branching=2) with 5 qubits total:
        height register h[0..2] (3 qubits) + branch b0 (1 qubit) + b1 (1 qubit)
        Root state has h[2]=|1>, all else |0>.
        """
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)

        qasm_str = ql.to_openqasm()
        sv = _simulate_statevector(qasm_str)

        # Find which physical qubit is the root height qubit (h[max_depth])
        # h[max_depth] = h[2] is at qubits[63] (MSB of the allocated width)
        root_phys_qubit = int(tree.height_register.qubits[63])

        # The statevector has 2^n entries for n qubits
        # Find the basis state index where only root_phys_qubit is |1>
        n_qubits = len(sv)

        # In Qiskit's convention, qubit 0 is LSB of the state index
        # So qubit k set to |1> means bit k of the index is 1
        root_idx = 1 << root_phys_qubit

        # Root state should have amplitude 1.0 at root_idx
        assert abs(sv[root_idx]) == pytest.approx(1.0, abs=1e-10), (
            f"Expected amplitude 1.0 at index {root_idx}, got {abs(sv[root_idx])}"
        )

        # All other amplitudes should be 0
        for i in range(n_qubits):
            if i != root_idx:
                assert abs(sv[i]) == pytest.approx(0.0, abs=1e-10), (
                    f"Expected 0 amplitude at index {i}, got {abs(sv[i])}"
                )

    def test_root_height_qubit_position(self):
        """Root height qubit h[max_depth] is at qubits[63] (MSB)."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)

        # The root height qubit (bit max_depth=2 of height register)
        # should be at qubits[63] which is the MSB of the allocated width
        root_qubit_idx = tree.height_register.qubits[63]
        # It should be a valid physical qubit index (non-negative)
        assert root_qubit_idx >= 0


# ---------------------------------------------------------------------------
# Group 3: Qubit Budget Tests
# ---------------------------------------------------------------------------


class TestQubitBudget:
    """TREE-02: Simulation-time qubit budget enforcement."""

    def test_budget_check_raises(self):
        """_check_qubit_budget() raises ValueError when over budget."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2, max_qubits=4)
        # total_qubits=5 > max_qubits=4
        with pytest.raises(ValueError, match="simulation budget"):
            tree._check_qubit_budget()

    def test_budget_check_passes(self):
        """_check_qubit_budget() passes when within budget."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2, max_qubits=17)
        # total_qubits=5 <= max_qubits=17, no error
        tree._check_qubit_budget()

    def test_custom_max_qubits(self):
        """QWalkTree(max_qubits=100) sets max_qubits to 100."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2, max_qubits=100)
        assert tree.max_qubits == 100

    def test_default_max_qubits(self):
        """Default max_qubits is 17."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2)
        assert tree.max_qubits == 17
