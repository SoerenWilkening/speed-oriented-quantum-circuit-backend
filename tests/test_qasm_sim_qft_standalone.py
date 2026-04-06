#!/usr/bin/env python3
"""
QASM simulation round-trip tests for the bare QFT/IQFT builders.

Issue: refactor-6s9 (PLAN_qft_standalone.md Step 4)

Builds a circuit that prepares a basis state |x>, applies qc_qft followed
by qc_iqft, exports to OpenQASM 3, simulates with qiskit-aer, and asserts
deterministic recovery of |x>. Exercises both contiguous and
non-contiguous qmaps.
"""

import ctypes
import os
import sys
import unittest

from qiskit.qasm3 import loads as qasm3_loads
from qiskit_aer import AerSimulator
from qiskit import transpile

# ---------------------------------------------------------------------------
# Load shared library
# ---------------------------------------------------------------------------
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_BUILD_DIR = os.path.join(os.path.dirname(_SCRIPT_DIR), "build")
_LIB_PATH = os.path.join(_BUILD_DIR, "libquantum.so")
if not os.path.exists(_LIB_PATH):
    _LIB_PATH = os.path.join(_BUILD_DIR, "libquantum.dylib")
if not os.path.exists(_LIB_PATH):
    sys.exit(f"ERROR: shared library not found in {_BUILD_DIR}")

_lib = ctypes.CDLL(_LIB_PATH)

_lib.qc_circuit_create.restype = ctypes.c_void_p
_lib.qc_circuit_create.argtypes = [ctypes.c_uint32]
_lib.qc_circuit_destroy.restype = None
_lib.qc_circuit_destroy.argtypes = [ctypes.c_void_p]
_lib.qc_circuit_set_simulate.restype = ctypes.c_int
_lib.qc_circuit_set_simulate.argtypes = [ctypes.c_void_p, ctypes.c_int]
_lib.qc_circuit_to_qasm.restype = ctypes.c_char_p
_lib.qc_circuit_to_qasm.argtypes = [ctypes.c_void_p]
_lib.qc_circuit_x.restype = ctypes.c_int
_lib.qc_circuit_x.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
_lib.qc_qubit_alloc_n.restype = ctypes.c_int
_lib.qc_qubit_alloc_n.argtypes = [ctypes.c_void_p, ctypes.c_uint32,
                                   ctypes.POINTER(ctypes.c_uint32)]
_lib.qc_qft.restype = ctypes.c_int
_lib.qc_qft.argtypes = [ctypes.c_void_p,
                         ctypes.POINTER(ctypes.c_uint32),
                         ctypes.c_uint32]
_lib.qc_iqft.restype = ctypes.c_int
_lib.qc_iqft.argtypes = [ctypes.c_void_p,
                          ctypes.POINTER(ctypes.c_uint32),
                          ctypes.c_uint32]

_sim = AerSimulator()


def _add_measurements(qasm_str, qubit_indices):
    lines = qasm_str.strip().split("\n")
    out = []
    n_meas = len(qubit_indices)
    for line in lines:
        out.append(line)
        if line.startswith("qubit["):
            out.append(f"bit[{n_meas}] c;")
    for ci, qi in enumerate(qubit_indices):
        out.append(f"c[{ci}] = measure q[{qi}];")
    return "\n".join(out) + "\n"


def _simulate(qasm_str, qubit_indices, width):
    qasm_meas = _add_measurements(qasm_str, qubit_indices)
    circuit = qasm3_loads(qasm_meas)
    tc = transpile(circuit, _sim)
    result = _sim.run(tc, shots=1).result()
    counts = result.get_counts()
    assert len(counts) == 1, f"non-deterministic: {counts}"
    bitstr = list(counts.keys())[0]
    bits = bitstr[::-1]
    val = 0
    for i in range(width):
        val += int(bits[i]) << i
    return val


def _make_ctx(initial=64):
    ctx = _lib.qc_circuit_create(initial)
    assert ctx is not None
    _lib.qc_circuit_set_simulate(ctx, 1)
    return ctx


def _get_qasm(ctx):
    qasm = _lib.qc_circuit_to_qasm(ctx)
    assert qasm is not None
    s = qasm.decode()
    _lib.qc_circuit_destroy(ctx)
    return s


class TestQftIqftRoundTrip(unittest.TestCase):
    """qc_qft followed by qc_iqft on |x> must recover |x>."""

    def _round_trip(self, n, x, qmap_layout):
        """qmap_layout: list of n hardware qubit indices to use."""
        ctx = _make_ctx()
        # Allocate enough qubits to cover the largest qmap index.
        max_q = max(qmap_layout) + 1
        start = ctypes.c_uint32(0)
        _lib.qc_qubit_alloc_n(ctx, max_q, ctypes.byref(start))
        # Prepare |x> on the qmap qubits (LSB first).
        for i in range(n):
            if (x >> i) & 1:
                _lib.qc_circuit_x(ctx, qmap_layout[i])
        qmap = (ctypes.c_uint32 * n)(*qmap_layout)
        self.assertEqual(_lib.qc_qft(ctx, qmap, n), 0)
        self.assertEqual(_lib.qc_iqft(ctx, qmap, n), 0)
        qasm = _get_qasm(ctx)
        result = _simulate(qasm, qmap_layout, n)
        self.assertEqual(
            result, x,
            f"round-trip n={n} qmap={qmap_layout} x={x} -> {result}")

    def test_contiguous_n3(self):
        for x in range(8):
            with self.subTest(x=x):
                self._round_trip(3, x, [0, 1, 2])

    def test_contiguous_n4(self):
        for x in [0, 1, 5, 9, 15]:
            with self.subTest(x=x):
                self._round_trip(4, x, [0, 1, 2, 3])

    def test_noncontiguous_n3(self):
        # Stride-2 layout: virtual {0,1,2} -> hardware {0,2,4}
        for x in range(8):
            with self.subTest(x=x):
                self._round_trip(3, x, [0, 2, 4])

    def test_noncontiguous_n4(self):
        # Permuted, non-contiguous layout
        for x in [0, 3, 7, 12]:
            with self.subTest(x=x):
                self._round_trip(4, x, [3, 7, 1, 5])


if __name__ == "__main__":
    unittest.main()
