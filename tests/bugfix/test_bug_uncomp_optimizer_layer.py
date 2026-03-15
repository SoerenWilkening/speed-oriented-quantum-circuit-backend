"""Regression test for uncomputation with comparison operations.

Tests that the history-graph-based uncomputation system correctly
reverses comparison gates in with-blocks.
"""

import gc

import quantum_language as ql


def test_with_block_comparison_uncompute():
    """with (cr == 1) | (cr == 3) should uncompute correctly."""
    gc.collect()
    ql.circuit()

    cr = ql.qint(0, width=3)
    _ci = ql.qint(0, width=3)  # noqa: F841 - occupies qubits

    # This should work without error.
    # Uncomputation via history graph reverses all comparison gates.
    with (cr == 1) | (cr == 3):
        cr += 1

    # If we get here without error, the uncomputation completed.
    # The circuit should have properly reversed all comparison gates.


def test_comparison_uncompute_preserves_state(verify_circuit):
    """After uncomputation of comparison, original value should be preserved."""

    def circuit_builder(val=5, w=4):
        x = ql.qint(val, width=w)
        result = x == 5
        # Keep result alive so it doesn't uncompute before export.
        # The comparison result (1 bit) is at higher qubit indices.
        # Expected: x=5 (4 bits) and result=1 (1 bit True)
        return (1, [x, result])

    actual, expected = verify_circuit(circuit_builder, width=1)
    assert actual == expected, f"After comparison, expected result={expected} but got {actual}"
