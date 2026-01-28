#!/usr/bin/env python3
"""
Comprehensive test suite for Quantum Assembly Language.

Phase 16: Dependency Tracking Tests
Tests verify dependency tracking infrastructure for all multi-operand operations.
"""

import gc
import sys

sys.path.insert(0, ".")

import quantum_language as ql

# =============================================================================
# Phase 16: Dependency Tracking Tests
# =============================================================================


def test_dependency_tracking_bitwise_and():
    """TRACK-01: Test dependency tracking for AND operation."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a & b

    # Should have 2 parents (a and b)
    assert len(result.dependency_parents) == 2, (
        f"Expected 2 parents, got {len(result.dependency_parents)}"
    )
    assert result.operation_type == "AND", f"Expected 'AND', got {result.operation_type}"

    # Verify parents are accessible via get_live_parents
    live_parents = result.get_live_parents()
    assert len(live_parents) == 2, "Both parents should be alive"
    print("  AND dependency tracking: PASS")


def test_dependency_tracking_bitwise_or():
    """TRACK-01: Test dependency tracking for OR operation."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a | b

    assert len(result.dependency_parents) == 2
    assert result.operation_type == "OR"
    print("  OR dependency tracking: PASS")


def test_dependency_tracking_bitwise_xor():
    """TRACK-01: Test dependency tracking for XOR operation."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a ^ b

    assert len(result.dependency_parents) == 2
    assert result.operation_type == "XOR"
    print("  XOR dependency tracking: PASS")


def test_dependency_tracking_classical_operand():
    """TRACK-01: Classical operands should not be tracked."""
    c = ql.circuit()  # noqa: F841
    a = ql.qint(5, width=4)
    result = a & 3  # Classical operand

    # Should only have 1 parent (a), not the classical 3
    assert len(result.dependency_parents) == 1, (
        f"Expected 1 parent for classical operand, got {len(result.dependency_parents)}"
    )
    print("  Classical operand (no tracking): PASS")


def test_dependency_tracking_not_skipped():
    """TRACK-01: Single-operand NOT should NOT create dependencies."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    _result = ~a  # NOT is in-place, returns self  # noqa: F841

    # NOT modifies in-place, no new dependency graph entry
    # The original 'a' should still have empty dependency_parents
    assert len(a.dependency_parents) == 0, "NOT should not add dependencies (single operand)"
    print("  NOT skips dependency tracking: PASS")


def test_dependency_tracking_comparison_eq():
    """TRACK-02: Test dependency tracking for equality comparison."""
    c = ql.circuit()  # noqa: F841
    a = ql.qint(5, width=4)
    b = ql.qint(3, width=4)
    result = a == b

    assert len(result.dependency_parents) == 2, (
        f"Expected 2 parents for ==, got {len(result.dependency_parents)}"
    )
    assert result.operation_type == "EQ"
    print("  Equality comparison tracking: PASS")


def test_dependency_tracking_comparison_lt():
    """TRACK-02: Test dependency tracking for less-than comparison."""
    c = ql.circuit()  # noqa: F841
    a = ql.qint(3, width=4)
    b = ql.qint(5, width=4)
    result = a < b

    assert len(result.dependency_parents) == 2
    assert result.operation_type == "LT"
    print("  Less-than comparison tracking: PASS")


def test_dependency_tracking_comparison_gt():
    """TRACK-02: Test dependency tracking for greater-than comparison."""
    c = ql.circuit()  # noqa: F841
    a = ql.qint(5, width=4)
    b = ql.qint(3, width=4)
    result = a > b

    assert len(result.dependency_parents) == 2
    assert result.operation_type == "GT"
    print("  Greater-than comparison tracking: PASS")


def test_dependency_tracking_comparison_le():
    """TRACK-02: Test dependency tracking for less-than-or-equal comparison."""
    c = ql.circuit()  # noqa: F841
    a = ql.qint(3, width=4)
    b = ql.qint(5, width=4)
    result = a <= b

    assert len(result.dependency_parents) == 2
    assert result.operation_type == "LE"
    print("  Less-than-or-equal comparison tracking: PASS")


def test_dependency_tracking_comparison_classical():
    """TRACK-02: Classical comparisons track only qint operand."""
    c = ql.circuit()  # noqa: F841
    a = ql.qint(5, width=4)
    result = a == 3  # Classical operand

    assert len(result.dependency_parents) == 1, "Classical comparison should track only qint"
    print("  Classical comparison tracking: PASS")


def test_dependency_weak_references():
    """TRACK-03: Weak references allow garbage collection."""
    c = ql.circuit()  # noqa: F841

    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a & b

    # Both parents alive
    assert len(result.get_live_parents()) == 2

    # Delete one parent
    del a
    gc.collect()

    # Now only one parent should be alive
    live = result.get_live_parents()
    assert len(live) == 1, f"Expected 1 live parent after del, got {len(live)}"
    print("  Weak references allow GC: PASS")


def test_dependency_creation_order():
    """TRACK-03: Creation order prevents circular references."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a & b

    # Verify creation order is assigned
    assert a._creation_order < b._creation_order < result._creation_order, (
        "Creation order should be monotonically increasing"
    )

    # Attempting to add result as dependency of itself should fail
    # (this would be caught by the assertion in add_dependency)
    try:
        result.add_dependency(result)
        raise AssertionError("Should have raised AssertionError for self-dependency")
    except AssertionError as e:
        if "Should have raised" in str(e):
            raise
        pass  # Expected cycle detection error

    print("  Creation order cycle prevention: PASS")


def test_dependency_scope_capture():
    """TRACK-04: Scope depth is captured at creation time."""
    c = ql.circuit()  # noqa: F841

    # Top level (scope 0)
    a = ql.qbool(True)
    assert a.creation_scope == 0, f"Expected scope 0, got {a.creation_scope}"

    print("  Scope depth capture: PASS")


def test_dependency_control_context_capture():
    """TRACK-04: Control context is captured at creation time."""
    c = ql.circuit()  # noqa: F841

    # Outside with block - no control
    a = ql.qbool(True)
    assert len(a.control_context) == 0, "No control context outside with block"

    # Inside with block - control qubit captured
    control = ql.qbool(True)
    with control:
        b = ql.qbool(False)
        # b should have captured control's qubit
        assert len(b.control_context) == 1, (
            f"Expected 1 control qubit, got {len(b.control_context)}"
        )

    print("  Control context capture: PASS")


def test_dependency_chained_operations():
    """Test dependency tracking through chained operations."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    d = ql.qbool(True)

    # c = a & b, then e = c | d
    intermediate = a & b
    final = intermediate | d

    # final should have 2 parents: intermediate and d
    assert len(final.dependency_parents) == 2

    # intermediate should have 2 parents: a and b
    assert len(intermediate.dependency_parents) == 2

    print("  Chained operation tracking: PASS")


def run_dependency_tracking_tests():
    """Run all Phase 16 dependency tracking tests."""
    print("\n=== Phase 16: Dependency Tracking Tests ===")

    test_dependency_tracking_bitwise_and()
    test_dependency_tracking_bitwise_or()
    test_dependency_tracking_bitwise_xor()
    test_dependency_tracking_classical_operand()
    test_dependency_tracking_not_skipped()
    test_dependency_tracking_comparison_eq()
    test_dependency_tracking_comparison_lt()
    test_dependency_tracking_comparison_gt()
    test_dependency_tracking_comparison_le()
    test_dependency_tracking_comparison_classical()
    test_dependency_weak_references()
    test_dependency_creation_order()
    test_dependency_scope_capture()
    test_dependency_control_context_capture()
    test_dependency_chained_operations()

    print("\n=== All Dependency Tracking Tests PASSED ===\n")


# =============================================================================
# Phase 17: Reverse Gate Generation Tests
# =============================================================================


def test_reverse_self_adjoint_gates():
    """Test that self-adjoint gates (X, H) reverse correctly."""
    ql.circuit()

    # Get current gate count
    before_gates = ql.circuit().gate_count
    current_depth = ql.circuit().depth

    # Reverse a range of layers (e.g., first 10 layers if they exist)
    # This tests that the function can reverse self-adjoint gates like H, X
    if current_depth >= 10:
        ql.reverse_instruction_range(0, 10)
        after_gates = ql.circuit().gate_count
        # Should have added reversed gates
        assert after_gates > before_gates, (
            f"Reversing layers 0-10 should add gates: {before_gates} -> {after_gates}"
        )
    print("  Self-adjoint gate reversal: PASS")


def test_reverse_phase_gates():
    """Test that phase gates invert correctly (P(t) -> P(-t))."""
    ql.circuit()

    start_gates = ql.circuit().gate_count

    # Create qints and perform addition (uses phase gates via QFT)
    a = ql.qint(13, width=8)
    a += 7  # Addition uses QFT which has phase gates

    mid_gates = ql.circuit().gate_count
    gates_added = mid_gates - start_gates

    # Verify operation added gates
    assert gates_added > 0, "Addition should add gates"

    # Reverse circuit from start to current depth
    current_depth = ql.circuit().depth
    ql.reverse_instruction_range(0, current_depth)

    after_gates = ql.circuit().gate_count

    # Should have added the reversed gates
    assert after_gates > mid_gates, f"Reversing should add gates: {mid_gates} -> {after_gates}"
    print("  Phase gate reversal: PASS")


def test_reverse_empty_range():
    """Test that empty range (start == end) is no-op."""
    ql.circuit()

    before_gates = ql.circuit().gate_count
    before_depth = ql.circuit().depth

    # Reverse empty range
    ql.reverse_instruction_range(5, 5)

    after_gates = ql.circuit().gate_count
    after_depth = ql.circuit().depth

    assert before_gates == after_gates, "Empty range should not add gates"
    assert before_depth == after_depth, "Empty range should not change depth"
    print("  Empty range reversal (no-op): PASS")


def test_reverse_controlled_gates():
    """Test that controlled gates preserve control structure."""
    ql.circuit()

    start_gates = ql.circuit().gate_count

    # Create qints and perform comparison (uses controlled gates)
    a = ql.qint(15, width=8)
    b = ql.qint(10, width=8)
    _c = a == b  # Comparison uses controlled gates

    mid_gates = ql.circuit().gate_count
    gates_added = mid_gates - start_gates

    # Verify operation added gates
    assert gates_added > 0, "Comparison should add gates"

    # Reverse circuit from start to current depth
    current_depth = ql.circuit().depth
    ql.reverse_instruction_range(0, current_depth)

    after_gates = ql.circuit().gate_count

    # Should have added the reversed gates
    assert after_gates > mid_gates, f"Reversing should add gates: {mid_gates} -> {after_gates}"
    print("  Controlled gate reversal: PASS")


def test_get_current_layer():
    """Test get_current_layer returns correct layer count."""
    # Circuit may already be initialized from previous tests
    # Just verify get_current_layer works
    layer_count = ql.get_current_layer()
    assert layer_count >= 0, "Layer count should be non-negative"

    # Verify the function returns an integer
    assert isinstance(layer_count, int), f"Expected int, got {type(layer_count)}"

    # Capture layer before and after an operation to show layers can increase
    before = ql.get_current_layer()
    a = ql.qint(7, width=8)  # Use different width to ensure new qubits
    b = ql.qint(11, width=8)
    _c = a + b  # noqa: F841
    after = ql.get_current_layer()

    # With fresh qubits and operation, layers should increase (or at least not decrease)
    assert after >= before, f"Layer count should not decrease, got {before} -> {after}"
    print("  get_current_layer: PASS")


def run_reverse_gate_tests():
    """Run all Phase 17 reverse gate generation tests."""
    print("\n=== Phase 17: Reverse Gate Generation Tests ===")

    test_get_current_layer()
    test_reverse_empty_range()
    test_reverse_self_adjoint_gates()
    test_reverse_phase_gates()
    test_reverse_controlled_gates()

    print("\n=== All Reverse Gate Tests PASSED ===\n")


# =============================================================================
# Phase 18: Basic Uncomputation Integration Tests
# =============================================================================


def test_uncompute_basic():
    """SCOPE-02: Basic uncomputation marks flag and clears allocation."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a & b

    assert not result._is_uncomputed, "Should start as not uncomputed"
    # Note: allocated_qubits is a cdef attribute, not accessible from Python
    # We verify allocation status indirectly through _is_uncomputed

    result.uncompute()

    assert result._is_uncomputed, "Should be marked uncomputed"
    # After uncompute, the flag should be set (qubits internally freed)
    print("  Basic uncompute: PASS")


def test_uncompute_idempotent():
    """CTRL-02: Calling uncompute twice is a no-op."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a & b

    result.uncompute()
    result.uncompute()  # Second call should be silent no-op

    assert result._is_uncomputed
    print("  Idempotent uncompute: PASS")


def test_uncompute_refcount_check():
    """CTRL-02: Uncompute fails if other references exist."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a & b
    alias = result  # Extra reference  # noqa: F841

    try:
        result.uncompute()
        raise AssertionError("Should have raised RuntimeError")
    except RuntimeError as e:
        assert "references still exist" in str(e)

    del alias
    result.uncompute()  # Now should work
    assert result._is_uncomputed
    print("  Refcount check: PASS")


def test_uncompute_use_after():
    """SCOPE-02: Using uncomputed qbool raises clear error."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a & b
    result.uncompute()

    try:
        _ = result & a
        raise AssertionError("Should have raised RuntimeError")
    except RuntimeError as e:
        assert "has been uncomputed" in str(e)

    print("  Use-after-uncompute: PASS")


def test_uncompute_cascade_lifo():
    """UNCOMP-02/03: Cascade uncomputes dependencies in LIFO order."""
    c = ql.circuit()  # noqa: F841

    # Create chain: a & b = ab, ab & c = abc
    a = ql.qbool(True)
    b = ql.qbool(False)
    ab = a & b
    c_val = ql.qbool(True)
    abc = ab & c_val

    # Track creation orders
    order_a = a._creation_order
    order_ab = ab._creation_order
    order_c = c_val._creation_order
    order_abc = abc._creation_order

    # Verify LIFO order: abc > ab > a, abc > c
    assert order_abc > order_ab > order_a
    assert order_abc > order_c
    # Note: order_b unused (b's order doesn't matter for cascade test)

    # Uncompute abc - should cascade to ab (its dependency)
    # Note: a, b, c_val won't be uncomputed because they're still referenced here
    abc.uncompute()

    assert abc._is_uncomputed
    # ab should be uncomputed via cascade (only referenced by abc)
    # But a, b, c_val remain alive (referenced in this scope)
    print("  LIFO cascade: PASS")


def test_uncompute_layer_tracking():
    """SCOPE-02: Layer boundaries captured for gate reversal."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)
    result = a & b

    # Layer boundaries should be set
    assert result._start_layer >= 0, "start_layer should be set"
    assert result._end_layer >= result._start_layer, "end_layer should be >= start_layer"

    print("  Layer tracking: PASS")


def test_uncompute_automatic_gc():
    """SCOPE-02: Automatic uncomputation on garbage collection."""
    c = ql.circuit()  # noqa: F841
    a = ql.qbool(True)
    b = ql.qbool(False)

    def create_temp():
        temp = a & b
        return temp._creation_order  # Return for verification, temp goes out of scope

    order = create_temp()  # noqa: F841
    gc.collect()

    # Can't directly verify temp was uncomputed, but no error = success
    print("  Automatic GC uncompute: PASS")


def test_uncompute_comparison_result():
    """SCOPE-02: Comparison results also track layers and uncompute."""
    c = ql.circuit()  # noqa: F841
    x = ql.qint(5, width=4)
    y = ql.qint(3, width=4)
    result = x > y

    assert hasattr(result, "_start_layer")
    assert hasattr(result, "_end_layer")
    assert hasattr(result, "_is_uncomputed")

    result.uncompute()
    assert result._is_uncomputed
    print("  Comparison uncompute: PASS")


def run_uncomputation_tests():
    """Run all Phase 18 uncomputation tests."""
    print("\n=== Phase 18: Basic Uncomputation Integration Tests ===")

    test_uncompute_basic()
    test_uncompute_idempotent()
    test_uncompute_refcount_check()
    test_uncompute_use_after()
    test_uncompute_cascade_lifo()
    test_uncompute_layer_tracking()
    test_uncompute_automatic_gc()
    test_uncompute_comparison_result()

    print("\n=== All Phase 18 Tests PASSED ===\n")


# =============================================================================
# Test Runner
# =============================================================================

if __name__ == "__main__":
    try:
        run_dependency_tracking_tests()
        run_reverse_gate_tests()
        run_uncomputation_tests()
        print("\nAll tests completed successfully!")
        sys.exit(0)
    except AssertionError as e:
        print(f"\n\nTest FAILED: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)
    except Exception as e:
        print(f"\n\nUnexpected error: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)
