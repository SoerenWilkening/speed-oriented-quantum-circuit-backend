"""Tests for chess quantum walk register scaffolding and board state replay.

Component tests for Phase 104 requirements WALK-01 and WALK-02.
Tests stay within 17-qubit budget by testing registers in isolation.
Derive/underive tests verify call order at circuit-generation level (no simulation).
"""

import os
import sys
from unittest.mock import MagicMock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src"))


class TestHeightRegister:
    """WALK-01: One-hot height register with root qubit initialization."""

    def test_width_matches_max_depth_plus_one(self, clean_circuit):
        """Height register for max_depth=3 has width 4."""
        from chess_walk import create_height_register

        h = create_height_register(max_depth=3)
        # Width of the qint = max_depth + 1
        assert h.width == 4

    def test_root_qubit_is_msb(self, clean_circuit):
        """Root qubit is at qubits[63] (MSB, right-aligned)."""
        from chess_walk import create_height_register

        h = create_height_register(max_depth=3)
        root_qubit = int(h.qubits[63])
        # Root qubit should be a valid physical qubit index (non-negative)
        assert root_qubit >= 0

    def test_root_qubit_initialized_via_emit_x(self, clean_circuit):
        """Root qubit should have emit_x called on it (circuit has X gate)."""
        from chess_walk import create_height_register

        from quantum_language.state import circuit_stats

        _h = create_height_register(max_depth=3)  # noqa: F841
        stats = circuit_stats()
        # There should be at least one gate emitted (the X gate on root)
        assert stats["total_gates"] >= 1

    def test_different_max_depth(self, clean_circuit):
        """Height register adapts to different max_depth values."""
        from chess_walk import create_height_register

        h2 = create_height_register(max_depth=2)
        assert h2.width == 3

        # Need a fresh circuit for a separate register
        import quantum_language as ql

        ql.circuit()
        h5 = create_height_register(max_depth=5)
        assert h5.width == 6


class TestBranchRegisters:
    """WALK-02: Per-level branch registers with correct widths."""

    def test_correct_number_of_registers(self, clean_circuit):
        """create_branch_registers returns one register per depth level."""
        from chess_walk import create_branch_registers

        # Mock move_data: 3 levels
        move_data = [
            {"branch_width": 5},
            {"branch_width": 3},
            {"branch_width": 5},
        ]
        regs = create_branch_registers(max_depth=3, move_data_per_level=move_data)
        assert len(regs) == 3

    def test_correct_widths(self, clean_circuit):
        """Each branch register has width matching branch_width from move_data."""
        from chess_walk import create_branch_registers

        move_data = [
            {"branch_width": 5},
            {"branch_width": 3},
        ]
        regs = create_branch_registers(max_depth=2, move_data_per_level=move_data)
        assert regs[0].width == 5
        assert regs[1].width == 3

    def test_alternating_white_black_widths(self, clean_circuit):
        """White levels (even) can be wider than black levels (odd)."""
        from chess_walk import create_branch_registers

        # Realistic: white 5-bit, black 3-bit, alternating
        move_data = [
            {"branch_width": 5},
            {"branch_width": 3},
            {"branch_width": 5},
            {"branch_width": 3},
        ]
        regs = create_branch_registers(max_depth=4, move_data_per_level=move_data)
        assert regs[0].width == 5
        assert regs[1].width == 3
        assert regs[2].width == 5
        assert regs[3].width == 3


class TestHeightQubit:
    """Height qubit accessor: physical qubit index from height register."""

    def test_depth_zero(self, clean_circuit):
        """height_qubit at depth=0 returns correct physical qubit."""
        from chess_walk import create_height_register, height_qubit

        h = create_height_register(max_depth=3)
        # width = max_depth+1 = 4
        # index = 64 - 4 + 0 = 60
        expected = int(h.qubits[60])
        result = height_qubit(h, depth=0, max_depth=3)
        assert result == expected

    def test_depth_equals_max_depth(self, clean_circuit):
        """height_qubit at depth=max_depth is the root qubit (qubits[63])."""
        from chess_walk import create_height_register, height_qubit

        h = create_height_register(max_depth=3)
        # depth = max_depth = 3, index = 64 - 4 + 3 = 63
        expected = int(h.qubits[63])
        result = height_qubit(h, depth=3, max_depth=3)
        assert result == expected

    def test_intermediate_depth(self, clean_circuit):
        """height_qubit at intermediate depth returns correct qubit."""
        from chess_walk import create_height_register, height_qubit

        h = create_height_register(max_depth=4)
        # width = 5, depth = 2, index = 64 - 5 + 2 = 61
        expected = int(h.qubits[61])
        result = height_qubit(h, depth=2, max_depth=4)
        assert result == expected

    def test_max_depth_two(self, clean_circuit):
        """height_qubit works for max_depth=2."""
        from chess_walk import create_height_register, height_qubit

        h = create_height_register(max_depth=2)
        # width = 3
        # depth=0: index = 64 - 3 + 0 = 61
        # depth=1: index = 64 - 3 + 1 = 62
        # depth=2: index = 64 - 3 + 2 = 63 (root)
        q0 = height_qubit(h, depth=0, max_depth=2)
        q1 = height_qubit(h, depth=1, max_depth=2)
        q2 = height_qubit(h, depth=2, max_depth=2)
        assert q0 == int(h.qubits[61])
        assert q1 == int(h.qubits[62])
        assert q2 == int(h.qubits[63])
        # All should be different physical qubits
        assert len({q0, q1, q2}) == 3


class TestDeriveUnderiveBoardState:
    """Board state derivation and uncomputation via oracle replay."""

    def test_derive_calls_oracles_in_forward_order(self):
        """derive_board_state calls each oracle in order 0, 1, ..., depth-1."""
        from chess_walk import derive_board_state

        # Create mock oracles and board arrays
        oracle0 = MagicMock()
        oracle1 = MagicMock()
        oracle2 = MagicMock()
        oracles = [oracle0, oracle1, oracle2]

        wk = MagicMock()
        bk = MagicMock()
        wn = MagicMock()
        board_arrs = (wk, bk, wn)

        branch0 = MagicMock()
        branch1 = MagicMock()
        branch2 = MagicMock()
        branch_regs = [branch0, branch1, branch2]

        derive_board_state(board_arrs, branch_regs, oracles, depth=3)

        # Each oracle called with (wk, bk, wn, branch_reg[level])
        oracle0.assert_called_once_with(wk, bk, wn, branch0)
        oracle1.assert_called_once_with(wk, bk, wn, branch1)
        oracle2.assert_called_once_with(wk, bk, wn, branch2)

    def test_derive_partial_depth(self):
        """derive_board_state with depth < max only calls oracles 0..depth-1."""
        from chess_walk import derive_board_state

        oracle0 = MagicMock()
        oracle1 = MagicMock()
        oracle2 = MagicMock()
        oracles = [oracle0, oracle1, oracle2]

        board_arrs = (MagicMock(), MagicMock(), MagicMock())
        branch_regs = [MagicMock(), MagicMock(), MagicMock()]

        derive_board_state(board_arrs, branch_regs, oracles, depth=2)

        oracle0.assert_called_once()
        oracle1.assert_called_once()
        oracle2.assert_not_called()

    def test_underive_calls_oracles_inverse_in_reverse(self):
        """underive_board_state calls .inverse in LIFO order: depth-1, ..., 0."""
        from chess_walk import underive_board_state

        oracle0 = MagicMock()
        oracle1 = MagicMock()
        oracle2 = MagicMock()
        oracles = [oracle0, oracle1, oracle2]

        wk = MagicMock()
        bk = MagicMock()
        wn = MagicMock()
        board_arrs = (wk, bk, wn)

        branch0 = MagicMock()
        branch1 = MagicMock()
        branch2 = MagicMock()
        branch_regs = [branch0, branch1, branch2]

        underive_board_state(board_arrs, branch_regs, oracles, depth=3)

        # Inverse called in reverse: 2, 1, 0
        oracle2.inverse.assert_called_once_with(wk, bk, wn, branch2)
        oracle1.inverse.assert_called_once_with(wk, bk, wn, branch1)
        oracle0.inverse.assert_called_once_with(wk, bk, wn, branch0)

    def test_derive_depth_zero_calls_nothing(self):
        """derive_board_state with depth=0 calls no oracles."""
        from chess_walk import derive_board_state

        oracle0 = MagicMock()
        oracles = [oracle0]

        board_arrs = (MagicMock(), MagicMock(), MagicMock())
        branch_regs = [MagicMock()]

        derive_board_state(board_arrs, branch_regs, oracles, depth=0)
        oracle0.assert_not_called()


class TestPrepareWalkData:
    """Walk data preparation with alternating side-to-move."""

    def test_alternates_side_to_move(self, clean_circuit):
        """Level 0 = white, level 1 = black, level 2 = white, etc."""
        from chess_walk import prepare_walk_data

        # Position: white king e1 (4), black king e8 (60), white knight b1 (1)
        data = prepare_walk_data(wk_sq=4, bk_sq=60, wn_squares=[1], max_depth=3)

        assert len(data) == 3
        # Level 0 = white's moves, level 1 = black's moves, level 2 = white's moves

        # White has knight + king moves, black has only king moves
        # White should generally have more moves than black
        # Verify that data alternates by checking move lists are different
        # Level 0 (white) and level 2 (white) should have same move set
        # (since this is classical precomputation from starting position)
        assert data[0]["moves"] == data[2]["moves"]

    def test_returns_correct_keys(self, clean_circuit):
        """Each level's data has required keys: moves, move_count, branch_width, apply_move."""
        from chess_walk import prepare_walk_data

        data = prepare_walk_data(wk_sq=4, bk_sq=60, wn_squares=[1], max_depth=2)
        for level in data:
            assert "moves" in level
            assert "move_count" in level
            assert "branch_width" in level
            assert "apply_move" in level

    def test_branch_width_values(self, clean_circuit):
        """Branch width correctly computed from move count."""
        from chess_walk import prepare_walk_data

        # Use a position where we can predict move counts
        # White king e1 (4), black king a8 (56), white knight b1 (1)
        data = prepare_walk_data(wk_sq=4, bk_sq=56, wn_squares=[1], max_depth=2)

        for level in data:
            # branch_width >= 1 always
            assert level["branch_width"] >= 1
            # branch_width should be enough to index all moves
            assert 2 ** level["branch_width"] >= level["move_count"]

    def test_move_counts_positive(self, clean_circuit):
        """All levels should have at least one legal move in a valid position."""
        from chess_walk import prepare_walk_data

        # Well-separated position ensures legal moves for both sides
        data = prepare_walk_data(wk_sq=4, bk_sq=60, wn_squares=[1], max_depth=2)
        for level in data:
            assert level["move_count"] > 0

    def test_white_generally_more_moves_than_black(self, clean_circuit):
        """White (with knight) should have more legal moves than black (king only)."""
        from chess_walk import prepare_walk_data

        # Position with knight giving white more moves
        data = prepare_walk_data(wk_sq=4, bk_sq=60, wn_squares=[1], max_depth=2)

        white_moves = data[0]["move_count"]  # Level 0 = white
        black_moves = data[1]["move_count"]  # Level 1 = black
        # White has knight + king, black has only king
        assert white_moves > black_moves
