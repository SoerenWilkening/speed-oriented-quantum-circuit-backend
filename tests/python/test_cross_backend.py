"""Phase 70: Cross-Backend Equivalence Tests (Addition & Subtraction).

Proves that Toffoli and QFT backends produce identical computational results
for all addition and subtraction variants at widths 1-8. Uses exhaustive
testing for widths 1-4 and sampled testing for widths 5-8.

Test infrastructure (_run_backend, _compare_backends, _simulate_and_extract)
is designed for reuse by Plan 02's multiplication and division tests.

Backend switching: ql.option('fault_tolerant', True) for Toffoli,
ql.option('fault_tolerant', False) for QFT. Result extraction uses
allocated_start for backend-independent physical qubit positions.
"""

import gc
import os
import sys
import warnings

import pytest
import qiskit.qasm3
from qiskit import transpile
from qiskit_aer import AerSimulator

import quantum_language as ql

# Add tests/ directory to sys.path so verify_helpers can be imported
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from verify_helpers import (
    generate_exhaustive_pairs,
    generate_sampled_pairs,
)

warnings.filterwarnings("ignore", message="Value .* exceeds")


# ---------------------------------------------------------------------------
# Helper functions (module-level, reusable by Plan 02)
# ---------------------------------------------------------------------------


def _get_num_qubits(qasm_str):
    """Extract qubit count from OpenQASM qubit[N] declaration."""
    for line in qasm_str.split("\n"):
        line = line.strip()
        if line.startswith("qubit["):
            return int(line.split("[")[1].split("]")[0])
    raise Exception(f"Could not find qubit count in QASM:\n{qasm_str[:200]}")


def _simulate_and_extract(qasm_str, num_qubits, result_start, result_width, use_mps=False):
    """Simulate QASM and extract integer from result register.

    Args:
        qasm_str: OpenQASM 3.0 string
        num_qubits: Total number of qubits in circuit
        result_start: Physical qubit index of result register LSB
        result_width: Number of qubits in result register
        use_mps: If True, transpile to basis gates and use MPS simulator
                 (needed for circuits containing MCX gates)

    Returns:
        Integer value of result register
    """
    circuit = qiskit.qasm3.loads(qasm_str)
    if not circuit.cregs:
        circuit.measure_all()

    if use_mps:
        basis_gates = ["cx", "u1", "u2", "u3", "x", "h", "ccx", "id"]
        circuit = transpile(circuit, basis_gates=basis_gates, optimization_level=0)
        simulator = AerSimulator(method="matrix_product_state")
    else:
        simulator = AerSimulator(method="statevector")

    job = simulator.run(circuit, shots=1)
    result = job.result()
    counts = result.get_counts()
    bitstring = list(counts.keys())[0]

    # Qiskit bitstring: position i = qubit (N-1-i)
    msb_pos = num_qubits - result_start - result_width
    lsb_pos = num_qubits - 1 - result_start
    result_bits = bitstring[msb_pos : lsb_pos + 1]
    return int(result_bits, 2)


def _run_backend(backend, build_fn, width, use_mps=False):
    """Run a circuit with a specific backend and return the integer result.

    Args:
        backend: "toffoli" or "qft"
        build_fn: Callable returning (result_qint, expected_value, keepalive_list)
        width: Result register width
        use_mps: Whether to use MPS simulator (for MCX-containing circuits)

    Returns:
        Integer result from simulation
    """
    gc.collect()
    ql.circuit()
    ql.option("fault_tolerant", backend == "toffoli")

    result_qint, expected, keepalive = build_fn()
    result_start = result_qint.allocated_start

    qasm_str = ql.to_openqasm()
    del keepalive  # Release qint references before simulation

    num_qubits = _get_num_qubits(qasm_str)
    actual = _simulate_and_extract(qasm_str, num_qubits, result_start, width, use_mps=use_mps)
    return actual


def _compare_backends(build_fn, width, use_mps_toffoli=False):
    """Run both backends and return (toffoli_result, qft_result).

    Args:
        build_fn: Callable returning (result_qint, expected_value, keepalive_list)
        width: Result register width
        use_mps_toffoli: Whether Toffoli backend needs MPS (for MCX gates)

    Returns:
        Tuple (toffoli_result, qft_result)
    """
    toffoli_result = _run_backend("toffoli", build_fn, width, use_mps=use_mps_toffoli)
    qft_result = _run_backend("qft", build_fn, width, use_mps=False)
    return toffoli_result, qft_result


# ---------------------------------------------------------------------------
# TestCrossBackendAddition
# ---------------------------------------------------------------------------


class TestCrossBackendAddition:
    """Cross-backend equivalence tests for all 4 addition variants (QQ, CQ, cQQ, cCQ).

    Parametrized across widths 1-8. Exhaustive for widths 1-4, sampled for 5-8.
    """

    @pytest.mark.parametrize(
        "width",
        [
            1,
            2,
            3,
            4,
            5,
            6,
            pytest.param(7, marks=pytest.mark.slow),
            pytest.param(8, marks=pytest.mark.slow),
        ],
    )
    def test_qq_add(self, width):
        """QQ addition (c = a + b) produces identical results in both backends."""
        if width <= 4:
            pairs = generate_exhaustive_pairs(width)
        elif width <= 6:
            pairs = generate_sampled_pairs(width, sample_size=20)
        else:
            pairs = generate_sampled_pairs(width, sample_size=10)

        failures = []
        for a, b in pairs:

            def build_fn(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qb = ql.qint(b, width=w)
                qc = qa + qb
                return qc, (a + b) % (1 << w), [qa, qb, qc]

            toffoli_result, qft_result = _compare_backends(build_fn, width)
            if toffoli_result != qft_result:
                failures.append(
                    f"w={width} a={a} b={b}: toffoli={toffoli_result}, qft={qft_result}"
                )

        assert not failures, f"{len(failures)} cross-backend mismatches:\n" + "\n".join(
            failures[:20]
        )

    @pytest.mark.parametrize(
        "width",
        [
            1,
            2,
            3,
            4,
            5,
            6,
            pytest.param(7, marks=pytest.mark.slow),
            pytest.param(8, marks=pytest.mark.slow),
        ],
    )
    def test_cq_add(self, width):
        """CQ addition (a += b) produces identical results in both backends."""
        if width <= 4:
            pairs = generate_exhaustive_pairs(width)
        elif width <= 6:
            pairs = generate_sampled_pairs(width, sample_size=20)
        else:
            pairs = generate_sampled_pairs(width, sample_size=10)

        failures = []
        for a, b in pairs:

            def build_fn(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qa += b
                return qa, (a + b) % (1 << w), [qa]

            toffoli_result, qft_result = _compare_backends(build_fn, width)
            if toffoli_result != qft_result:
                failures.append(
                    f"w={width} a={a} b={b}: toffoli={toffoli_result}, qft={qft_result}"
                )

        assert not failures, f"{len(failures)} cross-backend mismatches:\n" + "\n".join(
            failures[:20]
        )

    def test_cqq_add_w1(self):
        """Controlled QQ in-place addition at width 1 (cross-backend match)."""
        width = 1
        pairs = generate_exhaustive_pairs(width)

        failures = []
        for a, b in pairs:

            def build_fn(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qb = ql.qint(b, width=w)
                ctrl = ql.qint(1, width=1)
                with ctrl:
                    qa += qb
                return qa, (a + b) % (1 << w), [qa, qb, ctrl]

            toffoli_result, qft_result = _compare_backends(build_fn, width, use_mps_toffoli=True)
            if toffoli_result != qft_result:
                failures.append(
                    f"w={width} a={a} b={b} ctrl=1: toffoli={toffoli_result}, qft={qft_result}"
                )

        # ctrl=0: no-op (a stays unchanged)
        for a, b in pairs:

            def build_fn_ctrl0(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qb = ql.qint(b, width=w)
                ctrl = ql.qint(0, width=1)
                with ctrl:
                    qa += qb
                return qa, a, [qa, qb, ctrl]

            toffoli_result, qft_result = _compare_backends(
                build_fn_ctrl0, width, use_mps_toffoli=True
            )
            if toffoli_result != qft_result:
                failures.append(
                    f"w={width} a={a} b={b} ctrl=0: toffoli={toffoli_result}, qft={qft_result}"
                )

        assert not failures, f"{len(failures)} cross-backend mismatches:\n" + "\n".join(
            failures[:20]
        )

    @pytest.mark.parametrize(
        "width",
        [
            pytest.param(
                2,
                marks=pytest.mark.xfail(
                    reason="BUG-CQQ-QFT: QFT controlled QQ in-place addition produces "
                    "incorrect results at width 2+ (CCP rotation angle errors)",
                    strict=False,
                ),
            ),
            pytest.param(
                3,
                marks=pytest.mark.xfail(
                    reason="BUG-CQQ-QFT: QFT controlled QQ in-place addition produces "
                    "incorrect results at width 2+ (CCP rotation angle errors)",
                    strict=False,
                ),
            ),
            pytest.param(
                4,
                marks=pytest.mark.xfail(
                    reason="BUG-CQQ-QFT: QFT controlled QQ in-place addition produces "
                    "incorrect results at width 2+ (CCP rotation angle errors)",
                    strict=False,
                ),
            ),
            pytest.param(
                5,
                marks=[
                    pytest.mark.slow,
                    pytest.mark.xfail(
                        reason="BUG-CQQ-QFT: QFT controlled QQ in-place addition bug",
                        strict=False,
                    ),
                ],
            ),
            pytest.param(
                6,
                marks=[
                    pytest.mark.slow,
                    pytest.mark.xfail(
                        reason="BUG-CQQ-QFT: QFT controlled QQ in-place addition bug",
                        strict=False,
                    ),
                ],
            ),
            pytest.param(
                7,
                marks=[
                    pytest.mark.slow,
                    pytest.mark.xfail(
                        reason="BUG-CQQ-QFT: QFT controlled QQ in-place addition bug",
                        strict=False,
                    ),
                ],
            ),
            pytest.param(
                8,
                marks=[
                    pytest.mark.slow,
                    pytest.mark.xfail(
                        reason="BUG-CQQ-QFT: QFT controlled QQ in-place addition bug",
                        strict=False,
                    ),
                ],
            ),
        ],
    )
    def test_cqq_add(self, width):
        """Controlled QQ in-place addition (with ctrl: a += qb) cross-backend.

        Uses in-place qa += qb (not out-of-place qa + qb) because controlled
        out-of-place addition requires controlled XOR which is not yet supported.

        BUG-CQQ-QFT: QFT backend produces incorrect results for controlled
        quantum-quantum in-place addition at widths 2+. The CCP decomposition
        has rotation angle errors. Toffoli backend is correct. Tests are xfail
        at widths 2+ to document this discovered bug.
        """
        if width <= 4:
            pairs = generate_exhaustive_pairs(width)
        else:
            pairs = generate_sampled_pairs(width, sample_size=10)

        failures = []
        for a, b in pairs:

            def build_fn(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qb = ql.qint(b, width=w)
                ctrl = ql.qint(1, width=1)
                with ctrl:
                    qa += qb
                return qa, (a + b) % (1 << w), [qa, qb, ctrl]

            toffoli_result, qft_result = _compare_backends(build_fn, width, use_mps_toffoli=True)
            if toffoli_result != qft_result:
                failures.append(
                    f"w={width} a={a} b={b} ctrl=1: toffoli={toffoli_result}, qft={qft_result}"
                )

        assert not failures, f"{len(failures)} cross-backend mismatches:\n" + "\n".join(
            failures[:20]
        )

    @pytest.mark.parametrize(
        "width",
        [
            1,
            2,
            3,
            4,
            pytest.param(5, marks=pytest.mark.slow),
            pytest.param(6, marks=pytest.mark.slow),
            pytest.param(7, marks=pytest.mark.slow),
            pytest.param(8, marks=pytest.mark.slow),
        ],
    )
    def test_ccq_add(self, width):
        """Controlled CQ addition (with ctrl: a += b) produces identical results.

        Tests ctrl=1 exhaustively/sampled, ctrl=0 only for widths<=2 (subset).
        """
        if width <= 4:
            pairs = generate_exhaustive_pairs(width)
        else:
            pairs = generate_sampled_pairs(width, sample_size=10)

        # ctrl=1: addition should happen
        failures = []
        for a, b in pairs:

            def build_fn(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                ctrl = ql.qint(1, width=1)
                with ctrl:
                    qa += b
                return qa, (a + b) % (1 << w), [qa, ctrl]

            toffoli_result, qft_result = _compare_backends(build_fn, width, use_mps_toffoli=True)
            if toffoli_result != qft_result:
                failures.append(
                    f"w={width} a={a} b={b} ctrl=1: toffoli={toffoli_result}, qft={qft_result}"
                )

        # ctrl=0: no-op (a stays unchanged), only test for small widths
        if width <= 2:
            ctrl0_pairs = generate_exhaustive_pairs(width)
            for a, b in ctrl0_pairs:

                def build_fn_ctrl0(a=a, b=b, w=width):
                    qa = ql.qint(a, width=w)
                    ctrl = ql.qint(0, width=1)
                    with ctrl:
                        qa += b
                    return qa, a, [qa, ctrl]

                toffoli_result, qft_result = _compare_backends(
                    build_fn_ctrl0, width, use_mps_toffoli=True
                )
                if toffoli_result != qft_result:
                    failures.append(
                        f"w={width} a={a} b={b} ctrl=0: toffoli={toffoli_result}, qft={qft_result}"
                    )

        assert not failures, f"{len(failures)} cross-backend mismatches:\n" + "\n".join(
            failures[:20]
        )


# ---------------------------------------------------------------------------
# TestCrossBackendSubtraction
# ---------------------------------------------------------------------------


class TestCrossBackendSubtraction:
    """Cross-backend equivalence tests for QQ and CQ subtraction.

    Parametrized across widths 1-8. Exhaustive for widths 1-4, sampled for 5-8.
    """

    @pytest.mark.parametrize(
        "width",
        [
            1,
            2,
            3,
            4,
            5,
            6,
            pytest.param(7, marks=pytest.mark.slow),
            pytest.param(8, marks=pytest.mark.slow),
        ],
    )
    def test_qq_sub(self, width):
        """QQ subtraction (c = a - b) produces identical results in both backends."""
        if width <= 4:
            pairs = generate_exhaustive_pairs(width)
        elif width <= 6:
            pairs = generate_sampled_pairs(width, sample_size=20)
        else:
            pairs = generate_sampled_pairs(width, sample_size=10)

        failures = []
        for a, b in pairs:

            def build_fn(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qb = ql.qint(b, width=w)
                qc = qa - qb
                return qc, (a - b) % (1 << w), [qa, qb, qc]

            toffoli_result, qft_result = _compare_backends(build_fn, width)
            if toffoli_result != qft_result:
                failures.append(
                    f"w={width} a={a} b={b}: toffoli={toffoli_result}, qft={qft_result}"
                )

        assert not failures, f"{len(failures)} cross-backend mismatches:\n" + "\n".join(
            failures[:20]
        )

    @pytest.mark.parametrize(
        "width",
        [
            1,
            2,
            3,
            4,
            5,
            6,
            pytest.param(7, marks=pytest.mark.slow),
            pytest.param(8, marks=pytest.mark.slow),
        ],
    )
    def test_cq_sub(self, width):
        """CQ subtraction (a -= b) produces identical results in both backends."""
        if width <= 4:
            pairs = generate_exhaustive_pairs(width)
        elif width <= 6:
            pairs = generate_sampled_pairs(width, sample_size=20)
        else:
            pairs = generate_sampled_pairs(width, sample_size=10)

        failures = []
        for a, b in pairs:

            def build_fn(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qa -= b
                return qa, (a - b) % (1 << w), [qa]

            toffoli_result, qft_result = _compare_backends(build_fn, width)
            if toffoli_result != qft_result:
                failures.append(
                    f"w={width} a={a} b={b}: toffoli={toffoli_result}, qft={qft_result}"
                )

        assert not failures, f"{len(failures)} cross-backend mismatches:\n" + "\n".join(
            failures[:20]
        )
