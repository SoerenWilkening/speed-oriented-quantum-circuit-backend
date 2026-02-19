# _gates.pxd - Cython declarations for internal gate primitives

from libc.stdint cimport uint32_t

# Forward declaration for gate_t (from types.h)
cdef extern from "types.h":
    ctypedef unsigned int qubit_t

cdef extern from "gate.h":
    ctypedef struct gate_t:
        pass
    void h(gate_t *g, qubit_t target)
    void z(gate_t *g, qubit_t target)
    void ry(gate_t *g, qubit_t target, double angle)
    void ch(gate_t *g, qubit_t target, qubit_t control)
    void cz(gate_t *g, qubit_t target, qubit_t control)
    void cry(gate_t *g, qubit_t target, qubit_t control, double angle)
