"""Internal gate primitives for Grover's algorithm.

This module provides low-level gate emission functions used by:
- qint.branch() / qbool.branch() for Ry rotations
- Diffusion operator (Phase 78) for MCZ pattern

Not part of public API - users should use branch() method instead.
"""

cimport cython
from libc.string cimport memset

from ._core cimport (
    circuit_t, circuit_s, gate_t,
    add_gate,
)
from ._core import (
    _get_circuit, _get_controlled, _get_control_bool,
)

# C declarations from gate.h
cdef extern from "gate.h":
    void h(gate_t *g, unsigned int target)
    void z(gate_t *g, unsigned int target)
    void ry(gate_t *g, unsigned int target, double angle)
    void ch(gate_t *g, unsigned int target, unsigned int control)
    void cz(gate_t *g, unsigned int target, unsigned int control)
    void cry(gate_t *g, unsigned int target, unsigned int control, double angle)


cpdef void emit_ry(unsigned int target, double angle):
    """Emit Ry gate to circuit (internal use only).

    Handles controlled context automatically - if inside a `with qbool:` block,
    emits CRy instead of Ry.
    """
    cdef gate_t g
    cdef circuit_t *circ = <circuit_t*><unsigned long long>_get_circuit()

    memset(&g, 0, sizeof(gate_t))

    if _get_controlled():
        # Inside controlled context - emit CRy
        ctrl = _get_control_bool()
        # qint stores qubits right-aligned, control qubit is at qubits[63]
        from .qint import qint
        cry(&g, target, (<qint>ctrl).qubits[63], angle)
    else:
        ry(&g, target, angle)

    add_gate(circ, &g)


cpdef void emit_h(unsigned int target):
    """Emit H gate to circuit (internal use only)."""
    cdef gate_t g
    cdef circuit_t *circ = <circuit_t*><unsigned long long>_get_circuit()

    memset(&g, 0, sizeof(gate_t))

    if _get_controlled():
        ctrl = _get_control_bool()
        from .qint import qint
        ch(&g, target, (<qint>ctrl).qubits[63])
    else:
        h(&g, target)

    add_gate(circ, &g)


cpdef void emit_z(unsigned int target):
    """Emit Z gate to circuit (internal use only)."""
    cdef gate_t g
    cdef circuit_t *circ = <circuit_t*><unsigned long long>_get_circuit()

    memset(&g, 0, sizeof(gate_t))

    if _get_controlled():
        ctrl = _get_control_bool()
        from .qint import qint
        cz(&g, target, (<qint>ctrl).qubits[63])
    else:
        z(&g, target)

    add_gate(circ, &g)
