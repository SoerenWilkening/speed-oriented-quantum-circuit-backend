"""Comprehensive API coverage tests for Python API (TEST-03).

Tests verify that all public API methods work as documented.
Complements characterization tests by testing API contracts.
"""

import os
import sys

# noqa comments allow import after path setup
# ruff: noqa: E402

# Add python-backend to path
project_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
sys.path.insert(0, os.path.join(project_root, "python-backend"))

import quantum_language as ql


class TestCircuitAPI:
    """Test circuit class public API."""

    def test_circuit_creation(self):
        """circuit() creates circuit singleton."""
        c = ql.circuit()
        assert c is not None

    def test_circuit_visualize(self):
        """visualize() runs without error."""
        c = ql.circuit()
        _a = ql.qint(5, width=4)  # Create qubits to have gates
        c.visualize()  # Should not raise

    def test_circuit_gate_count(self):
        """gate_count returns int."""
        c = ql.circuit()
        _a = ql.qint(5, width=4)  # Create qubits to have gates
        assert isinstance(c.gate_count, int)

    def test_circuit_depth(self):
        """depth returns int."""
        c = ql.circuit()
        _a = ql.qint(5, width=4)  # Create qubits to have gates
        assert isinstance(c.depth, int)

    def test_circuit_qubit_count(self):
        """qubit_count returns int >= 0."""
        c = ql.circuit()
        _a = ql.qint(5, width=4)  # Create qubits to have gates
        assert isinstance(c.qubit_count, int)
        assert c.qubit_count >= 0

    def test_circuit_gate_counts(self):
        """gate_counts returns dict with expected keys."""
        c = ql.circuit()
        _a = ql.qint(5, width=4)  # Create qubits to have gates
        counts = c.gate_counts
        assert isinstance(counts, dict)
        assert "X" in counts
        assert "H" in counts
        assert "CNOT" in counts

    def test_circuit_available_passes(self):
        """available_passes returns list of pass names."""
        c = ql.circuit()
        passes = c.available_passes
        assert isinstance(passes, list)
        assert "merge" in passes
        assert "cancel_inverse" in passes

    def test_circuit_optimize(self):
        """optimize() returns before/after stats dict."""
        c = ql.circuit()
        a = ql.qint(5, width=4)
        b = ql.qint(3, width=4)
        _ = a + b
        stats = c.optimize()
        assert "before" in stats
        assert "after" in stats
        assert "gate_count" in stats["before"]

    def test_circuit_can_optimize(self):
        """can_optimize() returns bool."""
        c = ql.circuit()
        _a = ql.qint(5, width=4)  # Create qubits to have gates
        result = c.can_optimize()
        assert isinstance(result, bool)
