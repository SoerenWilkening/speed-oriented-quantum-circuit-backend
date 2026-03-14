"""Tests for Cython-level multiplication dispatch (qint_arithmetic.pxi).

Replaces the deleted C-level test_hot_path_mul.c tests. Exercises each
major dispatch path:
- Uncontrolled QQ and CQ multiplication (Toffoli)
- Controlled QQ and CQ multiplication (Toffoli)
- 1-bit edge case
- QFT mode (fault_tolerant=False) as baseline comparison
- Zero-value and boundary-value edge cases
- QQ path forced via force_qq parameter to ensure QQ dispatch is tested
"""

import warnings

import pytest
import qiskit.qasm3
from qiskit_aer import AerSimulator

import quantum_language as ql

warnings.filterwarnings("ignore", message="Value .* exceeds")

MAX_SIM_QUBITS = 17


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _get_num_qubits(qasm_str):
    for line in qasm_str.split("\n"):
        line = line.strip()
        if line.startswith("qubit["):
            return int(line.split("[")[1].split("]")[0])
    raise ValueError("Could not find qubit count in QASM")


def _simulate(qasm_str, num_qubits, start, width):
    circuit = qiskit.qasm3.loads(qasm_str)
    if not circuit.cregs:
        circuit.measure_all()
    sim = AerSimulator(method="statevector", max_parallel_threads=4)
    job = sim.run(circuit, shots=1)
    counts = job.result().get_counts()
    bitstring = list(counts.keys())[0]
    msb = num_qubits - start - width
    lsb = num_qubits - 1 - start
    return int(bitstring[msb : lsb + 1], 2)


def _run_multiplication(a_val, b_val, width, fault_tolerant=True,
                        controlled=False, force_qq=False):
    """Build circuit, perform multiplication, simulate, return result.

    For QQ mul (c = a * b), the result register starts at 2*width.
    For CQ imul (a *= b), the result register starts at width (after swap).
    When force_qq=True, b_val is used to create a second qint.
    """
    ql.circuit()
    ql.option("fault_tolerant", fault_tolerant)

    a = ql.qint(a_val, width=width)

    if force_qq or not isinstance(b_val, int):
        # QQ path: out-of-place multiplication c = a * b
        b = ql.qint(int(b_val), width=width)
        if controlled:
            ctrl = ql.qbool(True)
            with ctrl:
                c = a * b
        else:
            c = a * b

        qasm_str = ql.to_openqasm()
        nq = _get_num_qubits(qasm_str)
        if nq > MAX_SIM_QUBITS:
            pytest.skip(f"Circuit needs {nq} qubits (max {MAX_SIM_QUBITS})")

        # Result register starts after a and b (and ctrl if controlled)
        if controlled:
            result_start = c.allocated_start
        else:
            result_start = 2 * width

        return _simulate(qasm_str, nq, result_start, width)
    else:
        # CQ path: in-place multiplication a *= b
        if controlled:
            ctrl = ql.qbool(True)
            with ctrl:
                a *= b_val
        else:
            a *= b_val

        qasm_str = ql.to_openqasm()
        nq = _get_num_qubits(qasm_str)
        if nq > MAX_SIM_QUBITS:
            pytest.skip(f"Circuit needs {nq} qubits (max {MAX_SIM_QUBITS})")

        # After imul, a points to result register
        result_start = a.allocated_start

        return _simulate(qasm_str, nq, result_start, width)


def _run_gate_check(a_val, b_val, width, fault_tolerant=True,
                    controlled=False, force_qq=False):
    """Build circuit, return gate count (non-zero means gates were emitted)."""
    ql.circuit()
    ql.option("fault_tolerant", fault_tolerant)

    a = ql.qint(a_val, width=width)

    if force_qq or not isinstance(b_val, int):
        b = ql.qint(int(b_val), width=width)
        if controlled:
            ctrl = ql.qbool(True)
            with ctrl:
                c = a * b
        else:
            c = a * b
    else:
        if controlled:
            ctrl = ql.qbool(True)
            with ctrl:
                a *= b_val
        else:
            a *= b_val

    return ql.get_gate_count()


# ===========================================================================
# Toffoli: Uncontrolled QQ multiplication
# ===========================================================================


class TestToffoliQQUncontrolled:
    """Uncontrolled quantum-quantum multiplication via Toffoli dispatch."""

    @pytest.mark.parametrize("width", [2, 3])
    def test_qq_mul(self, width):
        mask = (1 << width) - 1
        result = _run_multiplication(2, 3, width, fault_tolerant=True,
                                     force_qq=True)
        assert result == (2 * 3) & mask

    def test_qq_mul_produces_gates(self):
        gc = _run_gate_check(3, 2, 4, fault_tolerant=True, force_qq=True)
        assert gc > 0, "QQ Toffoli mul should produce gates"

    @pytest.mark.parametrize("width", [2, 3])
    def test_qq_mul_exhaustive(self, width):
        """Exhaustive correctness for small widths."""
        mask = (1 << width) - 1
        failures = []
        for a in range(1 << width):
            for b in range(1 << width):
                result = _run_multiplication(a, b, width, fault_tolerant=True,
                                             force_qq=True)
                expected = (a * b) & mask
                if result != expected:
                    failures.append(f"  {a}*{b}@{width}b: got {result}, want {expected}")
        assert not failures, f"{len(failures)} failures:\n" + "\n".join(failures[:10])


# ===========================================================================
# Toffoli: Uncontrolled CQ multiplication
# ===========================================================================


class TestToffoliCQUncontrolled:
    """Uncontrolled classical-quantum multiplication via Toffoli dispatch."""

    @pytest.mark.parametrize("width", [2, 3])
    def test_cq_mul(self, width):
        mask = (1 << width) - 1
        result = _run_multiplication(3, 2, width, fault_tolerant=True)
        assert result == (3 * 2) & mask

    def test_cq_mul_produces_gates(self):
        gc = _run_gate_check(3, 5, 4, fault_tolerant=True)
        assert gc > 0, "CQ Toffoli mul should produce gates"

    def test_cq_mul_zero_value(self):
        """CQ mul with classical value 0 should produce 0."""
        result = _run_multiplication(5, 0, 4, fault_tolerant=True)
        assert result == 0

    @pytest.mark.parametrize("width", [2, 3])
    def test_cq_mul_exhaustive(self, width):
        """Exhaustive correctness for small widths."""
        mask = (1 << width) - 1
        failures = []
        for a in range(1 << width):
            for b in range(1 << width):
                result = _run_multiplication(a, b, width, fault_tolerant=True)
                expected = (a * b) & mask
                if result != expected:
                    failures.append(f"  {a}*{b}@{width}b: got {result}, want {expected}")
        assert not failures, f"{len(failures)} failures:\n" + "\n".join(failures[:10])


# ===========================================================================
# Toffoli: Controlled QQ and CQ
# ===========================================================================


class TestToffoliControlled:
    """Controlled multiplication via Toffoli dispatch (cQQ and cCQ paths)."""

    def test_controlled_qq_mul(self):
        result = _run_multiplication(2, 3, 4, controlled=True,
                                     fault_tolerant=True, force_qq=True)
        assert result == 6, "Controlled QQ mul with ctrl=True should multiply"

    def test_controlled_cq_mul(self):
        result = _run_multiplication(4, 3, 4, controlled=True,
                                     fault_tolerant=True)
        assert result == 12, "Controlled CQ mul with ctrl=True should multiply"

    def test_controlled_qq_produces_gates(self):
        gc = _run_gate_check(2, 3, 4, controlled=True, fault_tolerant=True,
                             force_qq=True)
        assert gc > 0, "Controlled QQ Toffoli mul should produce gates"

    def test_controlled_cq_produces_gates(self):
        gc = _run_gate_check(2, 5, 4, controlled=True, fault_tolerant=True)
        assert gc > 0, "Controlled CQ Toffoli mul should produce gates"


# ===========================================================================
# 1-bit edge case
# ===========================================================================


class TestOneBitEdgeCase:
    """1-bit multiplication uses direct gate emission (special case)."""

    def test_1bit_qq_mul(self):
        result = _run_multiplication(1, 1, 1, fault_tolerant=True,
                                     force_qq=True)
        assert result == 1, "1 * 1 = 1"

    def test_1bit_cq_mul(self):
        result = _run_multiplication(1, 1, 1, fault_tolerant=True)
        assert result == 1, "1 * 1 = 1"

    def test_1bit_qq_zero(self):
        result = _run_multiplication(1, 0, 1, fault_tolerant=True,
                                     force_qq=True)
        assert result == 0, "1 * 0 = 0"

    def test_1bit_cq_zero(self):
        result = _run_multiplication(1, 0, 1, fault_tolerant=True)
        assert result == 0, "1 * 0 = 0"

    def test_1bit_qq_produces_gates(self):
        gc = _run_gate_check(1, 1, 1, fault_tolerant=True, force_qq=True)
        assert gc > 0, "1-bit QQ Toffoli mul should produce gates"

    def test_1bit_cq_produces_gates(self):
        gc = _run_gate_check(1, 1, 1, fault_tolerant=True)
        assert gc > 0, "1-bit CQ Toffoli mul should produce gates"


# ===========================================================================
# QFT baseline (non-Toffoli)
# ===========================================================================


class TestQFTBaseline:
    """QFT-mode multiplication (fault_tolerant=False) for comparison."""

    def test_qft_qq_mul(self):
        result = _run_multiplication(3, 2, 4, fault_tolerant=False,
                                     force_qq=True)
        assert result == 6

    def test_qft_cq_mul(self):
        result = _run_multiplication(3, 4, 4, fault_tolerant=False)
        assert result == 12

    def test_qft_matches_toffoli(self):
        """QFT and Toffoli modes should produce identical results for QQ."""
        r_qft = _run_multiplication(5, 3, 4, fault_tolerant=False,
                                    force_qq=True)
        r_tof = _run_multiplication(5, 3, 4, fault_tolerant=True,
                                    force_qq=True)
        assert r_qft == r_tof


# ===========================================================================
# Zero and boundary values
# ===========================================================================


class TestEdgeCases:
    """Zero-value and boundary edge cases."""

    def test_mul_zero_qq(self):
        result = _run_multiplication(5, 0, 4, fault_tolerant=True,
                                     force_qq=True)
        assert result == 0

    def test_mul_zero_cq(self):
        result = _run_multiplication(5, 0, 4, fault_tolerant=True)
        assert result == 0

    def test_mul_one_qq(self):
        result = _run_multiplication(7, 1, 4, fault_tolerant=True,
                                     force_qq=True)
        assert result == 7

    def test_mul_one_cq(self):
        result = _run_multiplication(7, 1, 4, fault_tolerant=True)
        assert result == 7

    def test_overflow_wraps(self):
        """Multiplication that overflows should wrap (modular arithmetic)."""
        result = _run_multiplication(5, 4, 4, fault_tolerant=True,
                                     force_qq=True)
        assert result == (5 * 4) % 16, "5 * 4 mod 16 = 4"

    def test_max_value_mul(self):
        result = _run_multiplication(15, 15, 4, fault_tolerant=True,
                                     force_qq=True)
        assert result == (15 * 15) % 16
