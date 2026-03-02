"""Phase 97-02: Predicate interface and validation tests.

Tests predicate callable validation, mutual exclusion checking, compiled
predicate detection, and uncomputation support for QWalkTree.

Requirements: PRED-01, PRED-02, PRED-03
"""

import pytest

import quantum_language as ql
from quantum_language.compile import CompiledFunc
from quantum_language.walk import QWalkTree, TreeNode

# ---------------------------------------------------------------------------
# Group 1: Predicate Interface Tests
# ---------------------------------------------------------------------------


class TestPredicateInterface:
    """PRED-01: Predicate callable receives TreeNode and returns qbool tuple."""

    def test_predicate_accept_false_reject_false(self):
        """Raw callable returning (False, False) is accepted."""
        ql.circuit()

        def pred(node):
            return (ql.qbool(False), ql.qbool(False))

        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        assert tree._accept.measure() == 0
        assert tree._reject.measure() == 0

    def test_predicate_accept_true_reject_false(self):
        """Raw callable returning (True, False) is accepted (root is a solution)."""
        ql.circuit()

        def pred(node):
            return (ql.qbool(True), ql.qbool(False))

        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        assert tree._accept.measure() == 1
        assert tree._reject.measure() == 0

    def test_predicate_accept_false_reject_true(self):
        """Raw callable returning (False, True) is accepted (root is rejected)."""
        ql.circuit()

        def pred(node):
            return (ql.qbool(False), ql.qbool(True))

        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        assert tree._accept.measure() == 0
        assert tree._reject.measure() == 1

    def test_predicate_no_predicate(self):
        """QWalkTree with predicate=None skips validation (no error)."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2, predicate=None)
        assert tree._predicate is None

    def test_predicate_receives_tree_node(self):
        """Predicate receives a TreeNode with correct .depth and .branch_values."""
        ql.circuit()
        received_node = [None]

        def pred(node):
            received_node[0] = node
            return (ql.qbool(False), ql.qbool(False))

        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        node = received_node[0]
        assert isinstance(node, TreeNode)
        # depth is the height register (width = max_depth + 1 = 3)
        assert node.depth.width == tree.max_depth + 1
        # branch_values has max_depth entries
        assert len(node.branch_values) == 2

    def test_accept_reject_stored(self):
        """After construction with predicate, _accept and _reject are qbool instances."""
        ql.circuit()

        def pred(node):
            return (ql.qbool(False), ql.qbool(False))

        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        assert isinstance(tree._accept, ql.qbool)
        assert isinstance(tree._reject, ql.qbool)


# ---------------------------------------------------------------------------
# Group 2: Mutual Exclusion Validation
# ---------------------------------------------------------------------------


class TestPredicateMutualExclusion:
    """PRED-02: Predicate mutual exclusion validation."""

    def test_mutual_exclusion_violation(self):
        """Predicate returning (True, True) raises ValueError."""
        ql.circuit()

        def pred(node):
            return (ql.qbool(True), ql.qbool(True))

        with pytest.raises(ValueError, match="mutual exclusion"):
            QWalkTree(max_depth=2, branching=2, predicate=pred)

    def test_predicate_wrong_return_type_not_tuple(self):
        """Predicate returning a single qbool raises TypeError."""
        ql.circuit()

        def pred(node):
            return ql.qbool(False)

        with pytest.raises(TypeError):
            QWalkTree(max_depth=2, branching=2, predicate=pred)

    def test_predicate_wrong_return_length(self):
        """Predicate returning tuple of 3 qbools raises TypeError."""
        ql.circuit()

        def pred(node):
            return (ql.qbool(False), ql.qbool(False), ql.qbool(False))

        with pytest.raises(TypeError):
            QWalkTree(max_depth=2, branching=2, predicate=pred)


# ---------------------------------------------------------------------------
# Group 3: Compiled Predicate Tests
# ---------------------------------------------------------------------------


class TestCompiledPredicate:
    """PRED-03: Compiled predicate detection and uncomputation."""

    def test_compiled_predicate_accepted(self):
        """@ql.compile decorated predicate is accepted and detected."""
        ql.circuit()

        @ql.compile
        def pred(node):
            return (ql.qbool(False), ql.qbool(False))

        assert isinstance(pred, CompiledFunc)
        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        assert tree._predicate_is_compiled is True

    def test_raw_predicate_not_compiled(self):
        """Raw callable has _predicate_is_compiled = False."""
        ql.circuit()

        def pred(node):
            return (ql.qbool(False), ql.qbool(False))

        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        assert tree._predicate_is_compiled is False

    def test_uncompute_predicate_compiled(self):
        """uncompute_predicate() on compiled predicate calls adjoint without error."""
        ql.circuit()

        @ql.compile
        def pred(node):
            return (ql.qbool(False), ql.qbool(False))

        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        # Should not raise
        tree.uncompute_predicate()

    def test_uncompute_predicate_no_predicate(self):
        """uncompute_predicate() with predicate=None is a no-op."""
        ql.circuit()
        tree = QWalkTree(max_depth=2, branching=2, predicate=None)
        # Should not raise
        tree.uncompute_predicate()

    def test_uncompute_predicate_raw_is_noop(self):
        """uncompute_predicate() on raw callable is a no-op (user manages cleanup)."""
        ql.circuit()

        def pred(node):
            return (ql.qbool(False), ql.qbool(False))

        tree = QWalkTree(max_depth=2, branching=2, predicate=pred)
        # Should not raise -- raw predicates skip uncompute
        tree.uncompute_predicate()
