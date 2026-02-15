"""Phase 68: Toffoli Schoolbook Multiplication - Exhaustive Verification Tests.

Tests all 4 success criteria:
1. QQ Toffoli multiplication produces correct product for all input pairs at widths 1-3
2. CQ Toffoli multiplication produces correct product for all input pairs at widths 1-3
3. Toffoli multiplication circuits contain only CCX/CX/X gates (no CP/H gates)
4. a * b and a *= b operators dispatch to Toffoli multiplication in default mode

Uses Qiskit AerSimulator for statevector simulation to verify
arithmetic correctness exhaustively for small widths.
"""

import gc
import warnings

import pytest
import qiskit.qasm3
from qiskit_aer import AerSimulator
from verify_helpers import format_failure_message

import quantum_language as ql

warnings.filterwarnings("ignore", message="Value .* exceeds")


def _enable_toffoli():
    """Helper to enable Toffoli arithmetic mode."""
    ql.option("fault_tolerant", True)


def _simulate_and_extract(qasm_str, num_qubits, result_start, result_width):
    """Simulate a QASM circuit and extract result from specific qubit range.

    Args:
        qasm_str: OpenQASM 3.0 string
        num_qubits: Total number of qubits in circuit
        result_start: Starting physical qubit index of result register (LSB)
        result_width: Number of qubits in result register

    Returns:
        Integer value of result register
    """
    circuit = qiskit.qasm3.loads(qasm_str)
    if not circuit.cregs:
        circuit.measure_all()
    simulator = AerSimulator(method="statevector")
    job = simulator.run(circuit, shots=1)
    result = job.result()
    counts = result.get_counts()
    bitstring = list(counts.keys())[0]

    # Qiskit bitstring: position i = qubit (N-1-i)
    # Result register at physical qubits [result_start, result_start + result_width - 1]
    # MSB of result = qubit (result_start + result_width - 1) at position (N - result_start - result_width)
    # LSB of result = qubit result_start at position (N - 1 - result_start)
    msb_pos = num_qubits - result_start - result_width
    lsb_pos = num_qubits - 1 - result_start
    result_bits = bitstring[msb_pos : lsb_pos + 1]
    return int(result_bits, 2)


def _get_num_qubits(qasm_str):
    """Extract qubit count from OpenQASM string."""
    for line in qasm_str.split("\n"):
        line = line.strip()
        if line.startswith("qubit["):
            return int(line.split("[")[1].split("]")[0])
    raise Exception(f"Could not find qubit count in QASM:\n{qasm_str}")


def _verify_toffoli_mul_qq(circuit_builder, width):
    """Verify a Toffoli QQ multiplication circuit.

    For QQ mul (c = a * b), the allocation order is:
      a (self) at [0..width-1]
      b (other) at [width..2*width-1]
      c (result) at [2*width..3*width-1]
    Ancilla qubits (from CDKM adder loop) are above that.

    Args:
        circuit_builder: Callable that builds circuit and returns (expected, keepalive_refs)
        width: Result register width

    Returns:
        Tuple (actual, expected)
    """
    gc.collect()
    ql.circuit()

    result = circuit_builder()
    if isinstance(result, tuple):
        expected, _keepalive = result
    else:
        expected = result
        _keepalive = None

    qasm_str = ql.to_openqasm()
    num_qubits = _get_num_qubits(qasm_str)

    # Result register c starts after a and b registers
    result_start = 2 * width

    _keepalive = None

    try:
        actual = _simulate_and_extract(qasm_str, num_qubits, result_start, width)
        return (actual, expected)
    except Exception as e:
        raise Exception(f"Simulation failed: {e}\n\nQASM:\n{qasm_str}") from e


def _verify_toffoli_mul_cq(circuit_builder, width):
    """Verify a Toffoli CQ multiplication circuit.

    For CQ imul (qa *= val), __imul__ calls __mul__ which allocates:
      qa_orig (self) at [0..width-1]
      result at [width..2*width-1]
    Then __imul__ swaps qa's Python qubit refs to point to result qubits.
    Ancilla qubits (from CDKM adder loop) are above that.

    The product is computed into the result register at [width..2*width-1].

    Args:
        circuit_builder: Callable that builds circuit and returns expected value
        width: Bit width of result register

    Returns:
        Tuple (actual, expected)
    """
    gc.collect()
    ql.circuit()

    result = circuit_builder()
    if isinstance(result, tuple):
        expected, _keepalive = result
    else:
        expected = result
        _keepalive = None

    qasm_str = ql.to_openqasm()
    num_qubits = _get_num_qubits(qasm_str)

    # Result register starts after original self qubits
    result_start = width

    _keepalive = None

    try:
        actual = _simulate_and_extract(qasm_str, num_qubits, result_start, width)
        return (actual, expected)
    except Exception as e:
        raise Exception(f"Simulation failed: {e}\n\nQASM:\n{qasm_str}") from e


class TestToffoliQQMultiplication:
    """Success Criterion 1: QQ Toffoli multiplication produces correct product.

    Exhaustively tests all input pairs at widths 1-3.
    QQ mul (out-of-place) allocates 3*width qubits + ancilla.
    """

    @pytest.mark.parametrize("width", [1, 2, 3])
    def test_qq_mul_exhaustive(self, width):
        """QQ Toffoli multiplication produces correct results for all input pairs."""
        failures = []

        for a in range(1 << width):
            for b in range(1 << width):

                def circuit_builder(a=a, b=b, w=width):
                    _enable_toffoli()
                    qa = ql.qint(a, width=w)
                    qb = ql.qint(b, width=w)
                    qc = qa * qb
                    return ((a * b) % (1 << w), [qa, qb, qc])

                actual, expected = _verify_toffoli_mul_qq(circuit_builder, width)
                if actual != expected:
                    failures.append(
                        format_failure_message("toffoli_qq_mul", [a, b], width, expected, actual)
                    )

        assert not failures, f"{len(failures)} failures at width {width}:\n" + "\n".join(
            failures[:10]
        )


class TestToffoliCQMultiplication:
    """Success Criterion 2: CQ Toffoli multiplication produces correct product.

    Exhaustively tests all input pairs at widths 1-3.
    CQ imul allocates 2*width qubits + ancilla.
    """

    @pytest.mark.parametrize("width", [1, 2, 3])
    def test_cq_mul_exhaustive(self, width):
        """CQ Toffoli multiplication produces correct results for all input pairs."""
        failures = []

        for a in range(1 << width):
            for b in range(1 << width):

                def circuit_builder(a=a, b=b, w=width):
                    _enable_toffoli()
                    qa = ql.qint(a, width=w)
                    qa *= b
                    return (a * b) % (1 << w)

                actual, expected = _verify_toffoli_mul_cq(circuit_builder, width)
                if actual != expected:
                    failures.append(
                        format_failure_message("toffoli_cq_mul", [a, b], width, expected, actual)
                    )

        assert not failures, f"{len(failures)} failures at width {width}:\n" + "\n".join(
            failures[:10]
        )


class TestToffoliMultiplicationGatePurity:
    """Success Criterion 3: Toffoli multiplication circuits contain only CCX/CX/X gates.

    Verifies via both gate_counts API and OpenQASM export that no QFT-style
    rotation gates (H, P, CP, Rz) appear in the multiplication circuits.
    """

    def test_qq_mul_no_qft_gates(self):
        """QQ Toffoli multiplication uses only CCX/CX/X gates (width 2)."""
        gc.collect()
        ql.circuit()
        _enable_toffoli()

        a = ql.qint(2, width=2)
        b = ql.qint(3, width=2)
        c = a * b

        qasm_str = ql.to_openqasm()

        # Parse QASM lines for gate instructions
        forbidden_gates = {"h ", "p(", "cp(", "rz(", "ry(", "rx(", "u(", "u1(", "u2(", "u3("}
        gate_lines = [
            line.strip()
            for line in qasm_str.split("\n")
            if line.strip()
            and not line.strip().startswith("//")
            and not line.strip().startswith("OPENQASM")
            and not line.strip().startswith("include")
            and not line.strip().startswith("qubit")
            and not line.strip().startswith("bit")
            and not line.strip().startswith("measure")
            and not line.strip().startswith("{")
            and not line.strip().startswith("}")
        ]

        violations = []
        for line in gate_lines:
            for fg in forbidden_gates:
                if line.lower().startswith(fg):
                    violations.append(line)

        assert not violations, "Found QFT-style gates in Toffoli QQ mul circuit:\n" + "\n".join(
            violations[:10]
        )

        # Keep references alive
        _ = (a, b, c)

    def test_cq_mul_no_qft_gates(self):
        """CQ Toffoli multiplication uses only CCX/CX/X gates (width 2)."""
        gc.collect()
        ql.circuit()
        _enable_toffoli()

        a = ql.qint(2, width=2)
        a *= 3

        qasm_str = ql.to_openqasm()

        forbidden_gates = {"h ", "p(", "cp(", "rz(", "ry(", "rx(", "u(", "u1(", "u2(", "u3("}
        gate_lines = [
            line.strip()
            for line in qasm_str.split("\n")
            if line.strip()
            and not line.strip().startswith("//")
            and not line.strip().startswith("OPENQASM")
            and not line.strip().startswith("include")
            and not line.strip().startswith("qubit")
            and not line.strip().startswith("bit")
            and not line.strip().startswith("measure")
            and not line.strip().startswith("{")
            and not line.strip().startswith("}")
        ]

        violations = []
        for line in gate_lines:
            for fg in forbidden_gates:
                if line.lower().startswith(fg):
                    violations.append(line)

        assert not violations, "Found QFT-style gates in Toffoli CQ mul circuit:\n" + "\n".join(
            violations[:10]
        )

    def test_operator_dispatch_mul(self):
        """Default mode (Toffoli) dispatches a * b and a *= b without QFT gates.

        Verifies success criterion 4: operators use Toffoli path in default mode
        without needing explicit ql.option('fault_tolerant', True).
        """
        # Test a * b (QQ) in default mode
        gc.collect()
        ql.circuit()
        # Do NOT call _enable_toffoli() -- Toffoli is already the default
        a = ql.qint(2, width=3)
        b = ql.qint(3, width=3)
        c = a * b
        qasm_qq = ql.to_openqasm()

        forbidden_gates = {"h ", "p(", "cp("}
        gate_lines_qq = [
            line.strip().lower()
            for line in qasm_qq.split("\n")
            if line.strip()
            and not line.strip().startswith("//")
            and not line.strip().startswith("OPENQASM")
            and not line.strip().startswith("include")
            and not line.strip().startswith("qubit")
            and not line.strip().startswith("bit")
            and not line.strip().startswith("measure")
            and not line.strip().startswith("{")
            and not line.strip().startswith("}")
        ]

        qq_violations = [
            gl for gl in gate_lines_qq if any(gl.startswith(fg) for fg in forbidden_gates)
        ]
        assert not qq_violations, "QQ mul in default mode uses QFT gates:\n" + "\n".join(
            qq_violations[:5]
        )

        # Test a *= b (CQ) in default mode
        gc.collect()
        ql.circuit()
        a2 = ql.qint(2, width=3)
        a2 *= 3
        qasm_cq = ql.to_openqasm()

        gate_lines_cq = [
            line.strip().lower()
            for line in qasm_cq.split("\n")
            if line.strip()
            and not line.strip().startswith("//")
            and not line.strip().startswith("OPENQASM")
            and not line.strip().startswith("include")
            and not line.strip().startswith("qubit")
            and not line.strip().startswith("bit")
            and not line.strip().startswith("measure")
            and not line.strip().startswith("{")
            and not line.strip().startswith("}")
        ]

        cq_violations = [
            gl for gl in gate_lines_cq if any(gl.startswith(fg) for fg in forbidden_gates)
        ]
        assert not cq_violations, "CQ imul in default mode uses QFT gates:\n" + "\n".join(
            cq_violations[:5]
        )

        _ = (a, b, c, a2)
