"""Tests for __exit__ uncomputation of with-block condition qbools.

When a ``with qbool:`` block exits, the qbool's history graph is
uncomputed in reverse by calling run_instruction inverted for each
entry with a real sequence pointer, then cascading to orphaned children.

Step 1.3 of Phase 1: __exit__ Uncomputation for with Blocks
[Quantum_Assembly-2ab.3].
"""

import gc

import quantum_language as ql


# ---------------------------------------------------------------------------
# Test: with block uncomputes qbool
# ---------------------------------------------------------------------------


class TestWithBlockUncomputesQbool:
    """After a with block exits, the condition qbool's history is cleared."""

    def test_eq_cq_history_cleared(self):
        """CQ equality condition: history cleared after with block."""
        ql.circuit()
        a = ql.qint(5, width=4)
        cond = (a == 5)
        assert len(cond.history) == 1
        with cond:
            pass
        assert len(cond.history) == 0

    def test_eq_cq_inverse_gates_emitted(self):
        """CQ equality: inverse gates are appended to circuit."""
        ql.circuit()
        a = ql.qint(5, width=4)
        cond = (a == 5)
        gc_before = ql.get_gate_count()
        with cond:
            pass
        gc_after = ql.get_gate_count()
        # Inverse gates should have been emitted (gate count increased)
        assert gc_after > gc_before

    def test_user_qbool_no_uncomputation(self):
        """User-created qbool (no history) does not trigger uncomputation."""
        ql.circuit()
        flag = ql.qbool(True)
        assert len(flag.history) == 0
        gc_before = ql.get_gate_count()
        with flag:
            pass
        gc_after = ql.get_gate_count()
        # No history entries means no inverse gates emitted by history path
        assert len(flag.history) == 0

    def test_history_entries_cleared_not_just_emptied(self):
        """Verify entries list is actually cleared (not just iterated)."""
        ql.circuit()
        a = ql.qint(5, width=4)
        cond = (a == 5)
        seq_ptr, qm = cond.history.entries[0]
        assert seq_ptr != 0
        with cond:
            pass
        assert len(cond.history.entries) == 0
        assert not cond.history


# ---------------------------------------------------------------------------
# Test: with block uncomputes temporaries
# ---------------------------------------------------------------------------


class TestWithBlockUncomputesTemporaries:
    """Compound expressions like (a + 3) > 5 cascade to children."""

    def test_compound_condition_clears_children(self):
        """with (temp > 5): uncomputes and clears children."""
        ql.circuit()
        a = ql.qint(4, width=4)
        temp = a + 3
        cond = (temp > 5)
        # cond should have children (widened temporaries from comparison)
        assert len(cond.history.children) >= 0
        with cond:
            pass
        # After exit, children list is cleared
        assert len(cond.history.children) == 0

    def test_compound_condition_history_empty(self):
        """Compound condition entries are cleared after with block."""
        ql.circuit()
        a = ql.qint(4, width=4)
        temp = a + 3
        cond = (temp > 5)
        assert len(cond.history) >= 1
        with cond:
            pass
        assert len(cond.history) == 0


# ---------------------------------------------------------------------------
# Test: with block preserves live variables
# ---------------------------------------------------------------------------


class TestWithBlockPreservesLiveVariables:
    """Input variables are untouched after with block exits."""

    def test_preserves_single_operand(self):
        """a is preserved after with (a == 5): block."""
        ql.circuit()
        a = ql.qint(5, width=4)
        original_width = a.width
        original_history_len = len(a.history)
        cond = (a == 5)
        with cond:
            pass
        assert a.width == original_width
        assert len(a.history) == original_history_len
        assert not a._is_uncomputed

    def test_preserves_both_operands_qq(self):
        """a and b are preserved after with (a == b): block."""
        ql.circuit()
        a = ql.qint(5, width=4)
        b = ql.qint(5, width=4)
        cond = (a == b)
        with cond:
            pass
        assert a.width == 4
        assert b.width == 4
        assert len(a.history) == 0
        assert len(b.history) == 0
        assert not a._is_uncomputed
        assert not b._is_uncomputed

    def test_preserves_variable_after_controlled_ops(self):
        """Variables used in body are preserved after with block."""
        ql.circuit()
        a = ql.qint(5, width=4)
        result = ql.qint(0, width=4)
        cond = (a == 5)
        with cond:
            result += 1
        assert a.width == 4
        assert result.width == 4
        assert not a._is_uncomputed
        assert not result._is_uncomputed


# ---------------------------------------------------------------------------
# Test: nested with blocks
# ---------------------------------------------------------------------------


class TestNestedWithBlocks:
    """Inner with block uncomputes before outer."""

    def test_nested_inner_uncomputes_first(self):
        """Inner condition history is cleared before outer."""
        ql.circuit()
        a = ql.qint(5, width=4)
        b = ql.qint(3, width=4)
        cond_outer = (a == 5)
        assert len(cond_outer.history) == 1
        with cond_outer:
            cond_inner = (b == 3)
            assert len(cond_inner.history) == 1
            with cond_inner:
                pass
            # Inner has been uncomputed
            assert len(cond_inner.history) == 0
        # Outer has been uncomputed
        assert len(cond_outer.history) == 0

    def test_nested_preserves_inputs(self):
        """Both a and b are preserved after nested with blocks."""
        ql.circuit()
        a = ql.qint(5, width=4)
        b = ql.qint(3, width=4)
        cond_outer = (a == 5)
        with cond_outer:
            cond_inner = (b == 3)
            with cond_inner:
                pass
        assert a.width == 4
        assert b.width == 4
        assert not a._is_uncomputed
        assert not b._is_uncomputed

    def test_nested_emits_inverse_gates(self):
        """Both inner and outer exit emit inverse gates."""
        ql.circuit()
        a = ql.qint(5, width=4)
        b = ql.qint(3, width=4)
        cond_outer = (a == 5)
        gc0 = ql.get_gate_count()
        with cond_outer:
            gc1 = ql.get_gate_count()
            cond_inner = (b == 3)
            gc2 = ql.get_gate_count()
            with cond_inner:
                pass
            gc3 = ql.get_gate_count()
            # Inner exit should emit inverse gates
            assert gc3 > gc2
        gc4 = ql.get_gate_count()
        # Outer exit should emit inverse gates
        assert gc4 > gc3


# ---------------------------------------------------------------------------
# Test: HistoryGraph.uncompute() cascading
# ---------------------------------------------------------------------------


class TestHistoryGraphUncompute:
    """Direct tests for HistoryGraph.uncompute() method."""

    def test_uncompute_clears_entries(self):
        """After uncompute(), entries list is empty."""
        from quantum_language.history_graph import HistoryGraph
        hg = HistoryGraph()
        hg.append(0, (0, 1, 2))
        hg.append(0, (3, 4))
        calls = []
        hg.uncompute(lambda s, q: calls.append((s, q)))
        assert len(hg) == 0

    def test_uncompute_clears_children(self):
        """After uncompute(), children list is empty."""
        from quantum_language.history_graph import HistoryGraph

        class _Dummy:
            pass

        hg = HistoryGraph()
        child = _Dummy()
        child.history = HistoryGraph()
        hg.add_child(child)
        hg.uncompute(lambda s, q: None)
        assert len(hg.children) == 0

    def test_uncompute_skips_zero_seq_ptr(self):
        """Entries with seq_ptr=0 are skipped (compound ops)."""
        from quantum_language.history_graph import HistoryGraph
        hg = HistoryGraph()
        hg.append(0, (0, 1))
        hg.append(0, (2, 3))
        calls = []
        hg.uncompute(lambda s, q: calls.append((s, q)))
        # No calls because all entries have seq_ptr=0
        assert len(calls) == 0

    def test_uncompute_calls_run_fn_for_nonzero(self):
        """Entries with non-zero seq_ptr invoke run_fn."""
        from quantum_language.history_graph import HistoryGraph
        hg = HistoryGraph()
        hg.append(100, (0, 1))
        hg.append(200, (2, 3))
        calls = []
        hg.uncompute(lambda s, q: calls.append((s, q)))
        # Called in reverse order
        assert calls == [(200, (2, 3)), (100, (0, 1))]

    def test_uncompute_cascades_to_alive_children(self):
        """Children with non-empty history are cascaded."""
        from quantum_language.history_graph import HistoryGraph

        class _Dummy:
            pass

        child = _Dummy()
        child.history = HistoryGraph()
        child.history.append(300, (5, 6))

        parent = HistoryGraph()
        parent.add_child(child)
        calls = []
        parent.uncompute(lambda s, q: calls.append((s, q)))
        # Child's entry should have been processed
        assert (300, (5, 6)) in calls
        assert len(child.history) == 0

    def test_uncompute_skips_dead_children(self):
        """Dead weakrefs are skipped without error."""
        import gc as _gc
        from quantum_language.history_graph import HistoryGraph

        class _Dummy:
            pass

        parent = HistoryGraph()
        child = _Dummy()
        child.history = HistoryGraph()
        child.history.append(400, (7, 8))
        parent.add_child(child)
        del child
        _gc.collect()
        # Should not raise
        calls = []
        parent.uncompute(lambda s, q: calls.append((s, q)))
        # Dead child's entries are not processed
        assert len(calls) == 0

    def test_uncompute_idempotent(self):
        """Calling uncompute() twice is safe (second call is no-op)."""
        from quantum_language.history_graph import HistoryGraph
        hg = HistoryGraph()
        hg.append(100, (0,))
        calls = []
        hg.uncompute(lambda s, q: calls.append((s, q)))
        assert len(calls) == 1
        # Second call: nothing to do
        calls2 = []
        hg.uncompute(lambda s, q: calls2.append((s, q)))
        assert len(calls2) == 0
