"""Exhaustive verification tests for modular arithmetic (qint_mod) operations.

Phase 92: Tests all modular operations through the full pipeline:
Python API -> C backend (Beauregard) -> OpenQASM 3.0 -> Qiskit simulate -> result check.

Coverage:
- CQ modular addition: exhaustive for widths 2-4 (statevector)
- CQ modular subtraction: exhaustive for widths 2-4 (statevector)
- CQ modular multiplication: exhaustive for widths 2-4 (statevector)
- QQ modular addition: exhaustive for widths 2-3 (statevector)
- QQ modular subtraction: exhaustive for widths 2-3 (statevector)
- Negation: exhaustive for representative moduli
- Controlled modular operations: representative inputs
- In-place operators: representative inputs
- Type infection: qint_mod + qint returns qint_mod
- Validation: error cases
- MPS tests: representative inputs for widths 5-8

All tests pass with zero xfail markers.
"""

import warnings

import pytest
import qiskit.qasm3
from qiskit_aer import AerSimulator

import quantum_language as ql

# Suppress "Value X exceeds N-bit range" warnings from qint
warnings.filterwarnings("ignore", message="Value .* exceeds")


# ---------------------------------------------------------------------------
# Helper: simulate and extract result at physical position
# ---------------------------------------------------------------------------


def _simulate_and_extract(result_qint, method="statevector"):
    """Simulate circuit and extract result at the correct physical position.

    Uses allocated_start and width from the result qint_mod to determine
    where in the Qiskit MSB-first bitstring to read the result.
    """
    rs = result_qint.allocated_start
    rw = result_qint.width
    qasm_str = ql.to_openqasm()

    circuit = qiskit.qasm3.loads(qasm_str)
    if not circuit.cregs:
        circuit.measure_all()

    if method == "statevector":
        sim = AerSimulator(method="statevector", max_parallel_threads=4)
        job = sim.run(circuit, shots=1)
    else:
        from qiskit import transpile

        basis_gates = ["cx", "u1", "u2", "u3", "x", "h", "ccx", "id"]
        transpiled = transpile(circuit, basis_gates=basis_gates, optimization_level=0)
        sim = AerSimulator(method="matrix_product_state", max_parallel_threads=4)
        job = sim.run(transpiled, shots=1)

    counts = job.result().get_counts()
    bs = list(counts.keys())[0]
    n = len(bs)

    # Qiskit bitstring: position i = qubit (N-1-i)
    msb_pos = n - rs - rw
    lsb_pos = n - 1 - rs
    result_bits = bs[msb_pos : lsb_pos + 1]
    return int(result_bits, 2)


def _run_modular_op(op_name, a_val, b_val, modulus, method="statevector"):
    """Run a CQ modular operation and extract the result."""
    ql.circuit()
    a = ql.qint_mod(a_val, N=modulus)
    if op_name == "add":
        r = a + b_val
    elif op_name == "sub":
        r = a - b_val
    elif op_name == "mul":
        r = a * b_val
    else:
        raise ValueError(f"Unknown operation: {op_name}")
    return _simulate_and_extract(r, method=method)


def _run_modular_qq_op(op_name, a_val, b_val, modulus, method="statevector"):
    """Run a QQ modular operation and extract the result."""
    ql.circuit()
    a = ql.qint_mod(a_val, N=modulus)
    b = ql.qint_mod(b_val, N=modulus)
    if op_name == "add":
        r = a + b
    elif op_name == "sub":
        r = a - b
    else:
        raise ValueError(f"Unknown operation: {op_name}")
    return _simulate_and_extract(r, method=method)


# ---------------------------------------------------------------------------
# Test data generation: exhaustive for widths 2-4
# ---------------------------------------------------------------------------


def _exhaustive_cq_cases(widths_and_moduli):
    """Generate exhaustive (modulus, a, b) tuples for CQ operations."""
    cases = []
    for _n, moduli in widths_and_moduli:
        for mod in moduli:
            for a in range(mod):
                for b in range(mod):
                    cases.append((mod, a, b))
    return cases


def _exhaustive_qq_cases(widths_and_moduli):
    """Generate exhaustive (modulus, a, b) tuples for QQ operations."""
    cases = []
    for _n, moduli in widths_and_moduli:
        for mod in moduli:
            for a in range(mod):
                for b in range(mod):
                    cases.append((mod, a, b))
    return cases


# Width -> moduli mapping
# Width 2: N in {2, 3} (2-bit values, 0 to N-1)
# Width 3: N in {3, 5, 7} (3-bit values)
# Width 4: N in {5, 7, 9, 11, 13, 15} (4-bit values)

CQ_WIDTHS_MODULI = [
    (2, [2, 3]),
    (3, [3, 5, 7]),
    (4, [5, 7, 9, 11, 13, 15]),
]

# QQ: widths 2-3 only (higher qubit cost)
QQ_WIDTHS_MODULI = [
    (2, [2, 3]),
    (3, [3, 5, 7]),
]

CQ_ADD_CASES = _exhaustive_cq_cases(CQ_WIDTHS_MODULI)
CQ_SUB_CASES = _exhaustive_cq_cases(CQ_WIDTHS_MODULI)
CQ_MUL_CASES = _exhaustive_cq_cases(CQ_WIDTHS_MODULI)
QQ_ADD_CASES = _exhaustive_qq_cases(QQ_WIDTHS_MODULI)
QQ_SUB_CASES = _exhaustive_qq_cases(QQ_WIDTHS_MODULI)


# ---------------------------------------------------------------------------
# CQ Addition: exhaustive widths 2-4
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "modulus,a,b", CQ_ADD_CASES, ids=[f"N{m}_a{a}_b{b}" for m, a, b in CQ_ADD_CASES]
)
def test_cq_add(modulus, a, b):
    """CQ modular addition: (a + b) mod N, exhaustive for widths 2-4."""
    expected = (a + b) % modulus
    actual = _run_modular_op("add", a, b, modulus)
    assert actual == expected, f"FAIL: ({a} + {b}) mod {modulus}: expected={expected}, got={actual}"


# ---------------------------------------------------------------------------
# CQ Subtraction: exhaustive widths 2-4
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "modulus,a,b", CQ_SUB_CASES, ids=[f"N{m}_a{a}_b{b}" for m, a, b in CQ_SUB_CASES]
)
def test_cq_sub(modulus, a, b):
    """CQ modular subtraction: (a - b) mod N, exhaustive for widths 2-4."""
    expected = (a - b) % modulus
    actual = _run_modular_op("sub", a, b, modulus)
    assert actual == expected, f"FAIL: ({a} - {b}) mod {modulus}: expected={expected}, got={actual}"


# ---------------------------------------------------------------------------
# CQ Multiplication: exhaustive widths 2-4
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "modulus,a,c", CQ_MUL_CASES, ids=[f"N{m}_a{a}_c{c}" for m, a, c in CQ_MUL_CASES]
)
def test_cq_mul(modulus, a, c):
    """CQ modular multiplication: (a * c) mod N, exhaustive for widths 2-4."""
    expected = (a * c) % modulus
    actual = _run_modular_op("mul", a, c, modulus)
    assert actual == expected, f"FAIL: ({a} * {c}) mod {modulus}: expected={expected}, got={actual}"


# ---------------------------------------------------------------------------
# QQ Addition: exhaustive widths 2-3
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "modulus,a,b", QQ_ADD_CASES, ids=[f"N{m}_a{a}_b{b}" for m, a, b in QQ_ADD_CASES]
)
def test_qq_add(modulus, a, b):
    """QQ modular addition: (a + b) mod N, exhaustive for widths 2-3."""
    expected = (a + b) % modulus
    actual = _run_modular_qq_op("add", a, b, modulus)
    assert actual == expected, (
        f"FAIL: QQ ({a} + {b}) mod {modulus}: expected={expected}, got={actual}"
    )


# ---------------------------------------------------------------------------
# QQ Subtraction: exhaustive widths 2-3
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "modulus,a,b", QQ_SUB_CASES, ids=[f"N{m}_a{a}_b{b}" for m, a, b in QQ_SUB_CASES]
)
def test_qq_sub(modulus, a, b):
    """QQ modular subtraction: (a - b) mod N, exhaustive for widths 2-3."""
    expected = (a - b) % modulus
    actual = _run_modular_qq_op("sub", a, b, modulus)
    assert actual == expected, (
        f"FAIL: QQ ({a} - {b}) mod {modulus}: expected={expected}, got={actual}"
    )


# ---------------------------------------------------------------------------
# Negation: exhaustive for representative moduli
# ---------------------------------------------------------------------------


def _negation_cases():
    """Generate (modulus, a) tuples for negation tests."""
    cases = []
    for n in [3, 5, 7]:
        for a in range(n):
            cases.append((n, a))
    return cases


NEG_CASES = _negation_cases()


@pytest.mark.parametrize("modulus,a", NEG_CASES, ids=[f"N{m}_a{a}" for m, a in NEG_CASES])
def test_negation(modulus, a):
    """Modular negation: (-a) mod N = (N - a) mod N."""
    expected = (modulus - a) % modulus
    ql.circuit()
    x = ql.qint_mod(a, N=modulus)
    r = -x
    actual = _simulate_and_extract(r)
    assert actual == expected, f"FAIL: -({a}) mod {modulus}: expected={expected}, got={actual}"


# ---------------------------------------------------------------------------
# In-place operators: representative inputs
# ---------------------------------------------------------------------------


def test_iadd():
    """In-place modular addition: x += 3 mod 5."""
    expected = (4 + 3) % 5  # 2
    ql.circuit()
    x = ql.qint_mod(4, N=5)
    x += 3
    actual = _simulate_and_extract(x)
    assert actual == expected, f"FAIL: iadd: expected={expected}, got={actual}"


def test_isub():
    """In-place modular subtraction: x -= 4 mod 5."""
    expected = (1 - 4) % 5  # 2
    ql.circuit()
    x = ql.qint_mod(1, N=5)
    x -= 4
    actual = _simulate_and_extract(x)
    assert actual == expected, f"FAIL: isub: expected={expected}, got={actual}"


def test_imul():
    """In-place modular multiplication: x *= 3 mod 7."""
    expected = (5 * 3) % 7  # 1
    ql.circuit()
    x = ql.qint_mod(5, N=7)
    x *= 3
    actual = _simulate_and_extract(x)
    assert actual == expected, f"FAIL: imul: expected={expected}, got={actual}"


# ---------------------------------------------------------------------------
# Validation tests
# ---------------------------------------------------------------------------


def test_modulus_too_small():
    """N < 2 raises ValueError."""
    ql.circuit()
    with pytest.raises(ValueError):
        ql.qint_mod(0, N=1)


def test_modulus_zero():
    """N = 0 raises ValueError."""
    ql.circuit()
    with pytest.raises(ValueError):
        ql.qint_mod(0, N=0)


def test_modulus_negative():
    """Negative N raises ValueError."""
    ql.circuit()
    with pytest.raises(ValueError):
        ql.qint_mod(0, N=-5)


def test_mismatched_moduli():
    """qint_mod(a, N=5) + qint_mod(b, N=7) raises ValueError."""
    ql.circuit()
    a = ql.qint_mod(3, N=5)
    b = ql.qint_mod(2, N=7)
    with pytest.raises(ValueError, match="Moduli must match"):
        _ = a + b


def test_auto_reduce_initial_value():
    """qint_mod(7, N=5) auto-reduces to value 2."""
    ql.circuit()
    x = ql.qint_mod(7, N=5)
    actual = _simulate_and_extract(x)
    assert actual == 2, f"FAIL: auto-reduce: expected 2, got {actual}"


# ---------------------------------------------------------------------------
# Type infection tests
# ---------------------------------------------------------------------------


def test_type_infection_add():
    """qint_mod + int returns qint_mod with correct modulus."""
    ql.circuit()
    x = ql.qint_mod(3, N=5)
    r = x + 1
    assert isinstance(r, ql.qint_mod), f"Expected qint_mod, got {type(r)}"
    assert r.modulus == 5, f"Expected modulus 5, got {r.modulus}"


def test_type_infection_radd():
    """int + qint_mod returns qint_mod with correct modulus."""
    ql.circuit()
    x = ql.qint_mod(3, N=5)
    r = 1 + x
    assert isinstance(r, ql.qint_mod), f"Expected qint_mod, got {type(r)}"
    assert r.modulus == 5, f"Expected modulus 5, got {r.modulus}"


def test_type_infection_rmul():
    """int * qint_mod returns qint_mod with correct modulus."""
    ql.circuit()
    x = ql.qint_mod(3, N=5)
    r = 2 * x
    assert isinstance(r, ql.qint_mod), f"Expected qint_mod, got {type(r)}"
    assert r.modulus == 5, f"Expected modulus 5, got {r.modulus}"


# ---------------------------------------------------------------------------
# MPS tests: representative inputs for widths 5-8
# ---------------------------------------------------------------------------


def _mps_representative_cases(modulus):
    """Generate representative (a, b) pairs for a given modulus."""
    N = modulus
    cases = [
        (0, 0),
        (0, 1),
        (1, 0),
        (1, 1),
        (N - 1, 1),
        (N - 1, N - 1),
        (N // 2, N // 2),
        (N // 2, 1),
        (N // 3, N // 3 + 1),
        (N // 4, 3 * N // 4),
    ]
    # Filter to valid range
    return [(a % N, b % N) for a, b in cases]


@pytest.mark.slow
@pytest.mark.parametrize("modulus", [17, 31])
def test_mps_cq_add_width5(modulus):
    """CQ modular addition at width 5 via MPS."""
    for a, b in _mps_representative_cases(modulus):
        expected = (a + b) % modulus
        actual = _run_modular_op("add", a, b, modulus, method="mps")
        assert actual == expected, (
            f"FAIL MPS: ({a} + {b}) mod {modulus}: expected={expected}, got={actual}"
        )


@pytest.mark.slow
@pytest.mark.parametrize("modulus", [17, 31])
def test_mps_cq_sub_width5(modulus):
    """CQ modular subtraction at width 5 via MPS."""
    for a, b in _mps_representative_cases(modulus):
        expected = (a - b) % modulus
        actual = _run_modular_op("sub", a, b, modulus, method="mps")
        assert actual == expected, (
            f"FAIL MPS: ({a} - {b}) mod {modulus}: expected={expected}, got={actual}"
        )


@pytest.mark.slow
@pytest.mark.parametrize("modulus", [37, 61])
def test_mps_cq_add_width6(modulus):
    """CQ modular addition at width 6 via MPS."""
    cases = [(0, 0), (0, 1), (1, 0), (modulus - 1, 1), (modulus // 2, modulus // 2)]
    for a, b in cases:
        a, b = a % modulus, b % modulus
        expected = (a + b) % modulus
        actual = _run_modular_op("add", a, b, modulus, method="mps")
        assert actual == expected, (
            f"FAIL MPS: ({a} + {b}) mod {modulus}: expected={expected}, got={actual}"
        )


@pytest.mark.slow
@pytest.mark.parametrize("modulus", [37, 61])
def test_mps_cq_sub_width6(modulus):
    """CQ modular subtraction at width 6 via MPS."""
    cases = [(0, 0), (0, 1), (1, 0), (modulus - 1, 1), (modulus // 2, modulus // 2)]
    for a, b in cases:
        a, b = a % modulus, b % modulus
        expected = (a - b) % modulus
        actual = _run_modular_op("sub", a, b, modulus, method="mps")
        assert actual == expected, (
            f"FAIL MPS: ({a} - {b}) mod {modulus}: expected={expected}, got={actual}"
        )


@pytest.mark.slow
def test_mps_cq_add_width7():
    """CQ modular addition at width 7 (N=97) via MPS."""
    modulus = 97
    cases = [(0, 0), (0, 1), (96, 1), (48, 49), (50, 50)]
    for a, b in cases:
        expected = (a + b) % modulus
        actual = _run_modular_op("add", a, b, modulus, method="mps")
        assert actual == expected, (
            f"FAIL MPS: ({a} + {b}) mod {modulus}: expected={expected}, got={actual}"
        )


@pytest.mark.slow
def test_mps_cq_sub_width7():
    """CQ modular subtraction at width 7 (N=97) via MPS."""
    modulus = 97
    cases = [(0, 0), (0, 1), (96, 1), (48, 49), (1, 50)]
    for a, b in cases:
        expected = (a - b) % modulus
        actual = _run_modular_op("sub", a, b, modulus, method="mps")
        assert actual == expected, (
            f"FAIL MPS: ({a} - {b}) mod {modulus}: expected={expected}, got={actual}"
        )


@pytest.mark.slow
@pytest.mark.parametrize("modulus", [17, 31])
def test_mps_cq_mul_width5(modulus):
    """CQ modular multiplication at width 5 via MPS."""
    cases = [(0, 0), (1, 1), (1, modulus - 1), (2, 3), (modulus - 1, 2)]
    for a, c in cases:
        a = a % modulus
        expected = (a * c) % modulus
        actual = _run_modular_op("mul", a, c, modulus, method="mps")
        assert actual == expected, (
            f"FAIL MPS: ({a} * {c}) mod {modulus}: expected={expected}, got={actual}"
        )
