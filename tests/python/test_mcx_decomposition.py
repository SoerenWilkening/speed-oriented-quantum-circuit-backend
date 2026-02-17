"""Tests for Phase 74-03: MCX Gate Purity in Toffoli-mode Operations

Verifies that ALL Toffoli-mode operations emit only X/CX/CCX gates (plus
rotation gates for QFT), with zero MCX (3+ control) gates in the output.

The 'other' count in gate_counts tracks gates with 3+ controls (MCX).
After Phase 74-03 AND-ancilla decomposition, this count must be zero for
all Toffoli arithmetic operations.

Test coverage:
  - Uncontrolled Toffoli addition (QQ, CQ) -- various widths
  - Controlled Toffoli addition (cQQ, cCQ) -- various widths
  - Uncontrolled Toffoli multiplication (QQ, CQ)
  - Controlled Toffoli multiplication (cmul_qq, cmul_cq)
  - Equality comparison (CQ_equal_width, cCQ_equal_width) -- various widths
  - CLA adder (Brent-Kung) controlled path
"""

import quantum_language as ql


def _get_mcx_count(circ):
    """Return the number of MCX (3+ control) gates in the given circuit."""
    gc = circ.gate_counts
    return gc.get("other", 0)


# ============================================================================
# Toffoli Addition: Uncontrolled
# ============================================================================


class TestToffoliAddQQNoMCX:
    """Uncontrolled QQ Toffoli addition must produce zero MCX gates."""

    def test_add_qq_width_2(self):
        """Width-2 QQ addition: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=2)
        b = ql.qint(1, width=2)
        a += b
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_add_qq_width_3(self):
        """Width-3 QQ addition: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=3)
        b = ql.qint(1, width=3)
        a += b
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_add_qq_width_4(self):
        """Width-4 QQ addition (CLA threshold): no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=4)
        b = ql.qint(1, width=4)
        a += b
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"


class TestToffoliAddCQNoMCX:
    """Uncontrolled CQ Toffoli addition must produce zero MCX gates."""

    def test_add_cq_width_3(self):
        """Width-3 CQ addition: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(0, width=3)
        a += 3
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"


# ============================================================================
# Toffoli Addition: Controlled
# ============================================================================


class TestControlledToffoliAddNoMCX:
    """Controlled Toffoli addition must produce zero MCX gates.

    These are the primary targets of Phase 74-03 Task 1 decomposition:
    cQQ_add and cCQ_add previously contained MCX(3-control) gates.
    """

    def test_controlled_add_qq_width_2(self):
        """Controlled QQ addition width-2: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=2)
        b = ql.qint(1, width=2)
        ctrl = ql.qbool(True)
        with ctrl:
            a += b
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_controlled_add_qq_width_3(self):
        """Controlled QQ addition width-3: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=3)
        b = ql.qint(1, width=3)
        ctrl = ql.qbool(True)
        with ctrl:
            a += b
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_controlled_add_cq_width_2(self):
        """Controlled CQ addition width-2: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(0, width=2)
        ctrl = ql.qbool(True)
        with ctrl:
            a += 1
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_controlled_add_cq_width_3(self):
        """Controlled CQ addition width-3: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(0, width=3)
        ctrl = ql.qbool(True)
        with ctrl:
            a += 2
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"


# ============================================================================
# Toffoli Multiplication: Uncontrolled
# ============================================================================


class TestToffoliMulNoMCX:
    """Uncontrolled Toffoli multiplication must produce zero MCX gates."""

    def test_mul_qq_width_2(self):
        """Width-2 QQ multiplication: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=2)
        b = ql.qint(1, width=2)
        _ = a * b
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_mul_cq_width_2(self):
        """Width-2 CQ multiplication: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=2)
        _ = a * 2
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"


# ============================================================================
# Toffoli Multiplication: Controlled
# ============================================================================


class TestControlledToffoliMulNoMCX:
    """Controlled Toffoli multiplication must produce zero MCX gates.

    toffoli_cmul_qq width-1 case previously had MCX(3-control).
    Phase 74-03 Task 2 decomposed it to 3 CCX.
    """

    def test_controlled_mul_qq_width_2(self):
        """Controlled QQ multiplication width-2: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=2)
        b = ql.qint(1, width=2)
        ctrl = ql.qbool(True)
        with ctrl:
            _ = a * b
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_controlled_mul_cq_width_2(self):
        """Controlled CQ multiplication width-2: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        a = ql.qint(1, width=2)
        ctrl = ql.qbool(True)
        with ctrl:
            _ = a * 3
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"


# ============================================================================
# Equality Comparison: CQ_equal_width
# ============================================================================


class TestEqualityCQNoMCX:
    """CQ equality comparison must produce zero MCX gates.

    For bits >= 3, CQ_equal_width previously used MCX(bits controls).
    Phase 74-03 Task 2 decomposed to recursive AND-ancilla CCX.
    """

    def test_equality_cq_width_1(self):
        """Width-1 equality: uses CX, no MCX possible."""
        c = ql.circuit()
        a = ql.qint(1, width=1)
        _ = a == 1
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_equality_cq_width_2(self):
        """Width-2 equality: uses CCX, no MCX needed."""
        c = ql.circuit()
        a = ql.qint(1, width=2)
        _ = a == 1
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_equality_cq_width_3(self):
        """Width-3 equality: MCX(3) decomposed to AND-ancilla CCX."""
        c = ql.circuit()
        a = ql.qint(1, width=3)
        _ = a == 1
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_equality_cq_width_4(self):
        """Width-4 equality: MCX(4) decomposed recursively."""
        c = ql.circuit()
        a = ql.qint(5, width=4)
        _ = a == 5
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_equality_cq_width_8(self):
        """Width-8 equality: MCX(8) decomposed recursively."""
        c = ql.circuit()
        a = ql.qint(42, width=8)
        _ = a == 42
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_equality_cq_all_zeros(self):
        """Equality with zero (all bits need X flip): no MCX."""
        c = ql.circuit()
        a = ql.qint(0, width=4)
        _ = a == 0
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_equality_cq_all_ones(self):
        """Equality with max value (no X flips needed): no MCX."""
        c = ql.circuit()
        a = ql.qint(15, width=4)
        _ = a == 15
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"


# ============================================================================
# Equality Comparison: Controlled (cCQ_equal_width)
# ============================================================================


class TestControlledEqualityNoMCX:
    """Controlled equality comparison must produce zero MCX gates.

    For bits >= 2, cCQ_equal_width previously used MCX(bits+1 controls).
    Phase 74-03 Task 2 decomposed to recursive AND-ancilla CCX.
    """

    def test_controlled_equality_width_2(self):
        """Controlled width-2 equality: MCX(3) decomposed."""
        c = ql.circuit()
        a = ql.qint(1, width=2)
        ctrl = ql.qbool(True)
        with ctrl:
            _ = a == 1
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_controlled_equality_width_3(self):
        """Controlled width-3 equality: MCX(4) decomposed."""
        c = ql.circuit()
        a = ql.qint(3, width=3)
        ctrl = ql.qbool(True)
        with ctrl:
            _ = a == 3
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"

    def test_controlled_equality_width_4(self):
        """Controlled width-4 equality: MCX(5) decomposed."""
        c = ql.circuit()
        a = ql.qint(7, width=4)
        ctrl = ql.qbool(True)
        with ctrl:
            _ = a == 7
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"


# ============================================================================
# CLA (Carry Look-Ahead) Controlled Path
# ============================================================================


class TestCLAControlledNoMCX:
    """Controlled CLA (Brent-Kung) addition must produce zero MCX gates.

    CLA is dispatched automatically for width >= 4 (unless cla=False).
    Controlled BK CLA previously injected ext_ctrl creating MCX(3) gates.
    """

    def test_cla_controlled_width_4(self):
        """Controlled CLA width-4: no MCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("cla", True)
        a = ql.qint(1, width=4)
        b = ql.qint(1, width=4)
        ctrl = ql.qbool(True)
        with ctrl:
            a += b
        assert _get_mcx_count(c) == 0, f"MCX gates found: {_get_mcx_count(c)}"
