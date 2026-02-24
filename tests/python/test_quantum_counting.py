"""Phase 90: Quantum Counting Tests.

Tests covering:
- CNT-01: ql.count_solutions() returns integer solution count for known oracles
- CNT-02: CountResult exposes .count, .estimate, .count_interval, .search_space, .num_oracle_calls
- CNT-03: Verified against known-M oracles (M=1, M=2, M=3) via Qiskit simulation

Three test classes:
- TestCountResultUnit: Unit tests for CountResult (no Qiskit)
- TestQuantumCountingEndToEnd: Integration tests with Qiskit simulation

Note: All integration tests use 2-3 bit registers to stay well under
the 17-qubit simulator limit (project constraint).
"""

import pytest

import quantum_language as ql
from quantum_language.amplitude_estimation import AmplitudeEstimationResult
from quantum_language.quantum_counting import CountResult


# ---------------------------------------------------------------------------
# Group 1: Unit Tests for CountResult (no Qiskit needed)
# ---------------------------------------------------------------------------
class TestCountResultUnit:
    """Unit tests for CountResult int-like behavior."""

    def _make_result(self, estimate=0.125, num_oracle_calls=100, ci=(0.05, 0.20), N=8):
        """Helper to create a CountResult from a mock AmplitudeEstimationResult."""
        ae = AmplitudeEstimationResult(estimate, num_oracle_calls, confidence_interval=ci)
        return CountResult(ae, search_space=N)

    def test_count_property(self):
        """Create CountResult with amplitude 0.125, N=8. Assert .count == 1."""
        result = self._make_result(estimate=0.125, N=8)
        assert result.count == 1  # round(8 * 0.125) = round(1.0) = 1

    def test_estimate_property(self):
        """Assert .estimate == 8 * 0.125 = 1.0."""
        result = self._make_result(estimate=0.125, N=8)
        assert result.estimate == pytest.approx(1.0)

    def test_search_space_property(self):
        """Assert .search_space == 8."""
        result = self._make_result(N=8)
        assert result.search_space == 8

    def test_num_oracle_calls_property(self):
        """Assert .num_oracle_calls matches input."""
        result = self._make_result(num_oracle_calls=42)
        assert result.num_oracle_calls == 42
        assert isinstance(result.num_oracle_calls, int)

    def test_count_interval_property(self):
        """CI=(0.05, 0.20), N=8 => interval=(floor(0.4), ceil(1.6)) = (0, 2)."""
        result = self._make_result(estimate=0.125, ci=(0.05, 0.20), N=8)
        assert result.count_interval == (0, 2)

    def test_int_conversion(self):
        """int(result) returns count."""
        result = self._make_result(estimate=0.125, N=8)
        assert int(result) == 1

    def test_float_conversion(self):
        """float(result) returns estimate (raw unrounded count)."""
        result = self._make_result(estimate=0.125, N=8)
        assert float(result) == pytest.approx(1.0)

    def test_eq_int(self):
        """result == 1 is True, result == 2 is False."""
        result = self._make_result(estimate=0.125, N=8)
        assert result == 1
        assert not (result == 2)

    def test_eq_float_integer_value(self):
        """result == 1.0 works (int(1.0) == 1)."""
        result = self._make_result(estimate=0.125, N=8)
        assert result == 1.0

    def test_arithmetic_add(self):
        """result + 2 == 3 and 2 + result == 3."""
        result = self._make_result(estimate=0.125, N=8)
        assert result + 2 == 3
        assert 2 + result == 3

    def test_arithmetic_sub(self):
        """result - 1 == 0."""
        result = self._make_result(estimate=0.125, N=8)
        assert result - 1 == 0

    def test_arithmetic_mul(self):
        """result * 3 == 3 and 3 * result == 3."""
        result = self._make_result(estimate=0.125, N=8)
        assert result * 3 == 3
        assert 3 * result == 3

    def test_arithmetic_div(self):
        """result / 1 == 1.0."""
        result = self._make_result(estimate=0.125, N=8)
        assert result / 1 == 1.0

    def test_comparison_lt_gt(self):
        """result < 5 and result > 0."""
        result = self._make_result(estimate=0.125, N=8)
        assert result < 5
        assert result > 0

    def test_comparison_le_ge(self):
        """result <= 1 and result >= 1."""
        result = self._make_result(estimate=0.125, N=8)
        assert result <= 1
        assert result >= 1

    def test_bool_nonzero(self):
        """bool(result) is True for count=1."""
        result = self._make_result(estimate=0.125, N=8)
        assert bool(result) is True

    def test_bool_zero(self):
        """bool(result) is False for count=0."""
        result = self._make_result(estimate=0.0, N=8)
        assert bool(result) is False
        assert result.count == 0

    def test_neg(self):
        """-result returns -count."""
        result = self._make_result(estimate=0.125, N=8)
        assert -result == -1

    def test_abs(self):
        """abs(result) returns count."""
        result = self._make_result(estimate=0.125, N=8)
        assert abs(result) == 1

    def test_repr(self):
        """repr contains 'CountResult' and 'count=1'."""
        result = self._make_result(estimate=0.125, N=8)
        r = repr(result)
        assert "CountResult" in r
        assert "count=1" in r

    def test_zero_count_valid(self):
        """Zero is a valid count, no special case or sentinel."""
        result = self._make_result(estimate=0.001, N=8)
        # round(8 * 0.001) = round(0.008) = 0
        assert result.count == 0

    def test_count_clamped_to_N(self):
        """Amplitude slightly over 1.0 clamps count to N."""
        ae = AmplitudeEstimationResult(1.05, 100, confidence_interval=(0.95, 1.0))
        result = CountResult(ae, search_space=8)
        assert result.count == 8  # clamped to N

    def test_count_clamped_to_zero(self):
        """Negative amplitude clamps count to 0."""
        ae = AmplitudeEstimationResult(-0.05, 100, confidence_interval=(0.0, 0.1))
        result = CountResult(ae, search_space=8)
        assert result.count == 0  # clamped to 0

    def test_count_interval_none_ci(self):
        """When confidence_interval is None, count_interval == (count, count)."""
        ae = AmplitudeEstimationResult(0.125, 100, confidence_interval=None)
        result = CountResult(ae, search_space=8)
        assert result.count_interval == (1, 1)


# ---------------------------------------------------------------------------
# Group 2: End-to-End Integration Tests with Qiskit Simulation
# ---------------------------------------------------------------------------
class TestQuantumCountingEndToEnd:
    """Integration tests verifying ql.count_solutions() with Qiskit simulation.

    CRITICAL CONSTRAINTS (from project memory):
    - Max 17 qubits for Qiskit simulation -- use 2-3 bit search registers only
    - Max 4 threads -- already enforced by sim_backend
    """

    def test_count_single_solution_m1(self):
        """M=1: lambda x == 5 in 3-bit space. Expected count = 1.

        CNT-03: Quantum counting verified for M=1 known-M oracle.
        Search space N=8, one solution (x=5), true probability = 1/8.
        """
        result = ql.count_solutions(lambda x: x == 5, width=3, epsilon=0.05)
        assert result.count == 1, f"Expected count=1, got {result.count}"
        assert result.search_space == 8
        assert result.num_oracle_calls > 0
        assert result.count_interval[0] <= 1 <= result.count_interval[1]

    def test_count_two_solutions_m2(self):
        """M=2: lambda x > 5 in 3-bit space. x in {6, 7}. Expected count = 2.

        CNT-03: Quantum counting verified for M=2 known-M oracle.
        Search space N=8, two solutions, true probability = 2/8 = 0.25.
        """
        result = ql.count_solutions(lambda x: x > 5, width=3, epsilon=0.05)
        assert result.count == 2, f"Expected count=2, got {result.count}"
        assert result.search_space == 8

    def test_count_three_solutions_m3(self):
        """M=3: lambda x > 4 in 3-bit space. x in {5, 6, 7}. Expected count = 3.

        CNT-03: Quantum counting verified for M=3 known-M oracle.
        Search space N=8, three solutions, true probability = 3/8 = 0.375.
        """
        result = ql.count_solutions(lambda x: x > 4, width=3, epsilon=0.05)
        assert result.count == 3, f"Expected count=3, got {result.count}"
        assert result.search_space == 8

    def test_count_result_is_int_like(self):
        """Result from actual estimation supports int-like operations.

        Confirms CountResult integration in a real scenario:
        int() conversion, arithmetic, comparison all work on a real result.
        """
        result = ql.count_solutions(lambda x: x == 1, width=2, epsilon=0.05)
        # Int conversion
        n = int(result)
        assert isinstance(n, int)
        # Arithmetic
        s = result + 0
        assert isinstance(s, int)
        # Comparison
        assert result >= 0

    def test_two_bit_register(self):
        """M=1: lambda x == 1 in 2-bit space. Expected count = 1, search_space = 4.

        Tests minimal register size. 2-bit register stays well under
        17-qubit limit even with ancilla overhead.
        """
        result = ql.count_solutions(lambda x: x == 1, width=2, epsilon=0.05)
        assert result.count == 1, f"Expected count=1, got {result.count}"
        assert result.search_space == 4

    def test_all_solutions(self):
        """M=4: all values in 2-bit space match. Expected count = 4.

        Lambda x >= 0 is always true for unsigned 2-bit integers (0-3).
        True probability = 4/4 = 1.0.
        """
        result = ql.count_solutions(lambda x: x >= 0, width=2, epsilon=0.05)
        assert result.count == 4, f"Expected count=4, got {result.count}"
        assert result.search_space == 4
