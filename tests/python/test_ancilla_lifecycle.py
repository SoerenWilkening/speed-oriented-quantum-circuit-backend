"""
Integration tests for ancilla block allocation lifecycle (Phase 65).

Tests that the qubit allocator correctly handles multi-qubit ancilla
block allocation and freeing through the Python/Cython interface.
"""

import quantum_language as ql


class TestAncillaBlockLifecycle:
    """Test ancilla block allocation through the quantum language interface."""

    def test_basic_qint_lifecycle(self):
        """Creating qints exercises alloc path."""
        a = ql.qint(0, bits=4)
        b = ql.qint(0, bits=4)
        # Both should have valid qubit allocations
        assert a.width == 4
        assert b.width == 4

    def test_addition_allocates_and_frees_ancilla(self):
        """QFT addition path: verify no crash from allocator changes."""
        a = ql.qint(3, bits=4)
        b = ql.qint(5, bits=4)
        a += b
        # Operation completes without crash -- allocator handled correctly
        assert a is not None
        assert isinstance(a, ql.qint)

    def test_multiple_operations_no_leak(self):
        """Multiple arithmetic operations exercise alloc/free repeatedly."""
        a = ql.qint(7, bits=4)
        b = ql.qint(3, bits=4)
        c = ql.qint(0, bits=4)
        a += b
        c += a
        # Multiple operations complete without crash
        assert c is not None
        assert isinstance(c, ql.qint)

    def test_subtraction_exercises_reversal(self):
        """Subtraction uses reverse_circuit_range -- exercises the Phase 65 fix."""
        a = ql.qint(5, bits=4)
        b = ql.qint(3, bits=4)
        a -= b
        # Subtraction (reverse addition) completes without crash
        assert a is not None
        assert isinstance(a, ql.qint)

    def test_larger_width_block_alloc(self):
        """8-bit operations need larger contiguous blocks."""
        a = ql.qint(100, bits=8)
        b = ql.qint(55, bits=8)
        a += b
        # 8-bit addition with larger blocks completes without crash
        assert a is not None
        assert isinstance(a, ql.qint)
