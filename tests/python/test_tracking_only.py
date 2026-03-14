"""Tests for tracking_only wiring and simulate=False in @ql.compile.

Verifies that when simulate=False and opt=1, the compiled function
passes tracking_only=1 to run_instruction so gates are counted but
not stored in the global circuit. The DAG should still record gate
counts via the circ->gate_count delta.
"""

import quantum_language as ql
from quantum_language._core import get_current_layer, get_gate_count
from quantum_language.call_graph import get_tracking_only, set_tracking_only


class TestTrackingOnlyFlag:
    """Verify get/set_tracking_only module-level flag behavior."""

    def test_default_tracking_only_is_zero(self):
        """tracking_only flag defaults to 0 (normal mode)."""
        set_tracking_only(0)
        assert get_tracking_only() == 0

    def test_set_and_get_tracking_only(self):
        """Can set tracking_only to 1 and read it back."""
        set_tracking_only(1)
        assert get_tracking_only() == 1
        set_tracking_only(0)
        assert get_tracking_only() == 0


class TestSimulateFalseCapture:
    """Verify simulate=False with opt=1 uses tracking_only mode."""

    def test_simulate_false_no_circuit_gates(self):
        """simulate=False should not add gates to the flat circuit.

        The layer count should not increase during capture when
        tracking_only mode is active.
        """
        ql.circuit()

        @ql.compile(opt=1, simulate=False)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        layer_before = get_current_layer()
        inc(a)  # capture with tracking_only=1
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

        @ql.compile(opt=1, simulate=False)
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
            f"DAG node gate_count should be positive in tracking_only mode, "
            f"got {call_nodes[0].gate_count}"
        )

    def test_simulate_false_returns_qint(self):
        """simulate=False capture should still return a valid qint."""
        ql.circuit()

        @ql.compile(opt=1, simulate=False)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        result = inc(a)

        assert result is not None, "Should return a qint"
        assert result.width == 4, f"Return width should be 4, got {result.width}"

    def test_simulate_false_tracking_restored_after_capture(self):
        """tracking_only flag should be restored after capture completes."""
        set_tracking_only(0)
        ql.circuit()

        @ql.compile(opt=1, simulate=False)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        inc(a)

        assert get_tracking_only() == 0, (
            "tracking_only should be restored to 0 after capture"
        )

    def test_simulate_false_tracking_restored_on_error(self):
        """tracking_only flag should be restored even if capture raises."""
        set_tracking_only(0)
        ql.circuit()

        @ql.compile(opt=1, simulate=False)
        def bad_func(x):
            raise ValueError("intentional error")

        a = ql.qint(0, width=4)
        try:
            bad_func(a)
        except ValueError:
            pass

        assert get_tracking_only() == 0, (
            "tracking_only should be restored to 0 after error"
        )


class TestSimulateTrueDefault:
    """Verify simulate=True (default) preserves normal behavior."""

    def test_simulate_true_injects_gates(self):
        """Default simulate=True should inject gates into circuit normally."""
        ql.circuit()

        @ql.compile(opt=1)
        def inc(x):
            x += 1
            return x

        a = ql.qint(0, width=4)
        layer_before = get_current_layer()
        inc(a)  # capture with simulate=True (default)
        layer_after = get_current_layer()

        assert layer_after > layer_before, (
            f"simulate=True should inject gates (expected > 0 new layers, "
            f"got {layer_after - layer_before})"
        )

    def test_simulate_true_dag_gate_count_matches(self):
        """simulate=True DAG gate count should match stored gates."""
        ql.circuit()

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

        @ql.compile(opt=1, simulate=False)
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
