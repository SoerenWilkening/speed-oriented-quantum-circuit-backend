"""Regression tests for BUG-02: qarray *= scalar segfault.

Root cause: MAXLAYERINSEQUENCE buffer overflow in C backend (fixed by BUG-01).
Defensive fix: __imul__ now marks swapped-out result_qint as uncomputed to
prevent GC from reversing multiplication gates.

Tests verify:
- qarray *= scalar at widths 1-8 (single and multi-element)
- qint.__imul__ standalone at widths 1-8
- Simulation correctness where qubit count allows (<=17 qubits)
"""

import gc
import re
import warnings

import pytest
import qiskit.qasm3
from qiskit_aer import AerSimulator

import quantum_language as ql

warnings.filterwarnings("ignore", message="Value .* exceeds")


def _simulate_and_extract(qasm_str, num_qubits, result_start, result_width):
    """Simulate QASM and extract integer from result register."""
    circuit = qiskit.qasm3.loads(qasm_str)
    if not circuit.cregs:
        circuit.measure_all()

    simulator = AerSimulator(method="statevector", max_parallel_threads=4)
    job = simulator.run(circuit, shots=1)
    result = job.result()
    counts = result.get_counts()
    bitstring = list(counts.keys())[0]

    msb_pos = num_qubits - result_start - result_width
    lsb_pos = num_qubits - 1 - result_start
    result_bits = bitstring[msb_pos : lsb_pos + 1]
    return int(result_bits, 2)


def _get_num_qubits(qasm_str):
    """Extract number of qubits from OpenQASM string."""
    matches = re.findall(r"qubit\[(\d+)\]", qasm_str)
    if matches:
        return max(int(m) for m in matches)
    return 0


class TestBug02_QarrayImul:
    """Regression tests for BUG-02: qarray *= scalar segfault."""

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_qarray_imul_single_element(self, width):
        """qarray *= scalar with single element at various widths."""
        gc.collect()
        ql.circuit()
        arr = ql.qarray([1], width=width)
        arr *= 2
        gc.collect()  # Force GC to test defensive fix
        qasm = ql.to_openqasm()
        assert len(qasm) > 0, f"Width {width} should produce non-empty circuit"

    @pytest.mark.parametrize("width", [1, 2, 3, 4])
    def test_qarray_imul_multi_element(self, width):
        """qarray *= scalar with multiple elements."""
        gc.collect()
        ql.circuit()
        arr = ql.qarray([1, 2, 3], width=width)
        arr *= 2
        gc.collect()
        qasm = ql.to_openqasm()
        assert len(qasm) > 0, f"Width {width} should produce non-empty circuit"

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_qint_imul_standalone(self, width):
        """qint.__imul__ works outside qarray context."""
        gc.collect()
        ql.circuit()
        a = ql.qint(1, width=width)
        a *= 2
        gc.collect()
        qasm = ql.to_openqasm()
        assert len(qasm) > 0, f"Width {width} should produce non-empty circuit"

    def test_qarray_imul_simulation_width4(self):
        """qarray *= 2 produces correct result via simulation.

        Width 4, single element: 4*3=12 qubits (within 17-qubit limit).
        Tests that [3] *= 2 produces [6].
        """
        gc.collect()
        ql.circuit()
        arr = ql.qarray([3], width=4)
        arr *= 2
        gc.collect()

        elem = arr[0]
        result_start = elem.allocated_start
        result_width = elem.width
        qasm = ql.to_openqasm()
        num_qubits = _get_num_qubits(qasm)

        result = _simulate_and_extract(qasm, num_qubits, result_start, result_width)
        assert result == 6, f"Expected 6, got {result}"

    def test_qint_imul_simulation_width4(self):
        """qint *= 2 produces correct result via simulation.

        Width 4: 4*3=12 qubits. Tests 3 *= 2 = 6.
        """
        gc.collect()
        ql.circuit()
        a = ql.qint(3, width=4)
        a *= 2
        gc.collect()

        result_start = a.allocated_start
        result_width = a.width
        qasm = ql.to_openqasm()
        num_qubits = _get_num_qubits(qasm)

        result = _simulate_and_extract(qasm, num_qubits, result_start, result_width)
        assert result == 6, f"Expected 3*2=6, got {result}"

    def test_qint_imul_simulation_width3(self):
        """qint *= 3 at width 3 via simulation.

        Width 3: 3*3=9 qubits. Tests 2 *= 3 = 6.
        """
        gc.collect()
        ql.circuit()
        a = ql.qint(2, width=3)
        a *= 3
        gc.collect()

        result_start = a.allocated_start
        result_width = a.width
        qasm = ql.to_openqasm()
        num_qubits = _get_num_qubits(qasm)

        result = _simulate_and_extract(qasm, num_qubits, result_start, result_width)
        assert result == 6, f"Expected 2*3=6, got {result}"

    def test_qarray_mul_outofplace(self):
        """qarray * scalar (out-of-place) works."""
        gc.collect()
        ql.circuit()
        arr = ql.qarray([1, 2], width=4)
        result = arr * 2
        assert isinstance(result, ql.qarray)
        assert result.shape == (2,)

    def test_qarray_mul_array(self):
        """qarray * qarray (element-wise) works."""
        gc.collect()
        ql.circuit()
        a = ql.qarray([1, 2, 3], width=4)
        b = ql.qarray([2, 3, 4], width=4)
        c = a * b
        assert isinstance(c, ql.qarray)
        assert c.shape == (3,)
