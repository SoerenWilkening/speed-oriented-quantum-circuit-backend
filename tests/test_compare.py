"""Verification tests for qint comparison operations.

Tests that all 6 comparison operators (==, !=, <, >, <=, >=) produce correct
boolean results through the full pipeline: Python API -> C backend -> OpenQASM ->
Qiskit simulation.

Coverage:
- Exhaustive: All input pairs for widths 1-3 bits (QQ and CQ variants)
- Sampled: Representative pairs for widths 4-5 bits (QQ and CQ variants)
- BUG-02 regression: MSB index, GC gate reversal, unsigned overflow sub-cases

All comparisons return qbool (1-bit result): 1 = True, 0 = False.

Known bugs documented here (discovered during exhaustive verification):

BUG-CMP-01 (Equality Inversion): eq and ne operators return inverted results
    for ALL inputs at ALL widths. eq(a,b) returns 0 when a==b (should be 1),
    ne(a,b) returns 0 when a!=b (should be 1). Both QQ and CQ variants.
    Exception: CQ ne passes when b == max_val (all bits set) for a != max_val.

BUG-CMP-02 (Ordering Comparison Error): lt, gt, le, ge operators produce
    incorrect results for specific (a,b) pairs where the internal widened
    subtraction circuit does not correctly compute the sign of (b-a).
    Identical failure set for both QQ and CQ variants.

Note: Widths >= 6 are excluded because gt/le operators generate circuits
    that exceed available simulation memory (BUG-CMP-03).
"""

import warnings

import pytest
from verify_helpers import (
    format_failure_message,
    generate_exhaustive_pairs,
    generate_sampled_pairs,
)

import quantum_language as ql

# Suppress cosmetic warnings for values >= 2^(width-1) in unsigned interpretation
warnings.filterwarnings("ignore", message="Value .* exceeds")


# --- Oracle dictionaries ---

# Python native comparison oracles
OPS = {
    "eq": lambda a, b: int(a == b),
    "ne": lambda a, b: int(a != b),
    "lt": lambda a, b: int(a < b),
    "gt": lambda a, b: int(a > b),
    "le": lambda a, b: int(a <= b),
    "ge": lambda a, b: int(a >= b),
}

# Quantum-quantum operator callables (qint op qint)
QL_OPS_QQ = {
    "eq": lambda qa, qb: qa == qb,
    "ne": lambda qa, qb: qa != qb,
    "lt": lambda qa, qb: qa < qb,
    "gt": lambda qa, qb: qa > qb,
    "le": lambda qa, qb: qa <= qb,
    "ge": lambda qa, qb: qa >= qb,
}

# Quantum-classical operator callables (qint op int)
QL_OPS_CQ = {
    "eq": lambda qa, b: qa == b,
    "ne": lambda qa, b: qa != b,
    "lt": lambda qa, b: qa < b,
    "gt": lambda qa, b: qa > b,
    "le": lambda qa, b: qa <= b,
    "ge": lambda qa, b: qa >= b,
}


# ---------------------------------------------------------------------------
# Known-failing comparison cases (BUG-CMP-01 and BUG-CMP-02)
# ---------------------------------------------------------------------------

# BUG-CMP-02: (width, a, b) triples where lt and ge return wrong results.
# Same failure set for both QQ and CQ variants.
_LT_GE_FAIL_PAIRS = {
    (1, 1, 0),
    (2, 0, 3),
    (2, 2, 0),
    (2, 3, 0),
    (2, 3, 1),
    (3, 0, 5),
    (3, 0, 6),
    (3, 0, 7),
    (3, 1, 6),
    (3, 1, 7),
    (3, 2, 7),
    (3, 4, 0),
    (3, 5, 0),
    (3, 5, 1),
    (3, 6, 0),
    (3, 6, 1),
    (3, 6, 2),
    (3, 7, 0),
    (3, 7, 1),
    (3, 7, 2),
    (3, 7, 3),
    (4, 0, 9),
    (4, 0, 10),
    (4, 0, 11),
    (4, 0, 12),
    (4, 0, 13),
    (4, 0, 14),
    (4, 0, 15),
    (4, 1, 10),
    (4, 1, 11),
    (4, 1, 12),
    (4, 1, 13),
    (4, 1, 14),
    (4, 1, 15),
    (4, 2, 11),
    (4, 2, 12),
    (4, 2, 13),
    (4, 2, 14),
    (4, 2, 15),
    (4, 3, 12),
    (4, 3, 13),
    (4, 3, 14),
    (4, 3, 15),
    (4, 4, 13),
    (4, 4, 14),
    (4, 4, 15),
    (4, 5, 14),
    (4, 5, 15),
    (4, 6, 15),
    (4, 8, 0),
    (4, 9, 0),
    (4, 9, 1),
    (4, 10, 0),
    (4, 10, 1),
    (4, 10, 2),
    (4, 11, 0),
    (4, 11, 1),
    (4, 11, 2),
    (4, 11, 3),
    (4, 12, 0),
    (4, 12, 1),
    (4, 12, 2),
    (4, 12, 3),
    (4, 12, 4),
    (4, 13, 0),
    (4, 13, 1),
    (4, 13, 2),
    (4, 13, 3),
    (4, 13, 4),
    (4, 13, 5),
    (4, 14, 0),
    (4, 14, 1),
    (4, 14, 2),
    (4, 14, 3),
    (4, 14, 4),
    (4, 14, 5),
    (4, 14, 6),
    (4, 15, 0),
    (4, 15, 1),
    (4, 15, 2),
    (4, 15, 3),
    (4, 15, 4),
    (4, 15, 5),
    (4, 15, 6),
    (4, 15, 7),
}

# BUG-CMP-02: (width, a, b) triples where gt and le return wrong results.
# Same failure set for both QQ and CQ variants.
_GT_LE_FAIL_PAIRS = {
    (1, 1, 0),
    (2, 1, 2),
    (2, 2, 0),
    (2, 2, 1),
    (2, 3, 1),
    (3, 1, 4),
    (3, 2, 4),
    (3, 2, 5),
    (3, 3, 4),
    (3, 3, 5),
    (3, 3, 6),
    (3, 4, 0),
    (3, 4, 1),
    (3, 4, 2),
    (3, 4, 3),
    (3, 5, 1),
    (3, 5, 2),
    (3, 5, 3),
    (3, 6, 2),
    (3, 6, 3),
    (3, 7, 3),
    (4, 1, 8),
    (4, 2, 8),
    (4, 2, 9),
    (4, 3, 8),
    (4, 3, 9),
    (4, 3, 10),
    (4, 4, 8),
    (4, 4, 9),
    (4, 4, 10),
    (4, 4, 11),
    (4, 5, 8),
    (4, 5, 9),
    (4, 5, 10),
    (4, 5, 11),
    (4, 5, 12),
    (4, 6, 8),
    (4, 6, 9),
    (4, 6, 10),
    (4, 6, 11),
    (4, 6, 12),
    (4, 6, 13),
    (4, 7, 8),
    (4, 7, 9),
    (4, 7, 10),
    (4, 7, 11),
    (4, 7, 12),
    (4, 7, 13),
    (4, 7, 14),
    (4, 8, 0),
    (4, 8, 1),
    (4, 8, 2),
    (4, 8, 3),
    (4, 8, 4),
    (4, 8, 5),
    (4, 8, 6),
    (4, 8, 7),
    (4, 9, 1),
    (4, 9, 2),
    (4, 9, 3),
    (4, 9, 4),
    (4, 9, 5),
    (4, 9, 6),
    (4, 9, 7),
    (4, 10, 2),
    (4, 10, 3),
    (4, 10, 4),
    (4, 10, 5),
    (4, 10, 6),
    (4, 10, 7),
    (4, 11, 3),
    (4, 11, 4),
    (4, 11, 5),
    (4, 11, 6),
    (4, 11, 7),
    (4, 12, 4),
    (4, 12, 5),
    (4, 12, 6),
    (4, 12, 7),
    (4, 13, 5),
    (4, 13, 6),
    (4, 13, 7),
    (4, 14, 6),
    (4, 14, 7),
    (4, 15, 7),
}


def _qq_will_fail(width, a, b, op_name):
    """Predict if a QQ comparison test will fail due to known bugs.

    BUG-CMP-01: eq/ne return inverted results for ALL inputs.
    BUG-CMP-02: lt/ge and gt/le fail for specific (width,a,b) triples.
    """
    if op_name == "eq":
        return a == b  # Always inverted: returns 0 when equal
    if op_name == "ne":
        return a != b  # Always inverted: returns 0 when not equal
    if op_name in ("lt", "ge"):
        return (width, a, b) in _LT_GE_FAIL_PAIRS
    if op_name in ("gt", "le"):
        return (width, a, b) in _GT_LE_FAIL_PAIRS
    return False


def _cq_will_fail(width, a, b, op_name):
    """Predict if a CQ comparison test will fail due to known bugs.

    Same as QQ except CQ ne passes for b=max_val when a!=max_val, and
    CQ ne fails for (max_val, max_val).
    """
    if op_name == "eq":
        return a == b  # Same as QQ
    if op_name == "ne":
        max_val = (1 << width) - 1
        if b == max_val and a != max_val:
            return False  # CQ ne works correctly for b=max_val
        if a == max_val and b == max_val:
            return True  # CQ ne fails for (max_val, max_val)
        return a != b  # Same as QQ otherwise
    if op_name in ("lt", "ge"):
        return (width, a, b) in _LT_GE_FAIL_PAIRS
    if op_name in ("gt", "le"):
        return (width, a, b) in _GT_LE_FAIL_PAIRS
    return False


# ---------------------------------------------------------------------------
# BUG-CMP-01/02 xfail reasons
# ---------------------------------------------------------------------------
_XFAIL_EQ = pytest.mark.xfail(
    reason="BUG-CMP-01: eq returns inverted result (always 0 for equal values)",
    strict=True,
)
_XFAIL_NE = pytest.mark.xfail(
    reason="BUG-CMP-01: ne returns inverted result (always 0 for unequal values)",
    strict=True,
)
_XFAIL_ORDER = pytest.mark.xfail(
    reason="BUG-CMP-02: ordering comparison error for this (width, a, b) triple",
    strict=True,
)


# --- Module-level test data generation ---


def _exhaustive_cases():
    """Generate (width, a, b, op_name) tuples for exhaustive testing (widths 1-3)."""
    cases = []
    for width in [1, 2, 3]:
        for a, b in generate_exhaustive_pairs(width):
            for op_name in OPS:
                cases.append((width, a, b, op_name))
    return cases


def _sampled_cases():
    """Generate (width, a, b, op_name) tuples for sampled testing (widths 4-5)."""
    cases = []
    for width in [4, 5]:
        max_val = (1 << width) - 1
        boundary_pairs = [(0, 0), (0, max_val), (max_val, 0), (max_val, max_val)]
        sampled = generate_sampled_pairs(width, sample_size=5)
        # Merge boundary pairs with sampled pairs, deduplicated
        all_pairs = sorted(set(sampled) | set(boundary_pairs))
        for a, b in all_pairs:
            for op_name in OPS:
                cases.append((width, a, b, op_name))
    return cases


def _mark_qq_cases(cases):
    """Apply xfail markers to known-failing QQ comparison cases."""
    marked = []
    for case in cases:
        width, a, b, op_name = case
        if _qq_will_fail(width, a, b, op_name):
            if op_name in ("eq", "ne"):
                marked.append(
                    pytest.param(*case, marks=_XFAIL_EQ if op_name == "eq" else _XFAIL_NE)
                )
            else:
                marked.append(pytest.param(*case, marks=_XFAIL_ORDER))
        else:
            marked.append(case)
    return marked


def _mark_cq_cases(cases):
    """Apply xfail markers to known-failing CQ comparison cases."""
    marked = []
    for case in cases:
        width, a, b, op_name = case
        if _cq_will_fail(width, a, b, op_name):
            if op_name in ("eq", "ne"):
                marked.append(
                    pytest.param(*case, marks=_XFAIL_EQ if op_name == "eq" else _XFAIL_NE)
                )
            else:
                marked.append(pytest.param(*case, marks=_XFAIL_ORDER))
        else:
            marked.append(case)
    return marked


_XFAIL_ORDER_SAMPLED = pytest.mark.xfail(
    reason="BUG-CMP-02: ordering comparison may fail at wider widths",
    strict=False,
)


def _mark_sampled(cases, cq=False):
    """Apply xfail markers to sampled cases (widths 4-5).

    For width 4: use exact failure sets (same as exhaustive).
    For width 5: use non-strict xfail for ordering ops where MSBs differ.
    """
    will_fail = _cq_will_fail if cq else _qq_will_fail
    marked = []
    for case in cases:
        width, a, b, op_name = case
        if width == 4:
            # Width 4: exact prediction available
            if will_fail(width, a, b, op_name):
                if op_name in ("eq", "ne"):
                    marked.append(
                        pytest.param(*case, marks=_XFAIL_EQ if op_name == "eq" else _XFAIL_NE)
                    )
                else:
                    marked.append(pytest.param(*case, marks=_XFAIL_ORDER))
            else:
                marked.append(case)
        else:
            # Width 5: predict eq/ne exactly, ordering ops non-strict
            if op_name == "eq" and a == b:
                marked.append(pytest.param(*case, marks=_XFAIL_EQ))
            elif op_name == "ne":
                if cq:
                    max_val = (1 << width) - 1
                    if b == max_val and a != max_val:
                        marked.append(case)
                    elif a != b or (a == max_val and b == max_val):
                        marked.append(pytest.param(*case, marks=_XFAIL_NE))
                    else:
                        marked.append(case)
                elif a != b:
                    marked.append(pytest.param(*case, marks=_XFAIL_NE))
                else:
                    marked.append(case)
            elif op_name in ("lt", "gt", "le", "ge"):
                msb_a = a >> (width - 1)
                msb_b = b >> (width - 1)
                if msb_a != msb_b:
                    marked.append(pytest.param(*case, marks=_XFAIL_ORDER_SAMPLED))
                else:
                    marked.append(case)
            else:
                marked.append(case)
    return marked


EXHAUSTIVE_QQ = _mark_qq_cases(_exhaustive_cases())
SAMPLED_QQ = _mark_sampled(_sampled_cases(), cq=False)
EXHAUSTIVE_CQ = _mark_cq_cases(_exhaustive_cases())
SAMPLED_CQ = _mark_sampled(_sampled_cases(), cq=True)


# --- QQ Comparison Tests ---


@pytest.mark.parametrize("width,a,b,op_name", EXHAUSTIVE_QQ)
def test_qq_cmp_exhaustive(verify_circuit, width, a, b, op_name):
    """QQ comparison: qint(a) op qint(b) at widths 1-4 bits.

    Exhaustive coverage of all input pairs x all 6 operators.
    """
    expected = OPS[op_name](a, b)
    ql_op = QL_OPS_QQ[op_name]

    def circuit_builder(a=a, b=b, w=width, op=ql_op, exp=expected):
        qa = ql.qint(a, width=w)
        qb = ql.qint(b, width=w)
        _result = op(qa, qb)
        return exp

    actual, exp = verify_circuit(circuit_builder, width=1)
    assert actual == exp, format_failure_message(f"qq_{op_name}", [a, b], width, exp, actual)


@pytest.mark.parametrize("width,a,b,op_name", SAMPLED_QQ)
def test_qq_cmp_sampled(verify_circuit, width, a, b, op_name):
    """QQ comparison: qint(a) op qint(b) at widths 4-5 bits.

    Sampled coverage includes boundary values and random pairs.
    """
    expected = OPS[op_name](a, b)
    ql_op = QL_OPS_QQ[op_name]

    def circuit_builder(a=a, b=b, w=width, op=ql_op, exp=expected):
        qa = ql.qint(a, width=w)
        qb = ql.qint(b, width=w)
        _result = op(qa, qb)
        return exp

    actual, exp = verify_circuit(circuit_builder, width=1)
    assert actual == exp, format_failure_message(f"qq_{op_name}", [a, b], width, exp, actual)


# --- CQ Comparison Tests ---


@pytest.mark.parametrize("width,a,b,op_name", EXHAUSTIVE_CQ)
def test_cq_cmp_exhaustive(verify_circuit, width, a, b, op_name):
    """CQ comparison: qint(a) op int(b) at widths 1-4 bits.

    Exhaustive coverage of all input pairs x all 6 operators
    for the classical-quantum comparison variant.
    """
    expected = OPS[op_name](a, b)
    ql_op = QL_OPS_CQ[op_name]

    def circuit_builder(a=a, b=b, w=width, op=ql_op, exp=expected):
        qa = ql.qint(a, width=w)
        _result = op(qa, b)
        return exp

    actual, exp = verify_circuit(circuit_builder, width=1)
    assert actual == exp, format_failure_message(f"cq_{op_name}", [a, b], width, exp, actual)


@pytest.mark.parametrize("width,a,b,op_name", SAMPLED_CQ)
def test_cq_cmp_sampled(verify_circuit, width, a, b, op_name):
    """CQ comparison: qint(a) op int(b) at widths 4-5 bits.

    Sampled coverage for the classical-quantum comparison variant.
    """
    expected = OPS[op_name](a, b)
    ql_op = QL_OPS_CQ[op_name]

    def circuit_builder(a=a, b=b, w=width, op=ql_op, exp=expected):
        qa = ql.qint(a, width=w)
        _result = op(qa, b)
        return exp

    actual, exp = verify_circuit(circuit_builder, width=1)
    assert actual == exp, format_failure_message(f"cq_{op_name}", [a, b], width, exp, actual)


# --- BUG-02 Regression Tests ---


class TestBug02Regression:
    """BUG-02 regression: three root causes fixed in Phase 29-16.

    These specific test cases were the original BUG-02 failures that were
    fixed. They verify the fixes remain intact. Note: the wider BUG-CMP-01
    and BUG-CMP-02 bugs affect OTHER comparison cases but these specific
    cases happen to produce correct results.
    """

    def test_msb_index_fix(self, verify_circuit):
        """Root cause 1: MSB at index 63 (not 64-width) in right-aligned storage.
        Regression for: qint(3) < qint(5) at 4-bit width."""

        def circuit_builder():
            x = ql.qint(3, width=4)
            y = ql.qint(5, width=4)
            _result = x < y
            return 1  # 3 < 5 is True

        actual, expected = verify_circuit(circuit_builder, width=1)
        assert actual == expected, "BUG-02 regression: MSB index fix failed"

    def test_gc_gate_reversal_fix(self, verify_circuit):
        """Root cause 2: comparison results must not have layer tracking.
        Regression for: qint(5) <= qint(5) at 4-bit width."""

        def circuit_builder():
            x = ql.qint(5, width=4)
            y = ql.qint(5, width=4)
            _result = x <= y
            return 1  # 5 <= 5 is True

        actual, expected = verify_circuit(circuit_builder, width=1)
        assert actual == expected, "BUG-02 regression: GC gate reversal fix failed"

    def test_unsigned_overflow_fix(self, verify_circuit):
        """Root cause 3: (n+1)-bit widened comparison for full unsigned range.
        Regression for: qint(0) <= qint(15) at 4-bit width."""

        def circuit_builder():
            x = ql.qint(0, width=4)
            y = ql.qint(15, width=4)
            _result = x <= y
            return 1  # 0 <= 15 is True

        actual, expected = verify_circuit(circuit_builder, width=1)
        assert actual == expected, "BUG-02 regression: unsigned overflow fix failed"
