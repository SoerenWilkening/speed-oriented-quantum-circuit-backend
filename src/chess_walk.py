"""Chess quantum walk register scaffolding and board state replay.

Provides register construction (one-hot height, per-level branch) and board
state derivation functions for the chess quantum walk demo. Uses raw qint
allocation and manual oracle replay -- no QWalkTree class.

Phase 104 Plan 01: Walk register infrastructure.
Phase 104 Plan 02: Local diffusion operator (separate module addition).
"""

from chess_encoding import get_legal_moves_and_oracle
from quantum_language._gates import emit_x
from quantum_language.qint import qint

__all__ = [
    "create_height_register",
    "create_branch_registers",
    "derive_board_state",
    "underive_board_state",
    "height_qubit",
    "prepare_walk_data",
]


def create_height_register(max_depth):
    """Create one-hot height register with root initialized to |1>.

    Parameters
    ----------
    max_depth : int
        Maximum tree depth. Register has max_depth+1 qubits.

    Returns
    -------
    qint
        Height register with root qubit (MSB, qubits[63]) set via emit_x.
    """
    h = qint(0, width=max_depth + 1)
    emit_x(int(h.qubits[63]))  # Root = MSB
    return h


def create_branch_registers(max_depth, move_data_per_level):
    """Create per-level branch registers from precomputed move data.

    Parameters
    ----------
    max_depth : int
        Maximum tree depth (number of branch registers to create).
    move_data_per_level : list[dict]
        Each dict must have 'branch_width' key giving qint width for that level.

    Returns
    -------
    list[qint]
        One branch register per level, width from move_data.
    """
    return [qint(0, width=move_data_per_level[i]["branch_width"]) for i in range(max_depth)]


def height_qubit(h_reg, depth, max_depth):
    """Get physical qubit index for a specific depth in the height register.

    Parameters
    ----------
    h_reg : qint
        Height register (width = max_depth + 1).
    depth : int
        Depth level to address (0 = leaves, max_depth = root).
    max_depth : int
        Maximum tree depth.

    Returns
    -------
    int
        Physical qubit index.
    """
    width = max_depth + 1
    return int(h_reg.qubits[64 - width + depth])


def derive_board_state(board_arrs, branch_regs, oracle_per_level, depth):
    """Apply move oracles 0..depth-1 to derive board state at given depth.

    Plain Python emitter -- calls each compiled oracle in forward order.

    Parameters
    ----------
    board_arrs : tuple
        (wk_arr, bk_arr, wn_arr) qarrays representing the board.
    branch_regs : list[qint]
        Branch registers, one per level.
    oracle_per_level : list
        Compiled apply_move functions, one per level.
    depth : int
        Number of oracle levels to apply.
    """
    wk, bk, wn = board_arrs
    for level_idx in range(depth):
        oracle_per_level[level_idx](wk, bk, wn, branch_regs[level_idx])


def underive_board_state(board_arrs, branch_regs, oracle_per_level, depth):
    """Uncompute board state by applying oracle inverses in reverse (LIFO).

    Parameters
    ----------
    board_arrs : tuple
        (wk_arr, bk_arr, wn_arr) qarrays representing the board.
    branch_regs : list[qint]
        Branch registers, one per level.
    oracle_per_level : list
        Compiled apply_move functions (must have .inverse property).
    depth : int
        Number of oracle levels to undo.
    """
    wk, bk, wn = board_arrs
    for level_idx in reversed(range(depth)):
        oracle_per_level[level_idx].inverse(wk, bk, wn, branch_regs[level_idx])


def prepare_walk_data(wk_sq, bk_sq, wn_squares, max_depth):
    """Precompute move data for each level of the walk tree.

    Alternates side_to_move: white at level 0 (root's children), black at
    level 1, white at level 2, etc.

    Parameters
    ----------
    wk_sq : int
        White king square (0-63).
    bk_sq : int
        Black king square (0-63).
    wn_squares : list[int]
        White knight square(s).
    max_depth : int
        Number of levels in the walk tree.

    Returns
    -------
    list[dict]
        One dict per level with keys: moves, move_count, branch_width, apply_move.
    """
    data = []
    for level in range(max_depth):
        side = "white" if level % 2 == 0 else "black"
        level_data = get_legal_moves_and_oracle(wk_sq, bk_sq, wn_squares, side)
        data.append(level_data)
    return data
