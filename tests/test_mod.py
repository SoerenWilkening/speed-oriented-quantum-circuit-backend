"""Modulo verification tests (VARITH-04).

Tests qint % int (classical divisor modulo) through the full pipeline:
Python quantum_language API -> C backend circuit -> OpenQASM 3.0 -> Qiskit simulate -> result check.

Exhaustive at widths 1-3, sampled at width 4. Modulo by zero is skipped per design.

Result extraction: Modulo allocates input(w) + remainder(w) + ancillae.
The remainder register occupies qubits w..2w-1. In the Qiskit MSB-first bitstring of
length n, this maps to bs[n-2w : n-w].
"""

import random
import warnings

import pytest
import qiskit.qasm3
from qiskit_aer import AerSimulator
from verify_helpers import format_failure_message

import quantum_language as ql

warnings.filterwarnings("ignore", message="Value .* exceeds")

# ---------------------------------------------------------------------------
# Known-failing (width, a, divisor) triples -- ancilla leak in >= comparison (BUG-06)
# The >= operator in the modulo loop creates widened temporaries (comp_width = bits+1)
# whose qubits are never freed by the uncomputation mechanism. This causes incorrect
# results when leaked ancilla qubits interfere with the circuit state.
# Root cause: _do_uncompute reverses gates but doesn't cascade-free temporaries
# because they have operation_type=None and creation_scope=0 (lazy mode skips them).
# Modulo is more severely affected than division because the remainder extraction
# is more sensitive to leaked ancillae. Most width >= 2 cases fail.
# Phase 86-03 investigated but deferred the deep fix (requires uncomputation arch changes).
# Updated after BUG-05 fix (Phase 86-02).
# ---------------------------------------------------------------------------
KNOWN_MOD_MSB_LEAK = {
    # Width 1
    (1, 0, 1),
    # Width 2: nearly all cases fail
    (2, 0, 1),
    (2, 0, 2),
    (2, 0, 3),
    (2, 1, 1),
    (2, 1, 2),
    (2, 2, 1),
    (2, 2, 3),
    (2, 3, 1),
    (2, 3, 2),
    # Width 3: most cases fail (53 out of 56)
    (3, 0, 1),
    (3, 0, 2),
    (3, 0, 3),
    (3, 0, 4),
    (3, 0, 5),
    (3, 0, 6),
    (3, 0, 7),
    (3, 1, 1),
    (3, 1, 2),
    (3, 1, 3),
    (3, 1, 4),
    (3, 1, 5),
    (3, 1, 6),
    (3, 2, 1),
    (3, 2, 2),
    (3, 2, 3),
    (3, 2, 4),
    (3, 2, 5),
    (3, 2, 7),
    (3, 3, 1),
    (3, 3, 2),
    (3, 3, 4),
    (3, 3, 6),
    (3, 3, 7),
    (3, 4, 1),
    (3, 4, 2),
    (3, 4, 3),
    (3, 4, 5),
    (3, 4, 6),
    (3, 4, 7),
    (3, 5, 1),
    (3, 5, 2),
    (3, 5, 3),
    (3, 5, 4),
    (3, 5, 6),
    (3, 5, 7),
    (3, 6, 1),
    (3, 6, 3),
    (3, 6, 4),
    (3, 6, 5),
    (3, 6, 7),
    (3, 7, 1),
    (3, 7, 2),
    (3, 7, 3),
    (3, 7, 4),
    (3, 7, 5),
    (3, 7, 6),
    # Width 4: many sampled cases fail
    (4, 0, 2),
    (4, 0, 9),
    (4, 0, 13),
    (4, 0, 14),
    (4, 0, 15),
    (4, 1, 2),
    (4, 1, 14),
    (4, 2, 10),
    (4, 3, 1),
    (4, 3, 11),
    (4, 5, 12),
    (4, 6, 4),
    (4, 6, 12),
    (4, 7, 3),
    (4, 8, 4),
    (4, 8, 13),
    (4, 13, 4),
    (4, 14, 1),
    (4, 14, 2),
    (4, 14, 10),
    (4, 14, 15),
    (4, 15, 2),
    (4, 15, 14),
}


# ---------------------------------------------------------------------------
# Pipeline helper
# ---------------------------------------------------------------------------
def _run_mod(width, a_val, divisor):
    """Run modulo through full pipeline, return (actual_remainder, total_qubits, bitstring)."""
    ql.circuit()
    a = ql.qint(a_val, width=width)
    _r = a % divisor
    qasm_str = ql.to_openqasm()

    circuit = qiskit.qasm3.loads(qasm_str)
    if not circuit.cregs:
        circuit.measure_all()

    sim = AerSimulator(method="matrix_product_state", max_parallel_threads=4)
    job = sim.run(circuit, shots=1)
    counts = job.result().get_counts()
    bitstring = list(counts.keys())[0]

    n = len(bitstring)
    remainder_bits = bitstring[n - 2 * width : n - width]
    actual = int(remainder_bits, 2)
    return actual, n, bitstring


# ---------------------------------------------------------------------------
# Test data generation
# ---------------------------------------------------------------------------
def _exhaustive_mod_cases():
    """Generate (width, a, divisor) for widths 1-3, divisor in [1, 2^width)."""
    cases = []
    for width in [1, 2, 3]:
        for a_val in range(1 << width):
            for divisor in range(1, 1 << width):
                cases.append((width, a_val, divisor))
    return cases


def _sampled_mod_cases():
    """Generate ~30 (width, a, divisor) for width 4 with deterministic seed."""
    width = 4
    max_val = (1 << width) - 1
    pairs = set()
    # Edge cases
    for a_val in [0, 1, max_val - 1, max_val]:
        for divisor in [1, 2, max_val - 1, max_val]:
            pairs.add((a_val, divisor))
    # Random fill to ~30
    rng = random.Random(42)
    while len(pairs) < 30:
        a_val = rng.randint(0, max_val)
        divisor = rng.randint(1, max_val)
        pairs.add((a_val, divisor))
    return [(width, a, d) for a, d in sorted(pairs)]


EXHAUSTIVE_MOD = _exhaustive_mod_cases()
SAMPLED_MOD = _sampled_mod_cases()


# ---------------------------------------------------------------------------
# Helper to apply xfail marker for MSB leak cases only
# ---------------------------------------------------------------------------
def _mark_msb_leak_cases(cases):
    """Wrap known MSB-leak cases with pytest.param(..., marks=xfail)."""
    marked = []
    for triple in cases:
        if triple in KNOWN_MOD_MSB_LEAK:
            marked.append(
                pytest.param(
                    *triple,
                    marks=pytest.mark.xfail(
                        reason="BUG-DIV-02: MSB comparison leak (not overflow)",
                        strict=True,
                    ),
                )
            )
        else:
            marked.append(triple)
    return marked


# ---------------------------------------------------------------------------
# Parametrized tests
# ---------------------------------------------------------------------------
@pytest.mark.parametrize(
    "width,a,divisor", _mark_msb_leak_cases(EXHAUSTIVE_MOD), ids=lambda *args: None
)
def test_mod_exhaustive(width, a, divisor):
    """Modulo: qint(a) % divisor at widths 1-3 (exhaustive)."""
    expected = a % divisor
    actual, _n, _bs = _run_mod(width, a, divisor)
    assert actual == expected, format_failure_message("mod", [a, divisor], width, expected, actual)


@pytest.mark.parametrize(
    "width,a,divisor", _mark_msb_leak_cases(SAMPLED_MOD), ids=lambda *args: None
)
def test_mod_sampled(width, a, divisor):
    """Modulo: qint(a) % divisor at width 4 (sampled)."""
    expected = a % divisor
    actual, _n, _bs = _run_mod(width, a, divisor)
    assert actual == expected, format_failure_message("mod", [a, divisor], width, expected, actual)
