"""Tests for simulate option and tracking-only mode in @ql.compile.

Verifies that when ql.option('simulate') is False (default), gates are
counted but not stored in the global circuit.  The DAG should still
record gate counts via the circ->gate_count delta.
"""

import quantum_language as ql
from quantum_language._core import get_current_layer, get_gate_count


class TestSimulateOption:
    """Verify ql.option('simulate') get/set behavior."""

    def test_default_simulate_is_false(self):
        """simulate defaults to False."""
        ql.circuit()
        assert ql.option('simulate') is False

    def test_set_simulate_true(self):
        """Can set simulate to True and read it back."""
        ql.circuit()
        ql.option('simulate', True)
        assert ql.option('simulate') is True

    def test_set_simulate_false(self):
        """Can set simulate to False and read it back."""
        ql.circuit()
        ql.option('simulate', True)
        ql.option('simulate', False)
        assert ql.option('simulate') is False


class TestSimulateFalseCapture:
    """Verify simulate=False uses tracking-only mode."""

    def test_simulate_false_no_circuit_gates(self):
        """simulate=False should not add gates to the flat circuit.

        The layer count should not increase during capture when
        tracking-only mode is active.
        """
        ql.circuit()

        @ql.compile(opt=1)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        layer_before = get_current_layer()
        inc(a)
        layer_after = get_current_layer()

        assert layer_after == layer_before, (
            f"simulate=False should not inject gates into circuit "
            f"(expected 0 new layers, got {layer_after - layer_before})"
        )

    def test_simulate_false_dag_has_gate_count(self):
        """simulate=False should still record gate counts in the DAG.

        The DAG node gate_count should be positive even though no gates
        were stored in the circuit.
        """
        ql.circuit()

        @ql.compile(opt=1)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        inc(a)

        dag = inc._call_graph
        assert dag is not None, "opt=1 should build a call graph DAG"
        assert dag.node_count >= 1, "DAG should have at least 1 node"

        # Find call nodes (not operation-type nodes)
        call_nodes = [
            n for n in dag.nodes
            if not n.operation_type or n.operation_type == ""
        ]
        assert len(call_nodes) >= 1, "Should have at least 1 call node"
        assert call_nodes[0].gate_count > 0, (
            f"DAG node gate_count should be positive in tracking-only mode, "
            f"got {call_nodes[0].gate_count}"
        )

    def test_simulate_false_returns_qint(self):
        """simulate=False capture should still return a valid qint."""
        ql.circuit()

        @ql.compile(opt=1)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        result = inc(a)

        assert result is not None, "Should return a qint"
        assert result.width == 4, f"Return width should be 4, got {result.width}"


class TestSimulateTrueExplicit:
    """Verify simulate=True enables normal gate injection."""

    def test_simulate_true_injects_gates(self):
        """ql.option('simulate', True) should inject gates into circuit."""
        ql.circuit()
        ql.option('simulate', True)

        @ql.compile(opt=1)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        layer_before = get_current_layer()
        inc(a)
        layer_after = get_current_layer()

        assert layer_after > layer_before, (
            f"simulate=True should inject gates (expected > 0 new layers, "
            f"got {layer_after - layer_before})"
        )

    def test_simulate_true_dag_gate_count_matches(self):
        """simulate=True DAG gate count should match stored gates."""
        ql.circuit()
        ql.option('simulate', True)

        @ql.compile(opt=1)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        inc(a)

        dag = inc._call_graph
        assert dag is not None
        call_nodes = [
            n for n in dag.nodes
            if not n.operation_type or n.operation_type == ""
        ]
        assert len(call_nodes) >= 1
        assert call_nodes[0].gate_count > 0


class TestSimulateFalseAggregate:
    """Verify aggregate gate counts work correctly with simulate=False."""

    def test_multiple_calls_aggregate(self):
        """Multiple calls with simulate=False should aggregate gate counts."""
        ql.circuit()

        @ql.compile(opt=1)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        inc(a)  # capture (call 1)

        b = ql.qint(0, width=4)
        inc(b)  # replay (call 2)

        dag = inc._call_graph
        assert dag is not None

        agg = dag.aggregate()
        assert agg["gates"] > 0, "Aggregate should have positive gate count"

        # All call nodes should have the same gate count
        call_nodes = [
            n for n in dag.nodes
            if not n.operation_type or n.operation_type == ""
        ]
        gate_counts = [n.gate_count for n in call_nodes]
        if len(gate_counts) >= 2:
            assert len(set(gate_counts)) == 1, (
                f"All call nodes should have same gate count, got {gate_counts}"
            )
            assert agg["gates"] == len(gate_counts) * gate_counts[0], (
                f"Aggregate should be {len(gate_counts)}x per-call "
                f"({gate_counts[0]}), got {agg['gates']}"
            )
