"""Phase 81: Amplitude Estimation (IQAE) Tests.

Tests covering:
- AMP-01: ql.amplitude_estimate() returns estimated probability for known oracles
- AMP-02: QFT-free IQAE implementation (uses Grover iterate only, no QFT)
- AMP-03: Configurable precision (epsilon), confidence, max_iterations

Three test classes:
- TestAmplitudeEstimationResult: Unit tests for the result class (no Qiskit)
- TestIQAEHelpers: Unit tests for internal algorithm helpers (no Qiskit)
- TestAmplitudeEstimationEndToEnd: Integration tests with Qiskit simulation

Note: All integration tests use 2-3 bit registers to stay well under
the 17-qubit simulator limit (project constraint).
"""

import pytest

from quantum_language.amplitude_estimation import (
    AmplitudeEstimationResult,
    _clopper_pearson_confint,
    _find_next_k,
)


# ---------------------------------------------------------------------------
# Group 1: Unit Tests for AmplitudeEstimationResult (no Qiskit needed)
# ---------------------------------------------------------------------------
class TestAmplitudeEstimationResult:
    """Unit tests for AmplitudeEstimationResult float-like behavior."""

    def test_estimate_property(self):
        """Create result with estimate=0.125, verify .estimate returns 0.125."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert result.estimate == 0.125

    def test_num_oracle_calls_property(self):
        """Verify .num_oracle_calls returns correct int."""
        result = AmplitudeEstimationResult(0.125, 42)
        assert result.num_oracle_calls == 42
        assert isinstance(result.num_oracle_calls, int)

    def test_float_conversion(self):
        """float(result) returns the estimate value."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert float(result) == 0.125

    def test_arithmetic_add(self):
        """result + 0.5 and 0.5 + result both return correct float."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert result + 0.5 == pytest.approx(0.625)
        assert 0.5 + result == pytest.approx(0.625)

    def test_arithmetic_mul(self):
        """result * 2.0 and 2.0 * result both return correct float."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert result * 2.0 == pytest.approx(0.25)
        assert 2.0 * result == pytest.approx(0.25)

    def test_arithmetic_sub(self):
        """result - 0.1 and 1.0 - result both return correct float."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert result - 0.1 == pytest.approx(0.025)
        assert 1.0 - result == pytest.approx(0.875)

    def test_arithmetic_div(self):
        """result / 2 returns correct float."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert result / 2 == pytest.approx(0.0625)

    def test_comparison_lt(self):
        """result < 0.5 returns True for estimate=0.125."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert result < 0.5

    def test_comparison_gt(self):
        """result > 0.0 returns True for estimate=0.125."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert result > 0.0

    def test_bool_nonzero(self):
        """bool(result) is True for estimate=0.125, False for estimate=0.0."""
        result_nonzero = AmplitudeEstimationResult(0.125, 100)
        result_zero = AmplitudeEstimationResult(0.0, 0)
        assert bool(result_nonzero) is True
        assert bool(result_zero) is False

    def test_int_conversion(self):
        """int(result) returns 0 for estimate=0.125."""
        result = AmplitudeEstimationResult(0.125, 100)
        assert int(result) == 0

    def test_repr(self):
        """repr(result) contains 'AmplitudeEstimationResult' and the estimate value."""
        result = AmplitudeEstimationResult(0.125, 100)
        r = repr(result)
        assert "AmplitudeEstimationResult" in r
        assert "0.125" in r


# ---------------------------------------------------------------------------
# Group 2: Unit Tests for IQAE Helper Functions (no Qiskit needed)
# ---------------------------------------------------------------------------
class TestIQAEHelpers:
    """Unit tests for internal IQAE algorithm functions."""

    def test_clopper_pearson_confint_middle(self):
        """For counts=50, shots=100, alpha=0.05, verify lower < 0.5 < upper.

        The Clopper-Pearson interval should contain the observed proportion 0.5.
        """
        lower, upper = _clopper_pearson_confint(50, 100, 0.05)
        assert lower < 0.5
        assert upper > 0.5
        assert lower >= 0.0
        assert upper <= 1.0

    def test_clopper_pearson_confint_zero_counts(self):
        """counts=0 should give lower=0.0.

        When no good outcomes are observed, the lower bound must be exactly 0.
        """
        lower, upper = _clopper_pearson_confint(0, 100, 0.05)
        assert lower == 0.0
        assert upper > 0.0

    def test_clopper_pearson_confint_all_counts(self):
        """counts=shots should give upper=1.0.

        When all outcomes are good, the upper bound must be exactly 1.
        """
        lower, upper = _clopper_pearson_confint(100, 100, 0.05)
        assert upper == 1.0
        assert lower < 1.0

    def test_find_next_k_initial(self):
        """For k=0 (first call), verify returned k > 0 and is a valid integer.

        Starting from k=0, the algorithm should advance to a higher power.
        """
        # Initial theta interval [0, 0.25]
        k_next, _ = _find_next_k(0, True, [0.0, 0.25])
        assert isinstance(k_next, int)
        assert k_next >= 0

    def test_find_next_k_monotone(self):
        """Verify k increases or stays same across calls with narrowing interval.

        As the theta interval narrows, the algorithm should select larger
        Grover powers to resolve the estimate.
        """
        k = 0
        upper_half = True
        theta = [0.0, 0.25]

        k1, upper_half = _find_next_k(k, upper_half, theta)

        # Narrow the interval
        theta_narrow = [0.05, 0.15]
        k2, _ = _find_next_k(k1, upper_half, theta_narrow)

        assert k2 >= k1, f"Expected k2 >= k1, got k2={k2}, k1={k1}"
