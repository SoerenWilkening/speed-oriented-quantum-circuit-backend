"""Unit tests for selective sequence merging (merge_groups and merge infrastructure).

Tests merge candidate detection via CallGraphDAG.merge_groups() and
gate concatenation/optimization via _merge_and_optimize.
"""

from quantum_language.call_graph import (
    CallGraphDAG,
)

# ---------------------------------------------------------------------------
# merge_groups tests
# ---------------------------------------------------------------------------


class TestMergeGroups:
    """Tests for CallGraphDAG.merge_groups(threshold)."""

    def test_empty_dag_returns_empty(self):
        dag = CallGraphDAG()
        assert dag.merge_groups() == []

    def test_single_node_returns_empty(self):
        dag = CallGraphDAG()
        dag.add_node("f", {0, 1}, 5, ("k",))
        assert dag.merge_groups() == []

    def test_two_overlapping_nodes(self):
        dag = CallGraphDAG()
        dag.add_node("f", {0, 1, 2}, 5, ("k1",))
        dag.add_node("g", {1, 2, 3}, 5, ("k2",))
        result = dag.merge_groups()
        assert result == [[0, 1]]

    def test_two_disjoint_nodes_returns_empty(self):
        dag = CallGraphDAG()
        dag.add_node("f", {0, 1}, 5, ("k1",))
        dag.add_node("g", {2, 3}, 5, ("k2",))
        assert dag.merge_groups() == []

    def test_chain_a_b_c_transitive(self):
        """A overlaps B, B overlaps C, A disjoint from C -> single group."""
        dag = CallGraphDAG()
        dag.add_node("a", {0, 1}, 5, ("k1",))  # qubits 0,1
        dag.add_node("b", {1, 2}, 5, ("k2",))  # overlaps a (qubit 1), overlaps c (qubit 2)
        dag.add_node("c", {2, 3}, 5, ("k3",))  # disjoint from a
        result = dag.merge_groups()
        assert result == [[0, 1, 2]]

    def test_threshold_filters_low_overlap(self):
        """threshold=3 excludes edges with overlap < 3."""
        dag = CallGraphDAG()
        dag.add_node("f", {0, 1, 2, 3, 4}, 5, ("k1",))  # 5 qubits
        dag.add_node("g", {3, 4}, 5, ("k2",))  # overlap = 2 with f
        # With default threshold=1, they merge
        assert dag.merge_groups(threshold=1) == [[0, 1]]
        # With threshold=3, overlap of 2 is insufficient
        assert dag.merge_groups(threshold=3) == []

    def test_groups_sorted_by_node_index(self):
        """Each group's node indices are in ascending order."""
        dag = CallGraphDAG()
        dag.add_node("c", {2, 3}, 5, ("k3",))
        dag.add_node("a", {0, 1, 2}, 5, ("k1",))
        dag.add_node("b", {1, 3}, 5, ("k2",))
        result = dag.merge_groups()
        for group in result:
            assert group == sorted(group)

    def test_mix_overlapping_and_disjoint(self):
        """Only multi-node groups returned; singletons excluded."""
        dag = CallGraphDAG()
        dag.add_node("a", {0, 1}, 5, ("k1",))  # overlaps b
        dag.add_node("b", {1, 2}, 5, ("k2",))  # overlaps a
        dag.add_node("c", {10, 11}, 5, ("k3",))  # disjoint from all
        result = dag.merge_groups()
        assert result == [[0, 1]]

    def test_two_separate_groups(self):
        """Two independent overlap clusters -> two groups."""
        dag = CallGraphDAG()
        dag.add_node("a", {0, 1}, 5, ("k1",))
        dag.add_node("b", {1, 2}, 5, ("k2",))
        dag.add_node("c", {10, 11}, 5, ("k3",))
        dag.add_node("d", {11, 12}, 5, ("k4",))
        result = dag.merge_groups()
        assert len(result) == 2
        assert [0, 1] in result
        assert [2, 3] in result
