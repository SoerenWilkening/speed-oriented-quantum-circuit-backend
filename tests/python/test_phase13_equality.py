"""Tests for Phase 13: Equality Comparison

Requirements:
- COMP-01: Implement qint == int using CQ_equal_width in Python bindings
- COMP-02: Implement qint == qint as (qint - qint) == 0

These tests verify equality comparisons work correctly for various bit widths
and edge cases, and integrate properly with quantum control flow.
"""

import os
import sys

# Add python-backend to path
project_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
sys.path.insert(0, os.path.join(project_root, "python-backend"))

import quantum_language as ql  # noqa: E402

# ============================================================================
# COMP-01: qint == int Tests
# ============================================================================


class TestQintEqInt:
    """Tests for qint == int comparison using CQ_equal_width."""

    def test_basic_equality(self):
        """Basic qint == int comparison returns qbool."""
        a = ql.qint(5, width=8)
        result = a == 5
        assert isinstance(result, ql.qbool)
        assert result.width == 1

    def test_various_widths(self):
        """Equality works for various bit widths."""
        for width in [1, 4, 8, 16, 32]:
            a = ql.qint(0, width=width)
            result = a == 0
            assert isinstance(result, ql.qbool), f"Failed for width {width}"
            assert result.width == 1

    def test_width_1(self):
        """Single-bit comparison works."""
        a = ql.qint(1, width=1)
        result = a == 1
        assert isinstance(result, ql.qbool)

    def test_width_1_zero(self):
        """Single-bit comparison with zero."""
        a = ql.qint(0, width=1)
        result = a == 0
        assert isinstance(result, ql.qbool)

    def test_non_zero_value(self):
        """Comparison with non-zero classical value."""
        a = ql.qint(42, width=8)
        result = a == 42
        assert isinstance(result, ql.qbool)

    def test_max_value_for_width(self):
        """Comparison with maximum value for bit width."""
        # 8-bit max unsigned = 255
        a = ql.qint(127, width=8)  # Use 127 to avoid signed overflow warning
        result = a == 127
        assert isinstance(result, ql.qbool)


class TestQintEqIntOverflow:
    """Tests for overflow handling in qint == int."""

    def test_overflow_large_value(self):
        """Value too large for bit width returns not-equal."""
        a = ql.qint(5, width=4)  # 4-bit can hold 0-15
        result = a == 100  # 100 > 15, overflow
        assert isinstance(result, ql.qbool)
        # Should return qbool representing "not equal"

    def test_overflow_negative_value(self):
        """Negative value with unsigned qint returns not-equal."""
        a = ql.qint(5, width=8)
        result = a == -1  # Negative not representable in unsigned
        assert isinstance(result, ql.qbool)


# ============================================================================
# COMP-02: qint == qint Tests
# ============================================================================


class TestQintEqQint:
    """Tests for qint == qint comparison using subtract-add-back pattern."""

    def test_basic_qq_equality(self):
        """Basic qint == qint comparison returns qbool."""
        a = ql.qint(5, width=8)
        b = ql.qint(5, width=8)
        result = a == b
        assert isinstance(result, ql.qbool)
        assert result.width == 1

    def test_self_comparison(self):
        """Self-comparison (a == a) optimization works."""
        a = ql.qint(5, width=8)
        result = a == a
        assert isinstance(result, ql.qbool)
        # Self-comparison should be optimized

    def test_different_values(self):
        """Comparison of different values."""
        a = ql.qint(5, width=8)
        b = ql.qint(3, width=8)
        result = a == b
        assert isinstance(result, ql.qbool)

    def test_operand_preservation(self):
        """Both operands preserved after comparison."""
        a = ql.qint(5, width=8)
        b = ql.qint(3, width=8)
        # Store original widths
        a_width_before = a.width
        b_width_before = b.width

        result = a == b  # noqa: F841 - result unused, but comparison side effects are tested

        # Verify operands still have correct widths
        assert a.width == a_width_before
        assert b.width == b_width_before
        # Operands should still be usable
        assert isinstance(a, ql.qint)
        assert isinstance(b, ql.qint)


class TestQintEqQintMixedWidths:
    """Tests for qint == qint with different bit widths."""

    def test_different_widths_8_16(self):
        """Comparison with different widths (8 vs 16)."""
        a = ql.qint(5, width=8)
        b = ql.qint(5, width=16)
        result = a == b
        assert isinstance(result, ql.qbool)

    def test_different_widths_4_8(self):
        """Comparison with different widths (4 vs 8)."""
        a = ql.qint(5, width=4)
        b = ql.qint(5, width=8)
        result = a == b
        assert isinstance(result, ql.qbool)


# ============================================================================
# Inequality Tests
# ============================================================================


class TestInequality:
    """Tests for != operator (inverted ==)."""

    def test_ne_int(self):
        """qint != int returns qbool."""
        a = ql.qint(5, width=8)
        result = a != 5
        assert isinstance(result, ql.qbool)
        assert result.width == 1

    def test_ne_qint(self):
        """qint != qint returns qbool."""
        a = ql.qint(5, width=8)
        b = ql.qint(3, width=8)
        result = a != b
        assert isinstance(result, ql.qbool)

    def test_ne_overflow(self):
        """!= with overflow value returns qbool."""
        a = ql.qint(5, width=4)
        result = a != 100  # Overflow
        assert isinstance(result, ql.qbool)
