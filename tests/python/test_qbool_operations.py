"""Characterization tests for qbool and logic operations.

These tests capture the CURRENT BEHAVIOR of qbool and logic operations as of 2026-01-26.
They serve as golden masters to detect regressions during refactoring.

Tests verify that operations complete successfully and produce expected types,
not that they produce specific gate sequences.
"""

import os
import sys

# Add python-backend to path
project_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
sys.path.insert(0, os.path.join(project_root, "python-backend"))

import quantum_language as ql  # noqa: E402

# ============================================================================
# qbool Creation Tests
# ============================================================================


def test_qbool_creation_default():
    """Test qbool creation with default parameters."""
    a = ql.qbool()
    assert a is not None
    assert isinstance(a, ql.qbool)
    # qbool is subclass of qint
    assert isinstance(a, ql.qint)


def test_qbool_creation_with_value():
    """Test qbool creation with specified boolean value."""
    a = ql.qbool(value=True)
    assert a is not None
    assert isinstance(a, ql.qbool)


def test_qbool_from_qint_bit():
    """Test accessing qint bit returns qbool-like object."""
    a = ql.qint(value=5, bits=8)

    # Access individual bit (Python indexing)
    # Based on code: qint[INTEGERSIZE - self.bits] accesses first bit
    b = a[0]

    # Verify result exists and is qbool type
    assert b is not None
    assert isinstance(b, ql.qbool)


# ============================================================================
# Logic Operations - AND
# ============================================================================


def test_qbool_and_qbool():
    """Test AND operation between two qbools."""
    a = ql.qbool()
    b = ql.qbool()

    # Perform AND
    c = a & b

    # Verify result is qbool
    assert c is not None
    assert isinstance(c, ql.qbool)


def test_qint_and_qint():
    """Test AND operation between two qints."""
    a = ql.qint(value=5, bits=8)
    b = ql.qint(value=3, bits=8)

    # Perform AND (returns qbool based on code)
    c = a & b

    # Verify result is qbool
    assert c is not None
    assert isinstance(c, ql.qbool)


# ============================================================================
# Logic Operations - OR
# ============================================================================


def test_qbool_or_qbool():
    """Test OR operation between two qbools."""
    a = ql.qbool()
    b = ql.qbool()

    # Perform OR
    c = a | b

    # Verify result is qbool
    assert c is not None
    assert isinstance(c, ql.qbool)


def test_qint_or_qint():
    """Test OR operation between two qints."""
    a = ql.qint(value=5, bits=8)
    b = ql.qint(value=3, bits=8)

    # Perform OR
    c = a | b

    # Verify result is qbool
    assert c is not None
    assert isinstance(c, ql.qbool)


# ============================================================================
# Logic Operations - NOT (Invert)
# ============================================================================


def test_qbool_invert():
    """Test NOT operation on qbool."""
    a = ql.qbool()

    # Perform NOT
    b = ~a

    # Verify result is qbool (modifies in place, returns self)
    assert b is not None
    assert isinstance(b, ql.qbool)


def test_qint_invert():
    """Test NOT operation on qint."""
    a = ql.qint(value=5, bits=8)

    # Perform NOT
    b = ~a

    # Verify result is qint (modifies in place)
    assert b is not None
    assert isinstance(b, ql.qint)


# ============================================================================
# Context Manager (Controlled Operations)
# ============================================================================


def test_context_manager_with_qbool():
    """Test context manager (with statement) for controlled operations."""
    a = ql.qbool()
    b = ql.qint(value=0, bits=4)

    # Use qbool as control for operation
    with a:
        b += 1

    # Verify operation completed without error
    assert b is not None
    assert isinstance(b, ql.qint)


def test_context_manager_with_qint():
    """Test context manager with qint (uses first bit as control)."""
    a = ql.qint(value=5, bits=8)
    b = ql.qint(value=0, bits=4)

    # Use qint as control
    with a:
        b += 1

    # Verify operation completed
    assert b is not None


def test_nested_context_manager():
    """Test nested context managers for multi-controlled operations."""
    a = ql.qbool()
    c = ql.qbool()
    b = ql.qint(value=0, bits=4)

    # Nested control (AND of two qbools)
    with a & c:
        b += 1

    # Verify operation completed
    assert b is not None
    assert isinstance(b, ql.qint)


def test_context_manager_multiple_operations():
    """Test multiple operations within controlled context."""
    a = ql.qbool()
    b = ql.qint(value=0, bits=4)

    with a:
        b += 1
        b += 2
        b -= 1

    # Verify all operations completed
    assert b is not None


# ============================================================================
# Array Creation
# ============================================================================


def test_array_1d_qint():
    """Test creating 1D array of qints."""
    arr = ql.array(3, dtype=ql.qint)

    # Verify array creation
    assert arr is not None
    assert isinstance(arr, list)
    assert len(arr) == 3
    # Verify all elements are qints
    for element in arr:
        assert isinstance(element, ql.qint)


def test_array_1d_qbool():
    """Test creating 1D array of qbools."""
    arr = ql.array(5, dtype=ql.qbool)

    # Verify array creation
    assert arr is not None
    assert isinstance(arr, list)
    assert len(arr) == 5
    # Verify all elements are qbools
    for element in arr:
        assert isinstance(element, ql.qbool)


def test_array_2d_qbool():
    """Test creating 2D array of qbools."""
    arr = ql.array((3, 3), dtype=ql.qbool)

    # Verify array creation
    assert arr is not None
    assert isinstance(arr, list)
    assert len(arr) == 3
    # Verify 2D structure
    for row in arr:
        assert isinstance(row, list)
        assert len(row) == 3
        for element in row:
            assert isinstance(element, ql.qbool)


def test_array_2d_qint():
    """Test creating 2D array of qints."""
    arr = ql.array((2, 4), dtype=ql.qint)

    # Verify array creation
    assert arr is not None
    assert len(arr) == 2
    for row in arr:
        assert len(row) == 4
        for element in row:
            assert isinstance(element, ql.qint)


def test_array_from_list():
    """Test creating array from list of values."""
    arr = ql.array([1, 2, 3], dtype=ql.qint)

    # Verify array creation
    assert arr is not None
    assert isinstance(arr, list)
    assert len(arr) == 3
    # Verify all elements are qints
    for element in arr:
        assert isinstance(element, ql.qint)


# ============================================================================
# Combined Logic Operations
# ============================================================================


def test_complex_logic_expression():
    """Test combining multiple logic operations."""
    a = ql.qbool()
    b = ql.qbool()
    c = ql.qbool()

    # Complex expression
    result = (a & b) | c

    # Verify result is qbool
    assert result is not None
    assert isinstance(result, ql.qbool)


def test_qbool_in_comparison():
    """Test using qbool from comparison in logic operations."""
    a = ql.qint(value=5, bits=8)
    b = ql.qint(value=3, bits=8)

    # Get qbool from comparison
    cmp_result = a > b

    # Use in logic operation
    other = ql.qbool()
    logic_result = cmp_result & other

    # Verify types
    assert isinstance(cmp_result, ql.qbool)
    assert isinstance(logic_result, ql.qbool)


# ============================================================================
# Edge Cases
# ============================================================================


def test_array_empty_dimension():
    """Test array with dimension 0."""
    arr = ql.array(0, dtype=ql.qint)

    # Should return empty list
    assert arr is not None
    assert isinstance(arr, list)
    assert len(arr) == 0


def test_multiple_array_creation():
    """Test creating multiple arrays in sequence."""
    arr1 = ql.array(2, dtype=ql.qint)
    arr2 = ql.array(3, dtype=ql.qbool)
    arr3 = ql.array((2, 2), dtype=ql.qint)

    # Verify all arrays created successfully
    assert len(arr1) == 2
    assert len(arr2) == 3
    assert len(arr3) == 2
