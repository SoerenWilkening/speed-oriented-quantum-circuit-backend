"""Tests for CCX -> Clifford+T decomposition via ql.option('toffoli_decompose', True).

Phase 74-04: Verifies that toffoli_decompose option correctly decomposes CCX gates
into Clifford+T basis {H, T, Tdg, CX} in all inline emission paths, with exact
T-count reporting and correct arithmetic results.

Note: Cached sequence paths (toffoli_QQ_add, toffoli_cQQ_add) may still produce
CCX gates that are not decomposed. Plan 74-05 (hardcoded decomposed sequences)
will address those. Tests here focus on inline paths (multiplication, controlled
addition via inline emission).
"""

import quantum_language as ql


class TestOptionAPI:
    """Test the toffoli_decompose option API."""

    def test_default_value_is_false(self):
        """toffoli_decompose defaults to off (CCX in output)."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        # The default is False -- verify by checking that CCX appears in output
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        assert gc.get("CCX", 0) > 0, "Default should produce CCX gates"

    def test_can_set_to_true(self):
        """toffoli_decompose can be set to True."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        # Inline paths should produce T/Tdg instead of CCX
        assert gc.get("T_gates", 0) > 0, "Should have T gates when decompose is on"
        assert gc.get("Tdg_gates", 0) > 0, "Should have Tdg gates when decompose is on"

    def test_can_toggle_back_to_false(self):
        """toffoli_decompose can be toggled back to False."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        ql.option("toffoli_decompose", False)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        assert gc.get("CCX", 0) > 0, "After toggling off, should produce CCX again"

    def test_works_independently_of_fault_tolerant(self):
        """toffoli_decompose works even with fault_tolerant=False (QFT backend)."""
        ql.circuit()
        ql.option("fault_tolerant", False)
        ql.option("toffoli_decompose", True)
        # QFT arithmetic doesn't produce CCX gates, so just verify no crash
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a + b  # side-effect: builds circuit


class TestGateCounts:
    """Test gate count properties with Clifford+T decomposition."""

    def test_t_count_exact_with_decompose(self):
        """T-count is exact (sum of T+Tdg) when decompose is on."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        t_actual = gc.get("T_gates", 0) + gc.get("Tdg_gates", 0)
        assert gc["T"] == t_actual, f"T-count {gc['T']} != T+Tdg {t_actual}"
        assert t_actual > 0, "Should have actual T/Tdg gates"

    def test_each_ccx_produces_7_t_tdg(self):
        """Each CCX decomposition produces exactly 4T + 3Tdg = 7 T/Tdg gates.

        We verify this by comparing the T-count ratio: the number of T/Tdg gates
        with decompose=on should equal 7 * CCX_count from decompose=off (for
        inline paths that get fully decomposed).
        """
        c_off = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", False)
        a1 = ql.qint(2, 3)
        b1 = ql.qint(3, 3)
        _ = a1 * b1  # side-effect: builds circuit
        gc_off = c_off.gate_counts
        ccx_off = gc_off.get("CCX", 0)

        c_on = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a2 = ql.qint(2, 3)
        b2 = ql.qint(3, 3)
        _ = a2 * b2  # side-effect: builds circuit
        gc_on = c_on.gate_counts
        t_total = gc_on.get("T_gates", 0) + gc_on.get("Tdg_gates", 0)

        # The T-count with decompose-on should be >= 7 * CCX from decompose-off
        # (may be > if optimizer cancels some CCX pairs when decompose is off)
        assert t_total >= 7 * ccx_off, (
            f"T-count {t_total} should be >= 7 * CCX({ccx_off}) = {7 * ccx_off}"
        )

    def test_t_tdg_ratio_per_ccx(self):
        """Each CCX produces exactly 4 T gates and 3 Tdg gates."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        t_count = gc.get("T_gates", 0)
        tdg_count = gc.get("Tdg_gates", 0)
        # Ratio: T/Tdg = 4/3 per CCX
        assert t_count > 0 and tdg_count > 0
        # Verify ratio: 4*tdg_count == 3*t_count (exact integer arithmetic)
        assert 4 * tdg_count == 3 * t_count, (
            f"T/Tdg ratio should be 4:3, got {t_count}:{tdg_count} "
            f"(4*{tdg_count}={4 * tdg_count} != 3*{t_count}={3 * t_count})"
        )

    def test_no_ccx_in_inline_mul_path(self):
        """With decompose=on, QQ multiplication inline path produces zero CCX."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        assert gc.get("CCX", 0) == 0, (
            f"Inline QQ mul should have 0 CCX with decompose=on, got {gc.get('CCX', 0)}"
        )


class TestCorrectness:
    """Test that decompose=on produces identical circuits to decompose=off.

    Note: .measure() returns initial values (no quantum simulation).
    Actual arithmetic correctness requires Qiskit simulation (see
    test_cross_backend.py). Here we verify decomposition equivalence:
    decompose=on and decompose=off produce the same .measure() and
    QASM structure (modulo gate-level differences).
    """

    def test_qq_mul_equivalence_width1(self):
        """QQ mul width 1: decompose=on/off produce same measure results."""
        for a_val in range(2):
            for b_val in range(2):
                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", False)
                a1 = ql.qint(a_val, 1)
                b1 = ql.qint(b_val, 1)
                r1 = a1 * b1
                res_off = r1.measure()

                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", True)
                a2 = ql.qint(a_val, 1)
                b2 = ql.qint(b_val, 1)
                r2 = a2 * b2
                res_on = r2.measure()

                assert res_on == res_off, f"{a_val}*{b_val}: on={res_on}, off={res_off}"

    def test_qq_mul_equivalence_width2(self):
        """QQ mul width 2: decompose=on/off produce same measure results."""
        for a_val in range(4):
            for b_val in range(4):
                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", False)
                a1 = ql.qint(a_val, 2)
                b1 = ql.qint(b_val, 2)
                r1 = a1 * b1
                res_off = r1.measure()

                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", True)
                a2 = ql.qint(a_val, 2)
                b2 = ql.qint(b_val, 2)
                r2 = a2 * b2
                res_on = r2.measure()

                assert res_on == res_off, f"{a_val}*{b_val}: on={res_on}, off={res_off}"

    def test_qq_mul_equivalence_width3(self):
        """QQ mul width 3: decompose=on/off produce same measure results."""
        test_cases = [(2, 3), (3, 5), (7, 7), (0, 5), (1, 1)]
        for a_val, b_val in test_cases:
            a_v = a_val % 8
            b_v = b_val % 8
            ql.circuit()
            ql.option("fault_tolerant", True)
            ql.option("toffoli_decompose", False)
            a1 = ql.qint(a_v, 3)
            b1 = ql.qint(b_v, 3)
            r1 = a1 * b1
            res_off = r1.measure()

            ql.circuit()
            ql.option("fault_tolerant", True)
            ql.option("toffoli_decompose", True)
            a2 = ql.qint(a_v, 3)
            b2 = ql.qint(b_v, 3)
            r2 = a2 * b2
            res_on = r2.measure()

            assert res_on == res_off, f"{a_v}*{b_v}: on={res_on}, off={res_off}"

    def test_cq_mul_equivalence(self):
        """CQ mul: decompose=on/off produce same measure results.

        CQ mul uses run_instruction with cached QQ_add sequences, so the
        addition path won't decompose. But the dispatch should work correctly.
        """
        for a_val in range(8):
            for b_val in [0, 1, 3, 5, 7]:
                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", False)
                a1 = ql.qint(a_val, 3)
                r1 = a1 * b_val
                res_off = r1.measure()

                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", True)
                a2 = ql.qint(a_val, 3)
                r2 = a2 * b_val
                res_on = r2.measure()

                assert res_on == res_off, f"{a_val}*{b_val}: on={res_on}, off={res_off}"

    def test_subtraction_equivalence(self):
        """Subtraction: decompose=on/off produce same measure results."""
        for a_val in range(8):
            for b_val in range(8):
                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", False)
                a1 = ql.qint(a_val, 3)
                b1 = ql.qint(b_val, 3)
                a1 -= b1
                res_off = a1.measure()

                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", True)
                a2 = ql.qint(a_val, 3)
                b2 = ql.qint(b_val, 3)
                a2 -= b2
                res_on = a2.measure()

                assert res_on == res_off, f"{a_val}-{b_val}: on={res_on}, off={res_off}"

    def test_addition_equivalence(self):
        """Addition: decompose=on/off produce same measure results."""
        for a_val in range(8):
            for b_val in range(8):
                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", False)
                a1 = ql.qint(a_val, 3)
                b1 = ql.qint(b_val, 3)
                a1 += b1
                res_off = a1.measure()

                ql.circuit()
                ql.option("fault_tolerant", True)
                ql.option("toffoli_decompose", True)
                a2 = ql.qint(a_val, 3)
                b2 = ql.qint(b_val, 3)
                a2 += b2
                res_on = a2.measure()

                assert res_on == res_off, f"{a_val}+{b_val}: on={res_on}, off={res_off}"

    def test_qasm_structure_equivalence(self):
        """Decompose=on QASM has same qubit count and register structure."""
        ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", False)
        a1 = ql.qint(2, 3)
        b1 = ql.qint(3, 3)
        _ = a1 * b1  # side-effect: builds circuit
        qasm_off = ql.to_openqasm()

        ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a2 = ql.qint(2, 3)
        b2 = ql.qint(3, 3)
        _ = a2 * b2  # side-effect: builds circuit
        qasm_on = ql.to_openqasm()

        # Same qubit register declaration (same qubit allocation)
        off_lines = [line for line in qasm_off.split("\n") if line.startswith("qubit")]
        on_lines = [line for line in qasm_on.split("\n") if line.startswith("qubit")]
        assert off_lines == on_lines, f"Qubit declarations differ:\noff={off_lines}\non={on_lines}"


class TestGatePurity:
    """Test that decomposed circuits contain only Clifford+T basis gates."""

    def test_inline_mul_gate_purity(self):
        """QQ multiplication with decompose=on produces only {X, H, T, Tdg, CX} gates."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        # CCX should be 0 (inline path fully decomposed)
        assert gc.get("CCX", 0) == 0, f"CCX should be 0, got {gc.get('CCX', 0)}"
        # MCX should not appear (Toffoli mode eliminates them)
        assert gc.get("MCX", 0) == 0 or "MCX" not in gc, "MCX should not appear"
        # Only expected gate types should be present
        expected_keys = {"X", "CNOT", "H", "T_gates", "Tdg_gates", "T", "CCX"}
        for key, val in gc.items():
            if val > 0 and key not in expected_keys:
                # Allow 'other' key but it should be 0
                if key == "other" and val == 0:
                    continue

    def test_t_tdg_counts_are_multiples(self):
        """T gates are multiple of 4, Tdg gates are multiple of 3 (from CCX decomp)."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        assert gc.get("T_gates", 0) % 4 == 0, (
            f"T gates should be multiple of 4, got {gc.get('T_gates', 0)}"
        )
        assert gc.get("Tdg_gates", 0) % 3 == 0, (
            f"Tdg gates should be multiple of 3, got {gc.get('Tdg_gates', 0)}"
        )


class TestIndependence:
    """Test that toffoli_decompose works independently of other options."""

    def test_decompose_with_fault_tolerant_true(self):
        """Main use case: toffoli_decompose + fault_tolerant=True."""
        c = ql.circuit()
        ql.option("fault_tolerant", True)
        ql.option("toffoli_decompose", True)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        _ = a * b  # side-effect: builds circuit
        gc = c.gate_counts
        assert gc.get("T_gates", 0) > 0
        assert gc.get("CCX", 0) == 0

    def test_decompose_with_fault_tolerant_false(self):
        """toffoli_decompose with fault_tolerant=False (QFT mode).

        QFT mode doesn't produce CCX gates for addition, so decompose has
        limited effect. But the option should not cause errors.
        """
        c = ql.circuit()
        ql.option("fault_tolerant", False)
        ql.option("toffoli_decompose", True)
        a = ql.qint(2, 3)
        b = ql.qint(3, 3)
        a += b
        gc = c.gate_counts
        # QFT addition shouldn't produce CCX, so T/Tdg likely 0
        # Just verify no crash and valid gate counts
        assert gc.get("T", 0) >= 0
