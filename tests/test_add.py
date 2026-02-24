"""Verification tests for QFT addition operations.

Tests that addition (QQ and CQ variants) produces correct results
through the full pipeline: Python API -> C backend -> OpenQASM -> Qiskit simulation.

These tests use QFT mode explicitly (fault_tolerant=False) because the verify_circuit
fixture extracts results from the highest qubit indices, which doesn't account for
ancilla qubits allocated by the Toffoli path. Toffoli addition correctness
is verified separately in test_toffoli_addition.py.

Coverage:
- Exhaustive: All input pairs for widths 1-4 bits (QQ and CQ)
- Sampled: Representative pairs for widths 5-8 bits (QQ and CQ)
"""

import warnings

import pytest
from verify_helpers import (
    format_failure_message,
    generate_exhaustive_pairs,
    generate_sampled_pairs,
)

import quantum_language as ql

# Suppress cosmetic warnings for values >= 2^(width-1) in unsigned interpretation
warnings.filterwarnings("ignore", message="Value .* exceeds")


# --- Module-level test data generation ---


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
        # Merge boundary pairs with sampled pairs, deduplicated
        all_pairs = sorted(set(sampled) | set(boundary_pairs))
        for a, b in all_pairs:
            cases.append((width, a, b))
    return cases


EXHAUSTIVE_QQ_ADD = _exhaustive_cases()
SAMPLED_QQ_ADD = _sampled_cases()
EXHAUSTIVE_CQ_ADD = _exhaustive_cases()
SAMPLED_CQ_ADD = _sampled_cases()


# --- QQ Addition Tests ---


@pytest.mark.parametrize("width,a,b", EXHAUSTIVE_QQ_ADD)
def test_qq_add_exhaustive(verify_circuit, width, a, b):
    """QQ addition: qint(a) + qint(b) at widths 1-4 bits.

    Exhaustive coverage of all input pairs ensures correctness for
    fundamental bit widths. Total: 4 + 16 + 64 + 256 = 340 test cases.
    """

    def circuit_builder(a=a, b=b, w=width):
        ql.option("fault_tolerant", False)  # QFT mode for verify_circuit fixture
        qa = ql.qint(a, width=w)
        qb = ql.qint(b, width=w)
        _r = qa + qb
        return (a + b) % (1 << w)

    actual, expected = verify_circuit(circuit_builder, width)
    assert actual == expected, format_failure_message("qq_add", [a, b], width, expected, actual)


@pytest.mark.parametrize("width,a,b", SAMPLED_QQ_ADD)
def test_qq_add_sampled(verify_circuit, width, a, b):
    """QQ addition: qint(a) + qint(b) at widths 5-8 bits.

    Sampled coverage includes boundary values and random pairs for
    representative testing at larger bit widths.
    """

    def circuit_builder(a=a, b=b, w=width):
        ql.option("fault_tolerant", False)  # QFT mode for verify_circuit fixture
        qa = ql.qint(a, width=w)
        qb = ql.qint(b, width=w)
        _r = qa + qb
        return (a + b) % (1 << w)

    actual, expected = verify_circuit(circuit_builder, width)
    assert actual == expected, format_failure_message("qq_add", [a, b], width, expected, actual)


# --- CQ Addition Tests ---


@pytest.mark.parametrize("width,a,b", EXHAUSTIVE_CQ_ADD)
def test_cq_add_exhaustive(verify_circuit, width, a, b):
    """CQ addition: qint(a) += int(b) at widths 1-4 bits.

    Exhaustive coverage of all input pairs for the classical-quantum
    addition variant (in-place addition with classical operand).
    """

    def circuit_builder(a=a, b=b, w=width):
        ql.option("fault_tolerant", False)  # QFT mode for verify_circuit fixture
        qa = ql.qint(a, width=w)
        qa += b
        return (a + b) % (1 << w)

    actual, expected = verify_circuit(circuit_builder, width)
    assert actual == expected, format_failure_message("cq_add", [a, b], width, expected, actual)


@pytest.mark.parametrize("width,a,b", SAMPLED_CQ_ADD)
def test_cq_add_sampled(verify_circuit, width, a, b):
    """CQ addition: qint(a) += int(b) at widths 5-8 bits.

    Sampled coverage for the classical-quantum addition variant
    at larger bit widths.
    """

    def circuit_builder(a=a, b=b, w=width):
        ql.option("fault_tolerant", False)  # QFT mode for verify_circuit fixture
        qa = ql.qint(a, width=w)
        qa += b
        return (a + b) % (1 << w)

    actual, expected = verify_circuit(circuit_builder, width)
    assert actual == expected, format_failure_message("cq_add", [a, b], width, expected, actual)


# --- Mixed-Width QQ Addition Tests (BUG-04 regression) ---


def _mixed_width_cases():
    """Generate (wa, wb, a, b) for mixed-width QQ addition testing.

    Covers representative width combinations:
    - Off-by-one: (2,3), (3,4), (4,5), (5,6)
    - Max asymmetry: (1,4), (1,8), (2,8), (3,8)

    For each combination, tests boundary and a few random values.
    """
    import random as _rng

    _rng.seed(86_04)  # Deterministic seed for reproducibility
    combos = [(2, 3), (3, 4), (4, 5), (5, 6), (1, 4), (1, 8), (2, 8), (3, 8)]
    cases = []
    for wa, wb in combos:
        max_a = (1 << wa) - 1
        max_b = (1 << wb) - 1
        # Boundary pairs
        boundary = [(0, 0), (0, max_b), (max_a, 0), (max_a, max_b), (1, 1)]
        # Random pairs
        rand_pairs = [(_rng.randint(0, max_a), _rng.randint(0, max_b)) for _ in range(3)]
        for a, b in sorted(set(boundary + rand_pairs)):
            cases.append((wa, wb, a, b))
    return cases


MIXED_WIDTH_QQ_ADD = _mixed_width_cases()


@pytest.mark.parametrize("wa,wb,a,b", MIXED_WIDTH_QQ_ADD)
def test_qq_add_mixed_width(verify_circuit, wa, wb, a, b):
    """Mixed-width QQ addition: qint(a, width=wa) + qint(b, width=wb).

    Verifies BUG-04 fix: when operand widths differ, the narrower operand
    is zero-extended so QQ_add receives result_width qubits for both registers.
    """
    result_width = max(wa, wb)

    def circuit_builder(a=a, b=b, wa=wa, wb=wb, rw=result_width):
        ql.option("fault_tolerant", False)  # QFT mode
        qa = ql.qint(a, width=wa)
        qb = ql.qint(b, width=wb)
        _r = qa + qb
        return (a + b) % (1 << rw)

    actual, expected = verify_circuit(circuit_builder, result_width)
    assert actual == expected, format_failure_message(
        "qq_add_mixed", [f"({wa}){a}", f"({wb}){b}"], result_width, expected, actual
    )
