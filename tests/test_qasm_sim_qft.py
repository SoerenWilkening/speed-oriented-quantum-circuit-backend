#!/usr/bin/env python3
"""
QASM simulation tests for QFT-mode arithmetic circuits.

Phase 8, Module 8.6 -- refactor-5ec

Generates arithmetic circuits via the C API (QFT mode), exports to OpenQASM
via qc_circuit_to_qasm(), simulates with qiskit-aer, and verifies that
computational results match expected classical values.

Covers: CQ addition, QQ addition, CQ subtraction (negative add), CQ
multiplication, QQ multiplication, for widths 1-8.

Max 17 qubits per simulation.
"""

import ctypes
import os
import sys
import unittest

# ---------------------------------------------------------------------------
# Qiskit imports
# ---------------------------------------------------------------------------
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
    sys.exit(f"ERROR: shared library not found at {_LIB_PATH}\n"
             f"Build with: cd circuit-c-backend && cmake -B build && cmake --build build")

_lib = ctypes.CDLL(_LIB_PATH)

# ---------------------------------------------------------------------------
# ctypes bindings
# ---------------------------------------------------------------------------
_lib.qc_circuit_create.restype = ctypes.c_void_p
_lib.qc_circuit_create.argtypes = [ctypes.c_uint32]

_lib.qc_circuit_destroy.restype = None
_lib.qc_circuit_destroy.argtypes = [ctypes.c_void_p]

_lib.qc_circuit_set_simulate.restype = ctypes.c_int
_lib.qc_circuit_set_simulate.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.qc_circuit_set_arith_mode.restype = ctypes.c_int
_lib.qc_circuit_set_arith_mode.argtypes = [ctypes.c_void_p, ctypes.c_int]

_lib.qc_circuit_to_qasm.restype = ctypes.c_char_p
_lib.qc_circuit_to_qasm.argtypes = [ctypes.c_void_p]

_lib.qc_circuit_x.restype = ctypes.c_int
_lib.qc_circuit_x.argtypes = [ctypes.c_void_p, ctypes.c_uint32]

_lib.qc_qubit_alloc_n.restype = ctypes.c_int
_lib.qc_qubit_alloc_n.argtypes = [ctypes.c_void_p, ctypes.c_uint32,
                                   ctypes.POINTER(ctypes.c_uint32)]

_lib.qc_qubit_alloc.restype = ctypes.c_int
_lib.qc_qubit_alloc.argtypes = [ctypes.c_void_p,
                                 ctypes.POINTER(ctypes.c_uint32)]

_lib.qc_arith_cq_add.restype = ctypes.c_int
_lib.qc_arith_cq_add.argtypes = [ctypes.c_void_p,
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.c_uint32, ctypes.c_int64]

_lib.qc_arith_qq_add.restype = ctypes.c_int
_lib.qc_arith_qq_add.argtypes = [ctypes.c_void_p,
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.c_uint32]

_lib.qc_arith_cq_mul.restype = ctypes.c_int
_lib.qc_arith_cq_mul.argtypes = [ctypes.c_void_p,
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.c_uint32, ctypes.c_int64]

_lib.qc_arith_qq_mul.restype = ctypes.c_int
_lib.qc_arith_qq_mul.argtypes = [ctypes.c_void_p,
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.c_uint32]

QC_ARITH_QFT = 0

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
_sim = AerSimulator()


def _alloc_reg(ctx, width):
    """Allocate a contiguous qubit register, return c_uint32 array."""
    start = ctypes.c_uint32(0)
    err = _lib.qc_qubit_alloc_n(ctx, width, ctypes.byref(start))
    assert err == 0, f"qc_qubit_alloc_n failed: {err}"
    arr = (ctypes.c_uint32 * width)(*[start.value + i for i in range(width)])
    return arr, start.value


def _init_value(ctx, reg, width, value):
    """Set register to |value> by applying X gates to the appropriate qubits."""
    for i in range(width):
        if (value >> i) & 1:
            _lib.qc_circuit_x(ctx, reg[i])


def _add_measurements(qasm_str, qubit_indices):
    """Append measurement gates for the given qubit indices to a QASM string."""
    lines = qasm_str.strip().split("\n")
    new_lines = []
    n_qubits = 0
    for line in lines:
        new_lines.append(line)
        if line.startswith("qubit["):
            n_qubits = int(line.split("[")[1].split("]")[0])
            n_meas = len(qubit_indices)
            new_lines.append(f"bit[{n_meas}] c;")
    for ci, qi in enumerate(qubit_indices):
        new_lines.append(f"c[{ci}] = measure q[{qi}];")
    return "\n".join(new_lines) + "\n"


def _simulate_and_readout(qasm_str, qubit_indices, width):
    """Simulate a QASM circuit and return the integer readout of the register.

    qubit_indices: list of qubit indices forming the register (LSB first).
    width: number of bits in the register.
    Returns the integer value measured (deterministic circuits give 1 result).
    """
    qasm_meas = _add_measurements(qasm_str, qubit_indices)
    circuit = qasm3_loads(qasm_meas)
    tc = transpile(circuit, _sim)
    result = _sim.run(tc, shots=1).result()
    counts = result.get_counts()
    assert len(counts) == 1, f"Expected deterministic result, got {counts}"
    bitstr = list(counts.keys())[0]
    # qiskit bitstring: MSB first, so bitstr[0] = highest-index classical bit
    # Our measurement maps c[i] = measure q[qubit_indices[i]]
    # bitstr reads as c[n-1] c[n-2] ... c[0]
    bits = bitstr[::-1]  # now bits[i] = c[i]
    val = 0
    for i in range(width):
        val += int(bits[i]) << i
    return val


def _make_ctx():
    """Create a fresh circuit context with simulation enabled, QFT mode."""
    ctx = _lib.qc_circuit_create(64)
    assert ctx is not None
    _lib.qc_circuit_set_simulate(ctx, 1)
    _lib.qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT)
    return ctx


def _get_qasm(ctx):
    """Export circuit to QASM string and destroy context."""
    qasm = _lib.qc_circuit_to_qasm(ctx)
    assert qasm is not None
    qasm_str = qasm.decode()
    _lib.qc_circuit_destroy(ctx)
    return qasm_str


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
class TestQFTCQAddition(unittest.TestCase):
    """CQ addition: target += classical_value (QFT Draper adder)."""

    def _run_cq_add(self, width, init_val, add_val):
        mask = (1 << width) - 1
        expected = (init_val + add_val) & mask
        ctx = _make_ctx()
        reg, start = _alloc_reg(ctx, width)
        _init_value(ctx, reg, width, init_val)
        err = _lib.qc_arith_cq_add(ctx, reg, width, add_val)
        self.assertEqual(err, 0, f"qc_arith_cq_add error: {err}")
        qasm = _get_qasm(ctx)
        qubits = list(range(start, start + width))
        result = _simulate_and_readout(qasm, qubits, width)
        self.assertEqual(result, expected,
                         f"CQ add w={width}: {init_val}+{add_val}={result}, "
                         f"expected {expected}")

    def test_widths_1_to_8_zero_init(self):
        """0 + value for widths 1-8."""
        for w in range(1, 9):
            val = min((1 << w) - 1, 2 * w + 1)
            with self.subTest(width=w, value=val):
                self._run_cq_add(w, 0, val)

    def test_widths_1_to_8_nonzero_init(self):
        """init + value for widths 1-8."""
        for w in range(2, 9):
            mask = (1 << w) - 1
            init_val = 1
            add_val = min(mask, w)
            with self.subTest(width=w, init=init_val, add=add_val):
                self._run_cq_add(w, init_val, add_val)

    def test_overflow_wraps(self):
        """Addition that overflows wraps modulo 2^width."""
        for w in range(2, 7):
            mask = (1 << w) - 1
            with self.subTest(width=w):
                self._run_cq_add(w, mask, 1)

    def test_add_negative(self):
        """Subtraction via negative value: target + (-1) = target - 1."""
        for w in range(2, 7):
            init = 3 if w >= 2 else 1
            with self.subTest(width=w):
                self._run_cq_add(w, init, -1)


class TestQFTQQAddition(unittest.TestCase):
    """QQ addition: a += b (QFT Draper adder)."""

    def _run_qq_add(self, width, a_val, b_val):
        mask = (1 << width) - 1
        expected_a = (a_val + b_val) & mask
        ctx = _make_ctx()
        reg_a, start_a = _alloc_reg(ctx, width)
        reg_b, start_b = _alloc_reg(ctx, width)
        _init_value(ctx, reg_a, width, a_val)
        _init_value(ctx, reg_b, width, b_val)
        err = _lib.qc_arith_qq_add(ctx, reg_a, reg_b, width)
        self.assertEqual(err, 0)
        qasm = _get_qasm(ctx)
        # Read both registers
        qubits_a = list(range(start_a, start_a + width))
        qubits_b = list(range(start_b, start_b + width))
        result_a = _simulate_and_readout(qasm, qubits_a + qubits_b,
                                         2 * width)
        # Extract a and b from the combined readout
        ra = result_a & mask
        rb = (result_a >> width) & mask
        self.assertEqual(ra, expected_a,
                         f"QQ add w={width}: a={a_val}+{b_val}={ra}, "
                         f"expected {expected_a}")
        self.assertEqual(rb, b_val,
                         f"QQ add w={width}: b changed from {b_val} to {rb}")

    def test_widths_1_to_8(self):
        """QQ addition for widths 1-8."""
        for w in range(1, 9):
            # Stay within 2*w <= 16 qubits
            if 2 * w > 17:
                continue
            mask = (1 << w) - 1
            a = min(3, mask)
            b = min(2, mask)
            with self.subTest(width=w, a=a, b=b):
                self._run_qq_add(w, a, b)

    def test_overflow(self):
        """QQ addition overflow wraps."""
        for w in range(2, 7):
            mask = (1 << w) - 1
            with self.subTest(width=w):
                self._run_qq_add(w, mask, 1)


class TestQFTCQMultiplication(unittest.TestCase):
    """CQ multiplication: result = target * value (QFT)."""

    def _run_cq_mul(self, width, target_val, mul_val):
        mask = (1 << width) - 1
        expected = (target_val * mul_val) & mask
        ctx = _make_ctx()
        reg_result, start_result = _alloc_reg(ctx, width)
        reg_target, start_target = _alloc_reg(ctx, width)
        _init_value(ctx, reg_target, width, target_val)
        err = _lib.qc_arith_cq_mul(ctx, reg_result, reg_target, width,
                                     mul_val)
        self.assertEqual(err, 0)
        qasm = _get_qasm(ctx)
        # Read the result register
        qubits = list(range(start_result, start_result + width))
        result = _simulate_and_readout(qasm, qubits, width)
        self.assertEqual(result, expected,
                         f"CQ mul w={width}: {target_val}*{mul_val}={result}, "
                         f"expected {expected}")

    def test_widths_1_to_6(self):
        """CQ multiplication for widths 1-6 (2*w qubits + ancillae)."""
        for w in range(1, 7):
            mask = (1 << w) - 1
            t = min(2, mask)
            m = min(3, mask)
            with self.subTest(width=w, target=t, multiplier=m):
                self._run_cq_mul(w, t, m)

    def test_multiply_by_zero(self):
        """target * 0 = 0."""
        for w in range(1, 5):
            with self.subTest(width=w):
                self._run_cq_mul(w, 3 if w >= 2 else 1, 0)

    def test_multiply_by_one(self):
        """target * 1 = target."""
        for w in range(1, 5):
            mask = (1 << w) - 1
            val = min(5, mask)
            with self.subTest(width=w):
                self._run_cq_mul(w, val, 1)


class TestQFTQQMultiplication(unittest.TestCase):
    """QQ multiplication: result = a * b (QFT). Limited widths due to qubits."""

    def _run_qq_mul(self, width, a_val, b_val):
        mask = (1 << width) - 1
        expected = (a_val * b_val) & mask
        ctx = _make_ctx()
        reg_result, start_result = _alloc_reg(ctx, width)
        reg_a, start_a = _alloc_reg(ctx, width)
        reg_b, start_b = _alloc_reg(ctx, width)
        _init_value(ctx, reg_a, width, a_val)
        _init_value(ctx, reg_b, width, b_val)
        err = _lib.qc_arith_qq_mul(ctx, reg_result, reg_a, reg_b, width)
        self.assertEqual(err, 0)
        qasm = _get_qasm(ctx)
        qubits = list(range(start_result, start_result + width))
        result = _simulate_and_readout(qasm, qubits, width)
        self.assertEqual(result, expected,
                         f"QQ mul w={width}: {a_val}*{b_val}={result}, "
                         f"expected {expected}")

    def test_widths_1_to_4(self):
        """QQ multiplication for widths 1-4 (3*w qubits + ancillae)."""
        for w in range(1, 5):
            mask = (1 << w) - 1
            a = min(2, mask)
            b = min(3, mask)
            with self.subTest(width=w, a=a, b=b):
                self._run_qq_mul(w, a, b)


class TestQFTEdgeCases(unittest.TestCase):
    """Edge cases for QFT-mode arithmetic."""

    def test_add_zero(self):
        """Adding 0 leaves register unchanged."""
        for w in range(1, 7):
            mask = (1 << w) - 1
            init = min(5, mask)
            ctx = _make_ctx()
            reg, start = _alloc_reg(ctx, w)
            _init_value(ctx, reg, w, init)
            _lib.qc_arith_cq_add(ctx, reg, w, 0)
            qasm = _get_qasm(ctx)
            qubits = list(range(start, start + w))
            result = _simulate_and_readout(qasm, qubits, w)
            self.assertEqual(result, init,
                             f"add 0: w={w}, got {result}, expected {init}")

    def test_identity_chain(self):
        """Adding +v then -v returns to original value."""
        for w in range(2, 7):
            mask = (1 << w) - 1
            init = min(3, mask)
            val = min(5, mask)
            ctx = _make_ctx()
            reg, start = _alloc_reg(ctx, w)
            _init_value(ctx, reg, w, init)
            _lib.qc_arith_cq_add(ctx, reg, w, val)
            _lib.qc_arith_cq_add(ctx, reg, w, -val)
            qasm = _get_qasm(ctx)
            qubits = list(range(start, start + w))
            result = _simulate_and_readout(qasm, qubits, w)
            self.assertEqual(result, init,
                             f"+v-v: w={w}, got {result}, expected {init}")


if __name__ == "__main__":
    unittest.main()
