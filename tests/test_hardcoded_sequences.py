"""Validation tests for hardcoded addition sequences.

These tests verify that hardcoded (pre-computed) sequences for widths 1-8
produce correct arithmetic results - functional equivalence to dynamic generation.

NOTE: We test ARITHMETIC CORRECTNESS, not gate-by-gate structure comparison.
The internal gate sequence is an implementation detail. What matters is that
addition operations produce mathematically correct results.

Per CONTEXT.md, this is one-time verification (not in regular CI).
Mark with @pytest.mark.hardcoded_validation to allow filtering.
"""

import warnings

import pytest
from verify_helpers import format_failure_message

import quantum_language as ql

# Suppress cosmetic warnings for values >= 2^(width-1) in unsigned interpretation
warnings.filterwarnings("ignore", message="Value .* exceeds")


@pytest.mark.hardcoded_validation
class TestHardcodedSequenceValidation:
    """Verify hardcoded sequences produce correct arithmetic results."""

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_qq_add_produces_correct_results(self, verify_circuit, width):
        """QQ_add hardcoded produces correct arithmetic results."""
        # Test representative values including edge cases
        test_cases = [(0, 0), (1, 1)]
        max_val = (1 << width) - 1

        # Add more test cases that fit within the width
        if width >= 2:
            test_cases.append((2, 3) if max_val >= 3 else (max_val, 1))

        # Boundary cases
        test_cases.extend([(max_val, 1), (max_val, max_val)])

        for a, b in test_cases:
            if a >= (1 << width) or b >= (1 << width):
                continue

            def circuit_builder(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qb = ql.qint(b, width=w)
                _r = qa + qb
                return (a + b) % (1 << w)

            actual, expected = verify_circuit(circuit_builder, width)
            assert actual == expected, format_failure_message(
                "qq_add_hardcoded", [a, b], width, expected, actual
            )

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_cq_add_produces_correct_results(self, verify_circuit, width):
        """CQ_add (classical-quantum) produces correct arithmetic results."""
        # Test representative values
        test_cases = [(0, 0), (1, 1)]
        max_val = (1 << width) - 1

        if width >= 2:
            test_cases.append((2, 3) if max_val >= 3 else (max_val, 1))

        test_cases.extend([(max_val, 1), (1, max_val)])

        for a, b in test_cases:
            if a >= (1 << width) or b >= (1 << width):
                continue

            def circuit_builder(a=a, b=b, w=width):
                qa = ql.qint(a, width=w)
                qa += b  # In-place classical addition
                return (a + b) % (1 << w)

            actual, expected = verify_circuit(circuit_builder, width)
            assert actual == expected, format_failure_message(
                "cq_add_hardcoded", [a, b], width, expected, actual
            )

    def test_dynamic_fallback_widths(self, verify_circuit):
        """Width 9 uses dynamic generation and produces correct results.

        NOTE: QQ addition for width 9+ requires 3*width qubits for result.
        Width 9 uses 27 qubits total which is near the simulation limit.
        Width 10+ exceeds available memory for statevector simulation.
        """
        width = 9
        max_val = 255

        def circuit_builder(w=width, mv=max_val):
            qa = ql.qint(1, width=w)
            qb = ql.qint(mv, width=w)
            _r = qa + qb
            return (1 + mv) % (1 << w)

        actual, expected = verify_circuit(circuit_builder, width)
        assert actual == expected, format_failure_message(
            "qq_add_dynamic", [1, max_val], width, expected, actual
        )

    @pytest.mark.parametrize("width", [9, 10])
    def test_cq_dynamic_fallback_widths(self, verify_circuit, width):
        """CQ addition with widths > 8 uses dynamic generation correctly."""
        max_val = min((1 << width) - 1, 255)

        def circuit_builder(w=width, mv=max_val):
            qa = ql.qint(1, width=w)
            qa += mv
            return (1 + mv) % (1 << w)

        actual, expected = verify_circuit(circuit_builder, width)
        assert actual == expected, format_failure_message(
            "cq_add_dynamic", [1, max_val], width, expected, actual
        )


@pytest.mark.hardcoded_validation
class TestHardcodedSequenceExecution:
    """Verify hardcoded sequences execute without errors."""

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_qq_add_circuit_executes(self, width):
        """QQ_add hardcoded executes without error and produces non-trivial circuit."""
        # NOTE: ql.circuit() resets the circuit and returns a reference.
        # Do NOT call it again after building - use the same reference.
        circ = ql.circuit()
        qa = ql.qint(0, width=width)
        qb = ql.qint(0, width=width)
        _r = qa + qb

        assert circ.depth >= 1, f"Circuit should have non-zero depth for width {width}"

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_controlled_cq_add_circuit_executes(self, width):
        """Controlled CQ_add executes without error.

        NOTE: Controlled QQ addition (qa + qb inside `with ctrl:`) raises
        NotImplementedError at user level. However, controlled CQ addition
        (qa += b inside `with ctrl:`) works and uses the cQQ_add sequences
        internally via CQ_add's controlled path.
        """
        circ = ql.circuit()
        qa = ql.qint(1, width=width)

        # Controlled CQ addition via context manager
        ctrl = ql.qint(1, width=1)
        with ctrl:
            qa += 1  # Controlled in-place classical addition

        assert circ.depth >= 1, f"Controlled circuit should have depth for width {width}"

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_cq_add_circuit_executes(self, width):
        """CQ_add (classical-quantum) executes without error."""
        circ = ql.circuit()
        qa = ql.qint(0, width=width)
        qa += 1  # In-place classical addition

        assert circ.depth >= 1, f"CQ circuit should have non-zero depth for width {width}"


@pytest.mark.hardcoded_validation
class TestHardcodedBoundaryConditions:
    """Test boundary conditions for hardcoded sequence widths."""

    def test_width_8_boundary(self, verify_circuit):
        """Width 8 is the last hardcoded width - verify it works correctly."""
        max_val = 255  # 2^8 - 1

        def circuit_builder():
            qa = ql.qint(max_val, width=8)
            qb = ql.qint(1, width=8)
            _r = qa + qb
            return 0  # 255 + 1 = 256 mod 256 = 0

        actual, expected = verify_circuit(circuit_builder, 8)
        assert actual == expected, f"Width 8 overflow: expected {expected}, got {actual}"

    def test_width_9_uses_dynamic(self, verify_circuit):
        """Width 9 should fall back to dynamic generation."""

        def circuit_builder():
            qa = ql.qint(1, width=9)
            qb = ql.qint(1, width=9)
            _r = qa + qb
            return 2

        actual, expected = verify_circuit(circuit_builder, 9)
        assert actual == expected, f"Width 9 dynamic: expected {expected}, got {actual}"

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_zero_plus_zero(self, verify_circuit, width):
        """Addition identity: 0 + 0 = 0 for all hardcoded widths."""

        def circuit_builder(w=width):
            qa = ql.qint(0, width=w)
            qb = ql.qint(0, width=w)
            _r = qa + qb
            return 0

        actual, expected = verify_circuit(circuit_builder, width)
        assert actual == expected, f"Width {width} identity: expected {expected}, got {actual}"

    @pytest.mark.parametrize("width", [1, 2, 3, 4, 5, 6, 7, 8])
    def test_overflow_wrapping(self, verify_circuit, width):
        """Max value + 1 wraps to 0 for all hardcoded widths."""
        max_val = (1 << width) - 1

        def circuit_builder(w=width, mv=max_val):
            qa = ql.qint(mv, width=w)
            qb = ql.qint(1, width=w)
            _r = qa + qb
            return 0  # max + 1 wraps to 0

        actual, expected = verify_circuit(circuit_builder, width)
        assert actual == expected, f"Width {width} overflow: expected {expected}, got {actual}"
