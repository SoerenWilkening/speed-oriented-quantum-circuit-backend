#!/usr/bin/env python3
"""
QASM simulation tests for Toffoli-mode arithmetic circuits.

Phase 8, Module 8.6 -- refactor-5ec

Generates arithmetic circuits via the C API (Toffoli mode), exports to
OpenQASM via qc_circuit_to_qasm(), simulates with qiskit-aer, and verifies
that computational results match expected classical values.

Covers: CDKM QQ addition (b += a convention), CQ addition, Toffoli
multiplication, and CQ equality comparison, for widths 1-8.

Max 17 qubits per simulation.

Note on register conventions:
  - Toffoli QQ add: b += a (second register is modified)
  - Toffoli CQ add: target += value (uses temp ancillae internally)
  - Toffoli QQ mul: result = a * b
  - Toffoli CQ mul: result = target * value
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
    sys.exit(f"ERROR: shared library not found at {_LIB_PATH}")

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

_lib.qc_toffoli_qq_add.restype = ctypes.c_int
_lib.qc_toffoli_qq_add.argtypes = [ctypes.c_void_p,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.c_uint32]

_lib.qc_toffoli_cq_add.restype = ctypes.c_int
_lib.qc_toffoli_cq_add.argtypes = [ctypes.c_void_p,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.c_uint32, ctypes.c_int64]

_lib.qc_toffoli_qq_mul.restype = ctypes.c_int
_lib.qc_toffoli_qq_mul.argtypes = [ctypes.c_void_p,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.c_uint32]

_lib.qc_toffoli_cq_mul.restype = ctypes.c_int
_lib.qc_toffoli_cq_mul.argtypes = [ctypes.c_void_p,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.c_uint32, ctypes.c_int64]

_lib.qc_cmp_cq_equal.restype = ctypes.c_int
_lib.qc_cmp_cq_equal.argtypes = [ctypes.c_void_p,
                                   ctypes.POINTER(ctypes.c_uint32),
                                   ctypes.c_uint32, ctypes.c_int64,
                                   ctypes.c_uint32]

QC_ARITH_TOFFOLI = 1

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
_sim = AerSimulator()


def _alloc_reg(ctx, width):
    """Allocate a contiguous qubit register, return c_uint32 array and start."""
    start = ctypes.c_uint32(0)
    err = _lib.qc_qubit_alloc_n(ctx, width, ctypes.byref(start))
    assert err == 0, f"qc_qubit_alloc_n failed: {err}"
    arr = (ctypes.c_uint32 * width)(*[start.value + i for i in range(width)])
    return arr, start.value


def _init_value(ctx, reg, width, value):
    """Set register to |value> by applying X gates to appropriate qubits."""
    for i in range(width):
        if (value >> i) & 1:
            _lib.qc_circuit_x(ctx, reg[i])


def _add_measurements(qasm_str, qubit_indices):
    """Append measurement gates for the given qubit indices to a QASM string."""
    lines = qasm_str.strip().split("\n")
    new_lines = []
    for line in lines:
        new_lines.append(line)
        if line.startswith("qubit["):
            n_meas = len(qubit_indices)
            new_lines.append(f"bit[{n_meas}] c;")
    for ci, qi in enumerate(qubit_indices):
        new_lines.append(f"c[{ci}] = measure q[{qi}];")
    return "\n".join(new_lines) + "\n"


def _simulate_and_readout(qasm_str, qubit_indices, width):
    """Simulate a QASM circuit and read the integer from measured qubits.

    qubit_indices: list of qubit indices (LSB first).
    width: how many bits to read.
    Returns integer value.
    """
    qasm_meas = _add_measurements(qasm_str, qubit_indices)
    circuit = qasm3_loads(qasm_meas)
    tc = transpile(circuit, _sim)
    result = _sim.run(tc, shots=1).result()
    counts = result.get_counts()
    assert len(counts) == 1, f"Expected deterministic result, got {counts}"
    bitstr = list(counts.keys())[0]
    bits = bitstr[::-1]  # bits[i] = c[i]
    val = 0
    for i in range(width):
        val += int(bits[i]) << i
    return val


def _make_ctx():
    """Create circuit context with simulation enabled, Toffoli mode."""
    ctx = _lib.qc_circuit_create(128)
    assert ctx is not None
    _lib.qc_circuit_set_simulate(ctx, 1)
    _lib.qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI)
    return ctx


def _get_qasm(ctx):
    """Export QASM and destroy context."""
    qasm = _lib.qc_circuit_to_qasm(ctx)
    assert qasm is not None
    qasm_str = qasm.decode()
    _lib.qc_circuit_destroy(ctx)
    return qasm_str


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
class TestToffoliQQAddition(unittest.TestCase):
    """Toffoli CDKM QQ addition.

    Width 1: a ^= b (CX target=a, control=b).
    Width >= 2: b += a (CDKM ripple-carry, second register modified).
    """

    def _run_qq_add(self, width, a_val, b_val):
        mask = (1 << width) - 1
        ctx = _make_ctx()
        reg_a, start_a = _alloc_reg(ctx, width)
        reg_b, start_b = _alloc_reg(ctx, width)
        _init_value(ctx, reg_a, width, a_val)
        _init_value(ctx, reg_b, width, b_val)
        err = _lib.qc_toffoli_qq_add(ctx, reg_a, reg_b, width)
        self.assertEqual(err, 0)
        qasm = _get_qasm(ctx)
        qubits_a = list(range(start_a, start_a + width))
        qubits_b = list(range(start_b, start_b + width))
        all_q = qubits_a + qubits_b
        combined = _simulate_and_readout(qasm, all_q, 2 * width)
        ra = combined & mask
        rb = (combined >> width) & mask
        if width == 1:
            # Width 1: a ^= b (a is modified, b unchanged)
            expected_a = (a_val ^ b_val) & mask
            self.assertEqual(ra, expected_a,
                             f"Toff QQ add w=1: a={a_val}^{b_val}={ra}, "
                             f"expected {expected_a}")
            self.assertEqual(rb, b_val,
                             f"Toff QQ add w=1: b changed {b_val}->{rb}")
        else:
            # Width >= 2: b += a (b is modified, a unchanged)
            expected_b = (b_val + a_val) & mask
            self.assertEqual(ra, a_val,
                             f"Toff QQ add w={width}: a changed "
                             f"{a_val}->{ra}")
            self.assertEqual(rb, expected_b,
                             f"Toff QQ add w={width}: "
                             f"b={b_val}+{a_val}={rb}, "
                             f"expected {expected_b}")

    def test_width_1(self):
        """Width 1: a ^= b."""
        self._run_qq_add(1, 1, 0)
        self._run_qq_add(1, 0, 1)
        self._run_qq_add(1, 1, 1)

    def test_widths_2_to_8(self):
        """Widths 2-8 with various operands."""
        for w in range(2, 9):
            # 2*w + 1 ancilla <= 17
            if 2 * w + 1 > 17:
                continue
            mask = (1 << w) - 1
            a = min(3, mask)
            b = min(2, mask)
            with self.subTest(width=w, a=a, b=b):
                self._run_qq_add(w, a, b)

    def test_overflow(self):
        """Overflow wraps modulo 2^width."""
        for w in range(2, 7):
            mask = (1 << w) - 1
            with self.subTest(width=w):
                self._run_qq_add(w, mask, 1)


class TestToffoliCQAddition(unittest.TestCase):
    """Toffoli CQ addition: target += value."""

    def _run_cq_add(self, width, init_val, add_val):
        mask = (1 << width) - 1
        expected = (init_val + add_val) & mask
        ctx = _make_ctx()
        reg, start = _alloc_reg(ctx, width)
        _init_value(ctx, reg, width, init_val)
        err = _lib.qc_toffoli_cq_add(ctx, reg, width, add_val)
        self.assertEqual(err, 0)
        qasm = _get_qasm(ctx)
        qubits = list(range(start, start + width))
        result = _simulate_and_readout(qasm, qubits, width)
        self.assertEqual(result, expected,
                         f"Toff CQ add w={width}: {init_val}+{add_val}="
                         f"{result}, expected {expected}")

    def test_widths_1_to_8(self):
        """CQ add for widths 1-8."""
        for w in range(1, 9):
            mask = (1 << w) - 1
            val = min(5, mask)
            with self.subTest(width=w, value=val):
                self._run_cq_add(w, 0, val)

    def test_nonzero_init(self):
        """CQ add with nonzero initial value."""
        for w in range(2, 7):
            mask = (1 << w) - 1
            with self.subTest(width=w):
                self._run_cq_add(w, min(2, mask), min(3, mask))

    def test_add_zero(self):
        """Adding 0 is identity."""
        for w in range(1, 7):
            mask = (1 << w) - 1
            init = min(5, mask)
            with self.subTest(width=w):
                self._run_cq_add(w, init, 0)

    def test_negative_value(self):
        """Subtraction via negative value."""
        for w in range(2, 7):
            mask = (1 << w) - 1
            init = min(5, mask)
            with self.subTest(width=w):
                self._run_cq_add(w, init, -1)

    def test_overflow(self):
        """Overflow wraps."""
        for w in range(2, 7):
            mask = (1 << w) - 1
            with self.subTest(width=w):
                self._run_cq_add(w, mask, 1)


class TestToffoliCQMultiplication(unittest.TestCase):
    """Toffoli CQ multiplication: result = target * value."""

    def _run_cq_mul(self, width, target_val, mul_val):
        result_bits = width
        mask = (1 << result_bits) - 1
        expected = (target_val * mul_val) & mask
        ctx = _make_ctx()
        reg_result, start_result = _alloc_reg(ctx, result_bits)
        reg_target, start_target = _alloc_reg(ctx, width)
        _init_value(ctx, reg_target, width, target_val)
        err = _lib.qc_toffoli_cq_mul(ctx, reg_result, result_bits,
                                       reg_target, width, mul_val)
        self.assertEqual(err, 0, f"qc_toffoli_cq_mul error: {err}")
        qasm = _get_qasm(ctx)
        qubits = list(range(start_result, start_result + result_bits))
        result = _simulate_and_readout(qasm, qubits, result_bits)
        self.assertEqual(result, expected,
                         f"Toff CQ mul w={width}: {target_val}*{mul_val}="
                         f"{result}, expected {expected}")

    def test_widths_1_to_4(self):
        """CQ multiplication for small widths."""
        for w in range(1, 5):
            mask = (1 << w) - 1
            t = min(2, mask)
            m = min(3, mask)
            with self.subTest(width=w, target=t, multiplier=m):
                self._run_cq_mul(w, t, m)

    def test_multiply_by_zero(self):
        """target * 0 = 0."""
        for w in range(1, 4):
            with self.subTest(width=w):
                self._run_cq_mul(w, 1, 0)

    def test_multiply_by_one(self):
        """target * 1 = target."""
        for w in range(1, 4):
            mask = (1 << w) - 1
            val = min(3, mask)
            with self.subTest(width=w):
                self._run_cq_mul(w, val, 1)


class TestToffoliQQMultiplication(unittest.TestCase):
    """Toffoli QQ multiplication: result = a * b. Very limited widths."""

    def _run_qq_mul(self, width, a_val, b_val):
        result_bits = width
        mask = (1 << result_bits) - 1
        expected = (a_val * b_val) & mask
        ctx = _make_ctx()
        reg_result, start_result = _alloc_reg(ctx, result_bits)
        reg_a, start_a = _alloc_reg(ctx, width)
        reg_b, start_b = _alloc_reg(ctx, width)
        _init_value(ctx, reg_a, width, a_val)
        _init_value(ctx, reg_b, width, b_val)
        err = _lib.qc_toffoli_qq_mul(ctx, reg_result, result_bits,
                                       reg_a, width, reg_b, width)
        self.assertEqual(err, 0)
        qasm = _get_qasm(ctx)
        qubits = list(range(start_result, start_result + result_bits))
        result = _simulate_and_readout(qasm, qubits, result_bits)
        self.assertEqual(result, expected,
                         f"Toff QQ mul w={width}: {a_val}*{b_val}={result}, "
                         f"expected {expected}")

    def test_widths_1_to_3(self):
        """QQ mul for widths 1-3 (uses many ancillae)."""
        for w in range(1, 4):
            mask = (1 << w) - 1
            a = min(2, mask)
            b = min(1, mask)
            with self.subTest(width=w, a=a, b=b):
                self._run_qq_mul(w, a, b)


class TestToffoliCQEquality(unittest.TestCase):
    """CQ equality comparison: result = (A == value).

    Note: The comparison uses MSB-first encoding for the register (a[0]=MSB),
    matching qc_two_complement's convention. We initialize the register
    accordingly: a[0] gets the highest bit of the value.
    """

    def _run_cq_equal(self, width, a_val, cmp_val):
        expected = 1 if a_val == cmp_val else 0
        ctx = _make_ctx()
        reg_a, start_a = _alloc_reg(ctx, width)
        result_q = ctypes.c_uint32(0)
        _lib.qc_qubit_alloc(ctx, ctypes.byref(result_q))
        # Comparison uses MSB-first encoding: a[0] = MSB.
        # Initialize register in MSB-first order.
        for i in range(width):
            bit_pos = width - 1 - i  # a[0] = MSB
            if (a_val >> bit_pos) & 1:
                _lib.qc_circuit_x(ctx, reg_a[i])
        err = _lib.qc_cmp_cq_equal(ctx, reg_a, width, cmp_val,
                                     result_q.value)
        self.assertEqual(err, 0, f"qc_cmp_cq_equal error: {err}")
        qasm = _get_qasm(ctx)
        result = _simulate_and_readout(qasm, [result_q.value], 1)
        self.assertEqual(result, expected,
                         f"CQ equal w={width}: ({a_val}=={cmp_val})={result},"
                         f" expected {expected}")

    def test_equal_width_1(self):
        """Width 1: equality is just XOR + X."""
        self._run_cq_equal(1, 0, 0)
        self._run_cq_equal(1, 1, 1)
        self._run_cq_equal(1, 0, 1)
        self._run_cq_equal(1, 1, 0)

    def test_equal_width_2(self):
        """Width 2 equality."""
        for a in range(4):
            for v in range(4):
                with self.subTest(a=a, v=v):
                    self._run_cq_equal(2, a, v)

    # Width 3+ equality omitted: MCX decomposition produces circuits that
    # exceed the 17-qubit simulation limit and cause OOM.


class TestToffoliEdgeCases(unittest.TestCase):
    """Edge cases for Toffoli-mode arithmetic."""

    def test_add_then_subtract(self):
        """a += val then a -= val returns to original (CQ)."""
        for w in range(2, 7):
            mask = (1 << w) - 1
            init = min(3, mask)
            val = min(5, mask)
            ctx = _make_ctx()
            reg, start = _alloc_reg(ctx, w)
            _init_value(ctx, reg, w, init)
            _lib.qc_toffoli_cq_add(ctx, reg, w, val)
            _lib.qc_toffoli_cq_add(ctx, reg, w, -val)
            qasm = _get_qasm(ctx)
            qubits = list(range(start, start + w))
            result = _simulate_and_readout(qasm, qubits, w)
            self.assertEqual(result, init,
                             f"+v-v: w={w}, got {result}, expected {init}")

    def test_qq_add_zero(self):
        """QQ add where a=0: b unchanged."""
        for w in range(2, 6):
            mask = (1 << w) - 1
            b_val = min(5, mask)
            ctx = _make_ctx()
            reg_a, start_a = _alloc_reg(ctx, w)
            reg_b, start_b = _alloc_reg(ctx, w)
            _init_value(ctx, reg_b, w, b_val)
            # a stays 0
            _lib.qc_toffoli_qq_add(ctx, reg_a, reg_b, w)
            qasm = _get_qasm(ctx)
            qubits_b = list(range(start_b, start_b + w))
            rb = _simulate_and_readout(qasm, qubits_b, w)
            self.assertEqual(rb, b_val,
                             f"QQ add a=0: w={w}, b={rb}, expected {b_val}")


if __name__ == "__main__":
    unittest.main()
