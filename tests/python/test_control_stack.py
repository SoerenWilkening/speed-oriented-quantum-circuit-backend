"""Tests for control stack primitives and Toffoli AND gate helpers.

Unit tests for:
- _control_stack push/pop/get/set in _core.pyx
- emit_ccx raw Toffoli gate emission in _gates.pyx
- _toffoli_and / _uncompute_toffoli_and ancilla helpers in _gates.pyx
- Backward-compat wrappers (_get_controlled, _set_controlled, etc.)

Requirement: CTRL-02, CTRL-03
"""

import warnings

import pytest

import quantum_language as ql
from quantum_language._core import (
    _get_control_bool,
    _get_control_stack,
    _get_controlled,
    _get_list_of_controls,
    _pop_control,
    _push_control,
    _set_control_bool,
    _set_control_stack,
    _set_controlled,
    _set_list_of_controls,
)
from quantum_language._gates import (
    _toffoli_and,
    _uncompute_toffoli_and,
    emit_ccx,
)
from quantum_language.qbool import qbool

warnings.filterwarnings("ignore", message="Value .* exceeds")


class TestControlStackBasics:
    """Tests for stack push/pop/get/set accessors."""

    def test_stack_initially_empty(self):
        """After ql.circuit(), _get_controlled() returns False,
        _get_control_bool() returns None, _get_control_stack() returns []."""
        ql.circuit()
        assert _get_controlled() is False
        assert _get_control_bool() is None
        assert _get_control_stack() == []

    def test_push_makes_controlled(self):
        """After _push_control(qbool_ref, None), _get_controlled() returns True."""
        ql.circuit()
        b = qbool()
        _push_control(b, None)
        assert _get_controlled() is True
        _pop_control()  # cleanup

    def test_push_control_bool_returns_qbool(self):
        """After _push_control(qbool_ref, None), _get_control_bool() returns qbool_ref."""
        ql.circuit()
        b = qbool()
        _push_control(b, None)
        assert _get_control_bool() is b
        _pop_control()  # cleanup

    def test_push_with_ancilla_returns_ancilla(self):
        """After _push_control(qbool_ref, ancilla), _get_control_bool() returns ancilla."""
        ql.circuit()
        b = qbool()
        anc = qbool()
        _push_control(b, anc)
        assert _get_control_bool() is anc
        _pop_control()  # cleanup

    def test_pop_restores_uncontrolled(self):
        """After push then pop, _get_controlled() returns False."""
        ql.circuit()
        b = qbool()
        _push_control(b, None)
        _pop_control()
        assert _get_controlled() is False

    def test_pop_from_empty_raises(self):
        """_pop_control() on empty stack raises RuntimeError."""
        ql.circuit()
        with pytest.raises(RuntimeError):
            _pop_control()

    def test_pop_returns_entry(self):
        """_pop_control() returns the (qbool_ref, ancilla) tuple that was pushed."""
        ql.circuit()
        b = qbool()
        anc = qbool()
        _push_control(b, anc)
        entry = _pop_control()
        assert entry == (b, anc)

    def test_nested_push_pop(self):
        """Push A, push B, pop returns B, _get_control_bool() returns A, pop returns A, stack empty."""
        ql.circuit()
        a = qbool()
        b = qbool()
        _push_control(a, None)
        _push_control(b, None)

        entry_b = _pop_control()
        assert entry_b == (b, None)
        assert _get_control_bool() is a

        entry_a = _pop_control()
        assert entry_a == (a, None)
        assert _get_controlled() is False

    def test_set_control_stack_replaces(self):
        """_set_control_stack([]) clears stack, _set_control_stack(saved) restores it."""
        ql.circuit()
        b = qbool()
        _push_control(b, None)
        saved = list(_get_control_stack())  # shallow copy

        _set_control_stack([])
        assert _get_controlled() is False
        assert _get_control_stack() == []

        _set_control_stack(saved)
        assert _get_controlled() is True
        assert _get_control_bool() is b
        _pop_control()  # cleanup

    def test_circuit_reset_clears_stack(self):
        """After push then ql.circuit(), stack is empty."""
        ql.circuit()
        b = qbool()
        _push_control(b, None)
        assert _get_controlled() is True

        ql.circuit()
        assert _get_controlled() is False
        assert _get_control_stack() == []


class TestEmitCCX:
    """Tests for emit_ccx raw Toffoli gate emission."""

    def test_emit_ccx_gate_count(self):
        """After ql.circuit() and emit_ccx(target, ctrl1, ctrl2), circuit has exactly 1 gate (CCX)."""
        ql.circuit()
        # Allocate qubits via qbools to get valid qubit indices
        q_target = qbool()
        q_ctrl1 = qbool()
        q_ctrl2 = qbool()

        target_qubit = q_target.qubits[63]
        ctrl1_qubit = q_ctrl1.qubits[63]
        ctrl2_qubit = q_ctrl2.qubits[63]

        emit_ccx(target_qubit, ctrl1_qubit, ctrl2_qubit)

        # Verify exactly 1 CCX gate was emitted
        c_ref = ql.circuit.__new__(ql.circuit)
        gate_counts = c_ref.gate_counts
        assert gate_counts["CCX"] == 1


class TestToffoliAnd:
    """Tests for _toffoli_and and _uncompute_toffoli_and."""

    def test_toffoli_and_allocation(self):
        """_toffoli_and(ctrl1, ctrl2) returns a qbool with allocated_qubits=True."""
        ql.circuit()
        q1 = qbool()
        q2 = qbool()

        ancilla = _toffoli_and(q1.qubits[63], q2.qubits[63])

        assert isinstance(ancilla, qbool)
        assert ancilla.allocated_qubits is True

    def test_toffoli_and_gate_emitted(self):
        """After _toffoli_and, circuit gate count increases by 1 (one CCX)."""
        ql.circuit()
        q1 = qbool()
        q2 = qbool()

        _toffoli_and(q1.qubits[63], q2.qubits[63])

        c_ref = ql.circuit.__new__(ql.circuit)
        gate_counts = c_ref.gate_counts
        assert gate_counts["CCX"] == 1

    def test_uncompute_toffoli_and(self):
        """After _uncompute_toffoli_and(ancilla, ctrl1, ctrl2),
        ancilla._is_uncomputed is True and one more CCX gate emitted."""
        ql.circuit()
        q1 = qbool()
        q2 = qbool()

        ancilla = _toffoli_and(q1.qubits[63], q2.qubits[63])
        _uncompute_toffoli_and(ancilla, q1.qubits[63], q2.qubits[63])

        assert ancilla._is_uncomputed is True
        # Should have 2 CCX gates: 1 from _toffoli_and + 1 from _uncompute
        c_ref = ql.circuit.__new__(ql.circuit)
        gate_counts = c_ref.gate_counts
        assert gate_counts["CCX"] == 2


class TestBackwardCompat:
    """Tests for backward-compat wrapper functions."""

    def test_backward_compat_set_controlled_noop(self):
        """_set_controlled(True) and _set_controlled(False) are no-ops."""
        ql.circuit()
        # Should not crash
        _set_controlled(True)
        _set_controlled(False)
        # Stack should still be empty (no-op doesn't affect stack)
        assert _get_controlled() is False

    def test_backward_compat_set_control_bool_noop(self):
        """_set_control_bool(value) is no-op."""
        ql.circuit()
        b = qbool()
        _set_control_bool(b)
        # Should be no-op -- control_bool is derived from stack, not set directly
        assert _get_control_bool() is None

    def test_backward_compat_get_list_of_controls(self):
        """_get_list_of_controls() returns []."""
        ql.circuit()
        assert _get_list_of_controls() == []

    def test_backward_compat_set_list_of_controls(self):
        """_set_list_of_controls([]) is no-op (doesn't crash)."""
        ql.circuit()
        _set_list_of_controls([])
        _set_list_of_controls([1, 2, 3])
        # Should not crash and list_of_controls still returns []
        assert _get_list_of_controls() == []
