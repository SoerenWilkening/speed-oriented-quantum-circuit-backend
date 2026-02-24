"""Verification tests for cQQ_add (controlled quantum-quantum addition).

Tests that controlled addition (cQQ_add) produces correct results through the
full pipeline: Python API -> C backend -> OpenQASM -> Qiskit simulation.

BUG-05 fix: The source qubit mapping in cQQ_add Blocks 2 and 3 was reversed
relative to QQ_add, causing swapped source bits. Fixed by using the same
`bits + (bits - 1 - bit)` mapping as QQ_add.

The cQQ_add operation adds a += b only when the control qubit is |1>.
When the control qubit is |0>, a remains unchanged.

Qubit allocation order: ctrl(1 qubit) -> a(width qubits) -> b(width qubits)
This means a is NOT at the highest qubit indices, so standard verify_circuit
extraction doesn't work. Instead, we extract a's value from the bitstring
using known qubit positions.

Coverage:
- Exhaustive: All input pairs for widths 1-4 bits (ctrl=1 and ctrl=0)
- Sampled: Representative pairs for widths 5-8 bits (ctrl=1 and ctrl=0)
"""

import gc
import warnings

import pytest
import qiskit.qasm3
from qiskit_aer import AerSimulator
from verify_helpers import (
    format_failure_message,
    generate_exhaustive_pairs,
    generate_sampled_pairs,
)

import quantum_language as ql

# Suppress cosmetic warnings for values >= 2^(width-1)
warnings.filterwarnings("ignore", message="Value .* exceeds")


# --- Helper ---


def _run_controlled_add(a_val, b_val, width, ctrl_val):
    """Run controlled QQ addition and extract result.

    Args:
        a_val: Initial value of a register
        b_val: Initial value of b register
        width: Bit width of a and b
        ctrl_val: Control qubit value (0 or 1)

    Returns:
        Actual result extracted from a register after controlled addition.
    """
    gc.collect()
    ql.circuit()
    ql.option("fault_tolerant", False)  # QFT mode

    ctrl = ql.qint(ctrl_val, width=1)
    a = ql.qint(a_val, width=width)
    b = ql.qint(b_val, width=width)

    with ctrl:
        a += b

    qasm_str = ql.to_openqasm()
    qc = qiskit.qasm3.loads(qasm_str)
    qc.measure_all()

    sim = AerSimulator(method="statevector", max_parallel_threads=4)
    result = sim.run(qc, shots=1).result()
    bitstring = list(result.get_counts().keys())[0]
    n_qubits = len(bitstring)

    # Extract a from bitstring.
    # Allocation order: ctrl at q[0], a at q[1..width], b at q[width+1..2*width]
    # Bitstring is big-endian: bitstring[i] = qubit q[n_qubits - 1 - i]
    # a's MSB is q[width], a's LSB is q[1]
    val = 0
    for bit_idx in range(width - 1, -1, -1):  # MSB first
        qubit_idx = 1 + bit_idx
        bs_pos = n_qubits - 1 - qubit_idx
        val = val * 2 + int(bitstring[bs_pos])
    return val


# --- Test data generation ---


def _exhaustive_cases():
    """Generate (width, a, b) tuples for exhaustive testing (widths 1-4)."""
    cases = []
    for width in [1, 2, 3, 4]:
        for a, b in generate_exhaustive_pairs(width):
            cases.append((width, a, b))
    return cases


def _sampled_cases():
    """Generate (width, a, b) tuples for sampled testing (widths 5-8)."""
    cases = []
    for width in [5, 6, 7, 8]:
        max_val = (1 << width) - 1
        boundary_pairs = [(0, 0), (0, max_val), (max_val, 0), (max_val, max_val)]
        sampled = generate_sampled_pairs(width, sample_size=10)
        all_pairs = sorted(set(sampled) | set(boundary_pairs))
        for a, b in all_pairs:
            cases.append((width, a, b))
    return cases


EXHAUSTIVE_CQQ = _exhaustive_cases()
SAMPLED_CQQ = _sampled_cases()


# --- cQQ Addition Tests (ctrl=1: addition should happen) ---


@pytest.mark.parametrize("width,a,b", EXHAUSTIVE_CQQ)
def test_cqq_add_ctrl_on_exhaustive(width, a, b):
    """cQQ addition with ctrl=1: a += b at widths 1-4 bits.

    Exhaustive coverage ensures correctness of the Beauregard CCP decomposition
    for fundamental bit widths. Total: 4 + 16 + 64 + 256 = 340 test cases.

    BUG-05 regression: verifies source qubit mapping matches QQ_add convention.
    """
    expected = (a + b) % (1 << width)
    actual = _run_controlled_add(a, b, width, ctrl_val=1)
    assert actual == expected, format_failure_message("cqq_add_on", [a, b], width, expected, actual)


@pytest.mark.parametrize("width,a,b", SAMPLED_CQQ)
def test_cqq_add_ctrl_on_sampled(width, a, b):
    """cQQ addition with ctrl=1: a += b at widths 5-8 bits.

    Sampled coverage includes boundary values and random pairs for
    representative testing at larger bit widths.
    """
    expected = (a + b) % (1 << width)
    actual = _run_controlled_add(a, b, width, ctrl_val=1)
    assert actual == expected, format_failure_message("cqq_add_on", [a, b], width, expected, actual)


# --- cQQ Addition Tests (ctrl=0: should be no-op) ---


@pytest.mark.parametrize("width,a,b", EXHAUSTIVE_CQQ)
def test_cqq_add_ctrl_off_exhaustive(width, a, b):
    """cQQ addition with ctrl=0: a should remain unchanged at widths 1-4 bits.

    When control qubit is |0>, the controlled addition should be a no-op.
    This verifies the Beauregard CCP decomposition correctly cancels when
    the control is off.
    """
    expected = a  # No addition when ctrl=0
    actual = _run_controlled_add(a, b, width, ctrl_val=0)
    assert actual == expected, format_failure_message(
        "cqq_add_off", [a, b], width, expected, actual
    )


@pytest.mark.parametrize("width,a,b", SAMPLED_CQQ)
def test_cqq_add_ctrl_off_sampled(width, a, b):
    """cQQ addition with ctrl=0: a should remain unchanged at widths 5-8 bits.

    Sampled coverage for the no-op verification at larger bit widths.
    """
    expected = a
    actual = _run_controlled_add(a, b, width, ctrl_val=0)
    assert actual == expected, format_failure_message(
        "cqq_add_off", [a, b], width, expected, actual
    )
