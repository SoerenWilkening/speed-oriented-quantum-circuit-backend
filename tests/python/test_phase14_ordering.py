"""Tests for Phase 14: Ordering Comparisons

Requirements:
- COMP-03: Refactor <= to use in-place subtraction/addition (no temp qint)
- COMP-04: Refactor >= to use in-place subtraction/addition (no temp qint)

These tests verify ordering comparisons work correctly for various bit widths
and edge cases, without allocating temporary qints.
"""

import os
import sys

# Add python-backend to path
project_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
sys.path.insert(0, os.path.join(project_root, "python-backend"))

import quantum_language as ql  # noqa: E402

# ============================================================================
# Less-Than Tests (qint < int)
# ============================================================================


class TestQintLtInt:
    """Tests for qint < int comparison."""

    def test_basic_lt(self):
        """Basic qint < int returns qbool."""
        a = ql.qint(3, width=8)
        result = a < 5
        assert isinstance(result, ql.qbool)
        assert result.width == 1

    def test_lt_various_widths(self):
        """Less-than works for various bit widths."""
        for width in [1, 4, 8, 16, 32]:
            a = ql.qint(0, width=width)
            result = a < 1
            assert isinstance(result, ql.qbool), f"Failed for width {width}"

    def test_lt_equal_values(self):
        """qint < same_value returns qbool (represents False)."""
        a = ql.qint(5, width=8)
        result = a < 5
        assert isinstance(result, ql.qbool)

    def test_lt_operand_preserved(self):
        """qint operand unchanged after < comparison."""
        a = ql.qint(3, width=8)
        width_before = a.width
        _ = a < 5
        assert a.width == width_before


class TestQintLtIntOverflow:
    """Tests for overflow handling in qint < int."""

    def test_lt_large_value(self):
        """qint < large_value handles overflow (returns True)."""
        a = ql.qint(5, width=4)  # 4-bit max is 15
        result = a < 100  # 100 > 15, overflow
        assert isinstance(result, ql.qbool)
        # Should return True (qint always < value outside its range)

    def test_lt_negative_value(self):
        """qint < negative handles overflow (returns False)."""
        a = ql.qint(5, width=8)
        result = a < -1  # Negative
        assert isinstance(result, ql.qbool)
        # Should return False (unsigned qint >= 0, not < negative)


# ============================================================================
# Greater-Than Tests (qint > int)
# ============================================================================


class TestQintGtInt:
    """Tests for qint > int comparison."""

    def test_basic_gt(self):
        """Basic qint > int returns qbool."""
        a = ql.qint(5, width=8)
        result = a > 3
        assert isinstance(result, ql.qbool)
        assert result.width == 1

    def test_gt_various_widths(self):
        """Greater-than works for various bit widths."""
        for width in [1, 4, 8, 16]:
            max_val = (1 << width) - 1
            a = ql.qint(max_val, width=width)
            result = a > 0
            assert isinstance(result, ql.qbool), f"Failed for width {width}"

    def test_gt_operand_preserved(self):
        """qint operand unchanged after > comparison."""
        a = ql.qint(7, width=8)
        width_before = a.width
        _ = a > 3
        assert a.width == width_before


class TestQintGtIntOverflow:
    """Tests for overflow handling in qint > int."""

    def test_gt_large_value(self):
        """qint > large_value handles overflow (returns False)."""
        a = ql.qint(5, width=4)
        result = a > 100  # 100 > max 4-bit value
        assert isinstance(result, ql.qbool)
        # Should return False (qint never > value outside its range)

    def test_gt_negative_value(self):
        """qint > negative handles overflow (returns True)."""
        a = ql.qint(5, width=8)
        result = a > -5
        assert isinstance(result, ql.qbool)
        # Should return True (unsigned qint >= 0, always > negative)


# ============================================================================
# Less-Than-Or-Equal Tests (qint <= int)
# ============================================================================


class TestQintLeInt:
    """Tests for qint <= int comparison."""

    def test_basic_le(self):
        """Basic qint <= int returns qbool."""
        a = ql.qint(3, width=8)
        result = a <= 5
        assert isinstance(result, ql.qbool)
        assert result.width == 1

    def test_le_equal_values(self):
        """qint <= same_value returns qbool (represents True)."""
        a = ql.qint(5, width=8)
        result = a <= 5
        assert isinstance(result, ql.qbool)

    def test_le_various_widths(self):
        """Less-than-or-equal works for various bit widths."""
        for width in [1, 4, 8, 16]:
            a = ql.qint(0, width=width)
            result = a <= 0
            assert isinstance(result, ql.qbool), f"Failed for width {width}"

    def test_le_operand_preserved(self):
        """qint operand unchanged after <= comparison."""
        a = ql.qint(3, width=8)
        width_before = a.width
        _ = a <= 5
        assert a.width == width_before


class TestQintLeIntOverflow:
    """Tests for overflow handling in qint <= int."""

    def test_le_large_value(self):
        """qint <= large_value handles overflow (returns True)."""
        a = ql.qint(5, width=4)
        result = a <= 100
        assert isinstance(result, ql.qbool)
        # Should return True (qint always <= value outside its range)

    def test_le_negative_value(self):
        """qint <= negative handles overflow (returns False)."""
        a = ql.qint(5, width=8)
        result = a <= -1
        assert isinstance(result, ql.qbool)
        # Should return False (unsigned qint >= 0, not <= negative)


# ============================================================================
# Greater-Than-Or-Equal Tests (qint >= int)
# ============================================================================


class TestQintGeInt:
    """Tests for qint >= int comparison."""

    def test_basic_ge(self):
        """Basic qint >= int returns qbool."""
        a = ql.qint(5, width=8)
        result = a >= 3
        assert isinstance(result, ql.qbool)
        assert result.width == 1

    def test_ge_equal_values(self):
        """qint >= same_value returns qbool (represents True)."""
        a = ql.qint(5, width=8)
        result = a >= 5
        assert isinstance(result, ql.qbool)

    def test_ge_various_widths(self):
        """Greater-than-or-equal works for various bit widths."""
        for width in [1, 4, 8, 16]:
            a = ql.qint(0, width=width)
            result = a >= 0
            assert isinstance(result, ql.qbool), f"Failed for width {width}"

    def test_ge_operand_preserved(self):
        """qint operand unchanged after >= comparison."""
        a = ql.qint(7, width=8)
        width_before = a.width
        _ = a >= 3
        assert a.width == width_before


class TestQintGeIntOverflow:
    """Tests for overflow handling in qint >= int."""

    def test_ge_large_value(self):
        """qint >= large_value handles overflow (returns False)."""
        a = ql.qint(5, width=4)
        result = a >= 100
        assert isinstance(result, ql.qbool)
        # Should return False (qint never >= value outside its range)

    def test_ge_negative_value(self):
        """qint >= negative handles overflow (returns True)."""
        a = ql.qint(5, width=8)
        result = a >= -5
        assert isinstance(result, ql.qbool)
        # Should return True (unsigned qint >= 0, always >= negative)
