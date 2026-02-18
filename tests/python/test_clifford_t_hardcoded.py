"""Comprehensive tests for Clifford+T hardcoded sequence dispatch (Phase 75).

Verifies that toffoli_decompose=True with widths 1-8 uses pre-computed
Clifford+T sequences for CDKM (RCA) and BK CLA addition variants. Tests
gate purity (zero CCX/MCX), arithmetic correctness (equivalence with
non-decomposed path), T-count accuracy, and fallback behavior.

Test classes:
  TestCliffordTGatePurity  - Zero CCX/MCX in Clifford+T output
  TestCliffordTCorrectness - Results match non-decomposed path
  TestCliffordTTCount      - Exact T+Tdg count, 4:3 ratio
  TestCliffordTFallback    - Width 9+ dynamic path, CLA width-1 fallback
"""

import warnings

import pytest

import quantum_language as ql

# Suppress cosmetic warnings for values >= 2^(width-1) in unsigned interpretation
warnings.filterwarnings("ignore", message="Value .* exceeds")


def _setup_clifft():
    """Configure circuit for Clifford+T hardcoded sequence testing."""
    ql.option("fault_tolerant", True)
    ql.option("toffoli_decompose", True)


def _setup_nocliff():
    """Configure circuit for non-decomposed Toffoli testing."""
    ql.option("fault_tolerant", True)
    ql.option("toffoli_decompose", False)


# ============================================================================
# Gate purity: Clifford+T output must have zero CCX/MCX
# ============================================================================


class TestCliffordTGatePurity:
    """Verify that Clifford+T hardcoded sequences produce zero CCX/MCX gates."""

    @pytest.mark.parametrize("width", [1, 2, 3, 4])
    def test_cdkm_qq_purity(self, width):
        """CDKM QQ addition with toffoli_decompose=True: zero CCX, zero MCX."""
        c = ql.circuit()
        _setup_clifft()
        # Force CDKM (RCA) path by disabling CLA
        ql.option("cla", False)
        a = ql.qint(1, width)
        b = ql.qint(1, width)
        a += b
        gc = c.gate_counts
        assert gc.get("CCX", 0) == 0, f"CCX={gc.get('CCX', 0)} for width {width}"
        assert gc.get("MCX", 0) == 0, f"MCX={gc.get('MCX', 0)} for width {width}"
        if width >= 2:
            assert gc.get("T_gates", 0) > 0, f"Expected T gates for width {width}"
            assert gc.get("Tdg_gates", 0) > 0, f"Expected Tdg gates for width {width}"

    @pytest.mark.parametrize("width", [1, 2, 3, 4])
    def test_cdkm_cqq_purity(self, width):
        """Controlled CDKM QQ addition: zero CCX, zero MCX, T_gates > 0."""
        c = ql.circuit()
        _setup_clifft()
        ql.option("cla", False)
        a = ql.qint(1, width)
        b = ql.qint(1, width)
        ctrl = ql.qint(1, 1)
        with ctrl:
            a += b
        gc = c.gate_counts
        assert gc.get("CCX", 0) == 0, f"CCX={gc.get('CCX', 0)} for width {width}"
        assert gc.get("MCX", 0) == 0, f"MCX={gc.get('MCX', 0)} for width {width}"
        assert gc.get("T_gates", 0) > 0, f"Expected T gates for controlled width {width}"

    @pytest.mark.parametrize("width", [1, 2, 3, 4])
    def test_cdkm_cq_inc_purity(self, width):
        """CQ increment (a += 1) with toffoli_decompose=True: zero CCX."""
        c = ql.circuit()
        _setup_clifft()
        ql.option("cla", False)
        a = ql.qint(0, width)
        a += 1
        gc = c.gate_counts
        assert gc.get("CCX", 0) == 0, f"CCX={gc.get('CCX', 0)} for width {width}"
        assert gc.get("MCX", 0) == 0, f"MCX={gc.get('MCX', 0)} for width {width}"

    @pytest.mark.parametrize("width", [1, 2, 3, 4])
    def test_cdkm_ccq_inc_purity(self, width):
        """Controlled CQ increment inside with block: zero CCX."""
        c = ql.circuit()
        _setup_clifft()
        ql.option("cla", False)
        a = ql.qint(0, width)
        ctrl = ql.qint(1, 1)
        with ctrl:
            a += 1
        gc = c.gate_counts
        assert gc.get("CCX", 0) == 0, f"CCX={gc.get('CCX', 0)} for width {width}"
        assert gc.get("MCX", 0) == 0, f"MCX={gc.get('MCX', 0)} for width {width}"

    @pytest.mark.parametrize("width", [2, 3, 4])
    def test_cla_qq_purity(self, width):
        """BK CLA QQ addition with toffoli_decompose=True: zero CCX."""
        c = ql.circuit()
        _setup_clifft()
        ql.option("qubit_saving", True)  # BK variant
        a = ql.qint(1, width)
        b = ql.qint(1, width)
        a += b
        gc = c.gate_counts
        assert gc.get("CCX", 0) == 0, f"CCX={gc.get('CCX', 0)} for width {width}"
        assert gc.get("MCX", 0) == 0, f"MCX={gc.get('MCX', 0)} for width {width}"
        assert gc.get("T_gates", 0) > 0, f"Expected T gates for CLA width {width}"

    @pytest.mark.parametrize("width", [2, 3, 4])
    def test_cla_cqq_purity(self, width):
        """Controlled BK CLA QQ addition: zero CCX."""
        c = ql.circuit()
        _setup_clifft()
        ql.option("qubit_saving", True)  # BK variant
        a = ql.qint(1, width)
        b = ql.qint(1, width)
        ctrl = ql.qint(1, 1)
        with ctrl:
            a += b
        gc = c.gate_counts
        assert gc.get("CCX", 0) == 0, f"CCX={gc.get('CCX', 0)} for width {width}"
        assert gc.get("MCX", 0) == 0, f"MCX={gc.get('MCX', 0)} for width {width}"
        assert gc.get("T_gates", 0) > 0, f"Expected T gates for controlled CLA width {width}"


# ============================================================================
# Correctness: Clifford+T results must match non-decomposed path
# ============================================================================


class TestCliffordTCorrectness:
    """Verify Clifford+T sequences produce correct arithmetic results."""

    @pytest.mark.parametrize("width", [1, 2, 3, 4])
    def test_cdkm_qq_equivalence(self, width):
        """CDKM QQ addition: decomposed vs non-decomposed produce same results."""
        max_val = 1 << width
        for a_val in range(max_val):
            for b_val in range(max_val):
                # Non-decomposed
                ql.circuit()
                _setup_nocliff()
                ql.option("cla", False)
                a1 = ql.qint(a_val, width)
                b1 = ql.qint(b_val, width)
                a1 += b1
                res_off = a1.measure()

                # Decomposed
                ql.circuit()
                _setup_clifft()
                ql.option("cla", False)
                a2 = ql.qint(a_val, width)
                b2 = ql.qint(b_val, width)
                a2 += b2
                res_on = a2.measure()

                assert res_on == res_off, (
                    f"CDKM QQ w={width} {a_val}+{b_val}: on={res_on}, off={res_off}"
                )

    @pytest.mark.parametrize("width", [1, 2, 3, 4])
    def test_cdkm_cq_inc_equivalence(self, width):
        """CQ increment: decomposed vs non-decomposed produce same results."""
        max_val = 1 << width
        for a_val in range(max_val):
            # Non-decomposed
            ql.circuit()
            _setup_nocliff()
            ql.option("cla", False)
            a1 = ql.qint(a_val, width)
            a1 += 1
            res_off = a1.measure()

            # Decomposed
            ql.circuit()
            _setup_clifft()
            ql.option("cla", False)
            a2 = ql.qint(a_val, width)
            a2 += 1
            res_on = a2.measure()

            assert res_on == res_off, f"CQ inc w={width} {a_val}+1: on={res_on}, off={res_off}"

    @pytest.mark.parametrize("width", [2, 3, 4])
    def test_cla_qq_equivalence(self, width):
        """BK CLA QQ: decomposed vs non-decomposed produce same results."""
        max_val = 1 << width
        for a_val in range(max_val):
            for b_val in range(max_val):
                # Non-decomposed
                ql.circuit()
                _setup_nocliff()
                ql.option("qubit_saving", True)
                a1 = ql.qint(a_val, width)
                b1 = ql.qint(b_val, width)
                a1 += b1
                res_off = a1.measure()

                # Decomposed
                ql.circuit()
                _setup_clifft()
                ql.option("qubit_saving", True)
                a2 = ql.qint(a_val, width)
                b2 = ql.qint(b_val, width)
                a2 += b2
                res_on = a2.measure()

                assert res_on == res_off, (
                    f"CLA QQ w={width} {a_val}+{b_val}: on={res_on}, off={res_off}"
                )

    def test_subtraction_works_width_2(self):
        """Subtraction with Clifford+T at width 2: all input pairs."""
        width = 2
        max_val = 1 << width
        for a_val in range(max_val):
            for b_val in range(max_val):
                # Non-decomposed
                ql.circuit()
                _setup_nocliff()
                a1 = ql.qint(a_val, width)
                b1 = ql.qint(b_val, width)
                a1 -= b1
                res_off = a1.measure()

                # Decomposed
                ql.circuit()
                _setup_clifft()
                a2 = ql.qint(a_val, width)
                b2 = ql.qint(b_val, width)
                a2 -= b2
                res_on = a2.measure()

                assert res_on == res_off, f"Sub w=2 {a_val}-{b_val}: on={res_on}, off={res_off}"

    def test_subtraction_works_width_3(self):
        """Subtraction with Clifford+T at width 3: all input pairs."""
        width = 3
        max_val = 1 << width
        for a_val in range(max_val):
            for b_val in range(max_val):
                # Non-decomposed
                ql.circuit()
                _setup_nocliff()
                a1 = ql.qint(a_val, width)
                b1 = ql.qint(b_val, width)
                a1 -= b1
                res_off = a1.measure()

                # Decomposed
                ql.circuit()
                _setup_clifft()
                a2 = ql.qint(a_val, width)
                b2 = ql.qint(b_val, width)
                a2 -= b2
                res_on = a2.measure()

                assert res_on == res_off, f"Sub w=3 {a_val}-{b_val}: on={res_on}, off={res_off}"


# ============================================================================
# T-count accuracy: exact T+Tdg count, 4:3 ratio
# ============================================================================


class TestCliffordTTCount:
    """Verify T-count accuracy with Clifford+T hardcoded sequences."""

    @pytest.mark.parametrize("width", [2, 3, 4])
    def test_t_count_exact_cdkm_qq(self, width):
        """CDKM QQ Clifford+T: T-count is exact (T_gates + Tdg_gates)."""
        c = ql.circuit()
        _setup_clifft()
        ql.option("cla", False)
        a = ql.qint(1, width)
        b = ql.qint(1, width)
        a += b
        gc = c.gate_counts
        t_actual = gc.get("T_gates", 0) + gc.get("Tdg_gates", 0)
        assert gc["T"] == t_actual, (
            f"T-count {gc['T']} != T+Tdg {t_actual} for CDKM QQ width {width}"
        )
        assert t_actual > 0, f"Expected non-zero T-count for width {width}"
        # Verify 4:3 ratio (4T + 3Tdg per CCX)
        t_gates = gc.get("T_gates", 0)
        tdg_gates = gc.get("Tdg_gates", 0)
        assert 4 * tdg_gates == 3 * t_gates, f"T/Tdg ratio should be 4:3, got {t_gates}:{tdg_gates}"

    @pytest.mark.parametrize("width", [2, 3, 4])
    def test_t_count_exact_cla_qq(self, width):
        """BK CLA QQ Clifford+T: T-count is exact."""
        c = ql.circuit()
        _setup_clifft()
        ql.option("qubit_saving", True)
        a = ql.qint(1, width)
        b = ql.qint(1, width)
        a += b
        gc = c.gate_counts
        t_actual = gc.get("T_gates", 0) + gc.get("Tdg_gates", 0)
        assert gc["T"] == t_actual, (
            f"T-count {gc['T']} != T+Tdg {t_actual} for CLA QQ width {width}"
        )
        assert t_actual > 0, f"Expected non-zero T-count for CLA width {width}"
        # Verify 4:3 ratio
        t_gates = gc.get("T_gates", 0)
        tdg_gates = gc.get("Tdg_gates", 0)
        assert 4 * tdg_gates == 3 * t_gates, f"T/Tdg ratio should be 4:3, got {t_gates}:{tdg_gates}"

    def test_t_count_zero_for_width_1(self):
        """Width-1 QQ is single CX, no T gates."""
        c = ql.circuit()
        _setup_clifft()
        ql.option("cla", False)
        a = ql.qint(1, 1)
        b = ql.qint(1, 1)
        a += b
        gc = c.gate_counts
        assert gc.get("T_gates", 0) == 0, (
            f"Width-1 should have 0 T gates, got {gc.get('T_gates', 0)}"
        )
        assert gc.get("Tdg_gates", 0) == 0, (
            f"Width-1 should have 0 Tdg gates, got {gc.get('Tdg_gates', 0)}"
        )


# ============================================================================
# Fallback: width 9+ and CLA width-1
# ============================================================================


class TestCliffordTFallback:
    """Verify fallback behavior for widths outside hardcoded range."""

    def test_width_9_uses_dynamic_path(self):
        """Width 9 with toffoli_decompose=True: dynamic path still works.

        Width > 8 falls through to non-decomposed path. Verify equivalence.
        """
        # Non-decomposed
        ql.circuit()
        _setup_nocliff()
        ql.option("cla", False)
        a1 = ql.qint(5, 9)
        b1 = ql.qint(3, 9)
        a1 += b1
        res_off = a1.measure()

        # Decomposed (width 9 -> dynamic path fallback)
        ql.circuit()
        _setup_clifft()
        ql.option("cla", False)
        a2 = ql.qint(5, 9)
        b2 = ql.qint(3, 9)
        a2 += b2
        res_on = a2.measure()

        assert res_on == res_off, f"Width 9 dynamic path: on={res_on}, off={res_off}"

    def test_cla_width_1_falls_through(self):
        """Width 1 with CLA + toffoli_decompose=True: BK CLA returns NULL for
        width 1, should fall through to CDKM Clifford+T or regular path."""
        # Non-decomposed
        ql.circuit()
        _setup_nocliff()
        ql.option("qubit_saving", True)
        a1 = ql.qint(0, 1)
        b1 = ql.qint(1, 1)
        a1 += b1
        res_off = a1.measure()

        # Decomposed
        ql.circuit()
        _setup_clifft()
        ql.option("qubit_saving", True)
        a2 = ql.qint(0, 1)
        b2 = ql.qint(1, 1)
        a2 += b2
        res_on = a2.measure()

        assert res_on == res_off, f"CLA width 1 fallback: on={res_on}, off={res_off}"
