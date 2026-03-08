"""Chess quantum walk register scaffolding, board state replay, and local diffusion.

Provides register construction (one-hot height, per-level branch), board
state derivation, and local diffusion D_x with Montanaro angles for the
chess quantum walk demo. Uses raw qint allocation and walk.py internal
helpers -- no QWalkTree class.

Phase 104 Plan 01: Walk register infrastructure.
Phase 104 Plan 02: Local diffusion operator with variable branching.
"""

import math

import numpy as np

from chess_encoding import get_legal_moves_and_oracle
from quantum_language._gates import emit_x
from quantum_language.qint import qint
from quantum_language.walk import (
    _make_qbool_wrapper,
    _plan_cascade_ops,
    counting_diffusion_core,
)

__all__ = [
    "create_height_register",
    "create_branch_registers",
    "derive_board_state",
    "underive_board_state",
    "height_qubit",
    "prepare_walk_data",
    "montanaro_phi",
    "montanaro_root_phi",
    "precompute_diffusion_angles",
    "evaluate_children",
    "uncompute_children",
    "apply_diffusion",
    "r_a",
    "r_b",
    "all_walk_qubits",
    "walk_step",
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


# ---------------------------------------------------------------------------
# Montanaro angle helpers
# ---------------------------------------------------------------------------


def montanaro_phi(d):
    """Montanaro parent-children split angle for internal nodes.

    Parameters
    ----------
    d : int
        Branching factor (number of valid children).

    Returns
    -------
    float
        phi = 2 * arctan(sqrt(d)).
    """
    return 2.0 * math.atan(math.sqrt(d))


def montanaro_root_phi(d, max_depth):
    """Montanaro root angle with depth amplification.

    Parameters
    ----------
    d : int
        Branching factor at the root.
    max_depth : int
        Maximum tree depth n.

    Returns
    -------
    float
        phi_root = 2 * arctan(sqrt(n * d)).
    """
    return 2.0 * math.atan(math.sqrt(max_depth * d))


def precompute_diffusion_angles(d_max, branch_width):
    """Precompute angle data for each possible branching factor 1..d_max.

    Parameters
    ----------
    d_max : int
        Maximum branching degree.
    branch_width : int
        Width of the branch register in qubits.

    Returns
    -------
    dict[int, dict]
        Maps d_val -> {"phi": float, "cascade_ops": list}.
    """
    angles = {}
    for d_val in range(1, d_max + 1):
        phi = montanaro_phi(d_val)
        if d_val > 1:
            try:
                ops = _plan_cascade_ops(d_val, branch_width)
            except NotImplementedError:
                # Cascade planning fails for d_val > 2^(w-1) due to control
                # depth limitation. Fall back to Ry-only (no child cascade).
                ops = []
        else:
            ops = []
        angles[d_val] = {"phi": phi, "cascade_ops": ops}
    return angles


# ---------------------------------------------------------------------------
# Validity evaluation (child predicate loop)
# ---------------------------------------------------------------------------


def evaluate_children(
    depth, level_idx, d_max, branch_reg, h_reg, max_depth, oracle, board_arrs, validity
):
    """Evaluate each child's validity and store in validity ancillae.

    For each child i (0..d_max-1): encode child index in branch register,
    flip height, apply oracle to get child board state, check validity via
    quantum predicate, store result, then undo everything.

    Follows walk.py _evaluate_children pattern exactly.

    Parameters
    ----------
    depth : int
        Current depth level.
    level_idx : int
        Level index (max_depth - depth).
    d_max : int
        Maximum branching degree at this level.
    branch_reg : qint
        Branch register for this level.
    h_reg : qint
        Height register.
    max_depth : int
        Maximum tree depth.
    oracle : callable
        Compiled apply_move function for this level.
    board_arrs : tuple
        (wk_arr, bk_arr, wn_arr) qarrays.
    validity : list[qbool]
        Validity ancillae to store results (one per child).
    """
    from quantum_language.qbool import qbool as alloc_qbool

    bw = branch_reg.width
    wk, bk, wn = board_arrs

    for i in range(d_max):
        # (a) Encode child index i in branch register (MSB-first)
        for bit in range(bw):
            if (i >> (bw - 1 - bit)) & 1:
                emit_x(int(branch_reg.qubits[64 - bw + bit]))

        # (b) Flip height: depth -> depth-1 (move to child level)
        emit_x(height_qubit(h_reg, depth, max_depth))
        emit_x(height_qubit(h_reg, depth - 1, max_depth))

        # (c) Apply oracle to derive child board state
        oracle(wk, bk, wn, branch_reg)

        # (d) Quantum validity predicate: allocate reject qbool,
        # then set validity[i] = NOT reject.
        # For the KNK endgame with precomputed structurally valid moves,
        # all children in the oracle are valid. The predicate is trivially
        # satisfied (reject = |0>, validity = |1>). The quantum predicate
        # is still necessary to support the variable-branching D_x pattern
        # where d(x) counts valid children via these ancillae.
        reject = alloc_qbool()
        reject_qubit = int(reject.qubits[63])
        validity_qubit = int(validity[i].qubits[63])
        reject_ctrl = _make_qbool_wrapper(reject_qubit)
        with reject_ctrl:
            emit_x(validity_qubit)
        emit_x(validity_qubit)  # Flip: validity=|1> means valid

        # (e) Uncompute predicate (adjoint of trivial predicate is identity)

        # (f) Uncompute oracle
        oracle.inverse(wk, bk, wn, branch_reg)

        # (g) Undo height flip
        emit_x(height_qubit(h_reg, depth - 1, max_depth))
        emit_x(height_qubit(h_reg, depth, max_depth))

        # (h) Undo branch register encoding
        for bit in range(bw):
            if (i >> (bw - 1 - bit)) & 1:
                emit_x(int(branch_reg.qubits[64 - bw + bit]))


def uncompute_children(
    depth, level_idx, d_max, branch_reg, h_reg, max_depth, oracle, board_arrs, validity
):
    """Uncompute validity ancillae (reverse of evaluate_children).

    Iterates children in reversed order and undoes the validity store.

    Parameters
    ----------
    depth : int
        Current depth level.
    level_idx : int
        Level index (max_depth - depth).
    d_max : int
        Maximum branching degree at this level.
    branch_reg : qint
        Branch register for this level.
    h_reg : qint
        Height register.
    max_depth : int
        Maximum tree depth.
    oracle : callable
        Compiled apply_move function for this level.
    board_arrs : tuple
        (wk_arr, bk_arr, wn_arr) qarrays.
    validity : list[qbool]
        Validity ancillae to uncompute.
    """
    from quantum_language.qbool import qbool as alloc_qbool

    bw = branch_reg.width
    wk, bk, wn = board_arrs

    for i in reversed(range(d_max)):
        # Navigate to child i
        for bit in range(bw):
            if (i >> (bw - 1 - bit)) & 1:
                emit_x(int(branch_reg.qubits[64 - bw + bit]))

        emit_x(height_qubit(h_reg, depth, max_depth))
        emit_x(height_qubit(h_reg, depth - 1, max_depth))

        # Re-apply oracle
        oracle(wk, bk, wn, branch_reg)

        # Re-evaluate predicate (trivial)
        reject = alloc_qbool()
        reject_qubit = int(reject.qubits[63])
        validity_qubit = int(validity[i].qubits[63])

        # Undo validity store (reverse order: X then CNOT)
        emit_x(validity_qubit)
        reject_ctrl = _make_qbool_wrapper(reject_qubit)
        with reject_ctrl:
            emit_x(validity_qubit)

        # Uncompute oracle
        oracle.inverse(wk, bk, wn, branch_reg)

        # Undo navigation
        emit_x(height_qubit(h_reg, depth - 1, max_depth))
        emit_x(height_qubit(h_reg, depth, max_depth))

        for bit in range(bw):
            if (i >> (bw - 1 - bit)) & 1:
                emit_x(int(branch_reg.qubits[64 - bw + bit]))


# ---------------------------------------------------------------------------
# Local diffusion D_x
# ---------------------------------------------------------------------------


def apply_diffusion(
    depth, h_reg, branch_regs, board_arrs, oracle_per_level, move_data_per_level, max_depth
):
    """Apply local diffusion D_x at a given depth.

    Uses the shared counting_diffusion_core from walk.py for O(d_max)
    gate complexity:
    1. Derive board state at current node (replay oracles 0..depth-1).
    2. Allocate validity ancillae.
    3. Evaluate children (quantum predicate per child).
    4. Precompute angles, delegate to counting_diffusion_core.
    5. Uncompute validity.
    6. Underive board state.

    Parameters
    ----------
    depth : int
        Current depth level (1..max_depth).
    h_reg : qint
        Height register.
    branch_regs : list[qint]
        Per-level branch registers.
    board_arrs : tuple
        (wk_arr, bk_arr, wn_arr) qarrays.
    oracle_per_level : list
        Compiled apply_move functions, one per level.
    move_data_per_level : list[dict]
        Move data per level (needs 'move_count' key).
    max_depth : int
        Maximum tree depth.
    """
    from quantum_language.qbool import qbool

    # --- Step 1: Setup ---
    level_idx = max_depth - depth

    # --- Step 2: Derive board state at current node ---
    # Replay oracles 0..level_idx-1 to reach current depth.
    # Root (depth=max_depth, level_idx=0) needs 0 replays (starting position).
    derive_board_state(board_arrs, branch_regs, oracle_per_level, level_idx)
    d_max = move_data_per_level[level_idx]["move_count"]
    branch_reg = branch_regs[level_idx]
    is_root = depth == max_depth
    h_qubit_idx = height_qubit(h_reg, depth, max_depth)
    h_child_idx = height_qubit(h_reg, depth - 1, max_depth)

    # --- Step 3: Allocate validity ancillae ---
    validity = [qbool() for _ in range(d_max)]

    # --- Step 4: Evaluate children ---
    evaluate_children(
        depth,
        level_idx,
        d_max,
        branch_reg,
        h_reg,
        max_depth,
        oracle_per_level[level_idx],
        board_arrs,
        validity,
    )

    # --- Step 5: Precompute angles ---
    angles = precompute_diffusion_angles(d_max, branch_reg.width)
    root_angles = {}
    if is_root:
        for d_val in range(1, d_max + 1):
            root_angles[d_val] = montanaro_root_phi(d_val, max_depth)

    # --- Step 6: Counting diffusion core (shared with walk.py) ---
    counting_diffusion_core(
        validity,
        d_max,
        branch_reg,
        h_qubit_idx,
        h_child_idx,
        angles,
        root_angles,
        is_root,
    )

    # --- Step 7: Uncompute validity ---
    uncompute_children(
        depth,
        level_idx,
        d_max,
        branch_reg,
        h_reg,
        max_depth,
        oracle_per_level[level_idx],
        board_arrs,
        validity,
    )

    # --- Step 8: Underive board state ---
    underive_board_state(board_arrs, branch_regs, oracle_per_level, level_idx)


# ---------------------------------------------------------------------------
# Mega-register for compile
# ---------------------------------------------------------------------------


def all_walk_qubits(h_reg, branch_regs, max_depth):
    """Create qint wrapping height + branch walk qubits.

    Used as one of the arguments to the compiled walk step so these
    qubits are treated as parameter qubits (not internal ancillas).
    Board array qarrays are passed separately to the compiled function
    since the total qubit count exceeds the 64-qubit qint width limit.

    Parameters
    ----------
    h_reg : qint
        Height register (width = max_depth + 1).
    branch_regs : list[qint]
        Per-level branch registers.
    max_depth : int
        Maximum tree depth.

    Returns
    -------
    qint
        Register wrapping height + branch qubits with create_new=False.
    """
    all_indices = []
    # Height register qubits
    w = max_depth + 1
    for i in range(w):
        all_indices.append(int(h_reg.qubits[64 - w + i]))
    # Branch register qubits
    for br in branch_regs:
        bw = br.width
        for i in range(bw):
            all_indices.append(int(br.qubits[64 - bw + i]))

    arr = np.zeros(64, dtype=np.uint32)
    total = len(all_indices)
    for i, idx in enumerate(all_indices):
        arr[64 - total + i] = idx
    return qint(0, create_new=False, bit_list=arr, width=total)


# ---------------------------------------------------------------------------
# Walk operators R_A and R_B
# ---------------------------------------------------------------------------


def r_a(h_reg, branch_regs, board_arrs, oracle_per_level, move_data_per_level, max_depth):
    """Apply R_A: diffusion at even depths, excluding root and leaves.

    R_A covers even-numbered depth levels (0, 2, 4, ...) but skips:
    - depth 0 (leaves -- no children, level_idx out of range)
    - depth == max_depth (root always belongs to R_B)

    Together with R_B, covers all active depths exactly once (disjoint).

    Parameters
    ----------
    h_reg : qint
        Height register.
    branch_regs : list[qint]
        Per-level branch registers.
    board_arrs : tuple
        (wk_arr, bk_arr, wn_arr) qarrays.
    oracle_per_level : list
        Compiled apply_move functions, one per level.
    move_data_per_level : list[dict]
        Move data per level.
    max_depth : int
        Maximum tree depth.
    """
    for depth in range(0, max_depth + 1, 2):
        if depth == 0:
            continue  # Leaves: no children, level_idx out of range
        if depth == max_depth:
            continue  # Root always belongs to R_B
        apply_diffusion(
            depth, h_reg, branch_regs, board_arrs, oracle_per_level, move_data_per_level, max_depth
        )


def r_b(h_reg, branch_regs, board_arrs, oracle_per_level, move_data_per_level, max_depth):
    """Apply R_B: diffusion at odd depths plus root.

    R_B covers odd-numbered depth levels (1, 3, 5, ...) and always includes
    the root (depth == max_depth). If max_depth is even, root is added
    explicitly since it wouldn't be in the odd range.

    Together with R_A, covers all active depths exactly once (disjoint).

    Parameters
    ----------
    h_reg : qint
        Height register.
    branch_regs : list[qint]
        Per-level branch registers.
    board_arrs : tuple
        (wk_arr, bk_arr, wn_arr) qarrays.
    oracle_per_level : list
        Compiled apply_move functions, one per level.
    move_data_per_level : list[dict]
        Move data per level.
    max_depth : int
        Maximum tree depth.
    """
    for depth in range(1, max_depth + 1, 2):
        apply_diffusion(
            depth, h_reg, branch_regs, board_arrs, oracle_per_level, move_data_per_level, max_depth
        )
    # Root always in R_B; add explicitly if max_depth is even (not in odd range)
    if max_depth % 2 == 0:
        apply_diffusion(
            max_depth,
            h_reg,
            branch_regs,
            board_arrs,
            oracle_per_level,
            move_data_per_level,
            max_depth,
        )


# ---------------------------------------------------------------------------
# Walk step U = R_B * R_A (compiled)
# ---------------------------------------------------------------------------

# Module-level compiled function: created once per process, re-captures per circuit.
# The @ql.compile infrastructure handles its own per-circuit cache clearing.
_walk_compiled_fn = None


def walk_step(h_reg, branch_regs, board_arrs, oracle_per_level, move_data_per_level, max_depth):
    """Apply walk step U = R_B * R_A.

    On first call, the gate sequence is captured and compiled via
    @ql.compile for caching and automatic inverse derivation.
    Subsequent calls replay the cached gate sequence.

    The height + branch qubits are wrapped in a mega-register via
    all_walk_qubits(), and board qarrays (wk, bk, wn) are passed as
    separate arguments so the compile infrastructure treats all walk
    qubits as parameters (not internal ancillas).

    Parameters
    ----------
    h_reg : qint
        Height register.
    branch_regs : list[qint]
        Per-level branch registers.
    board_arrs : tuple
        (wk_arr, bk_arr, wn_arr) qarrays.
    oracle_per_level : list
        Compiled apply_move functions, one per level.
    move_data_per_level : list[dict]
        Move data per level.
    max_depth : int
        Maximum tree depth.
    """
    global _walk_compiled_fn
    from quantum_language.compile import compile as ql_compile

    mega_reg = all_walk_qubits(h_reg, branch_regs, max_depth)
    total = mega_reg.width + sum(len(a) for a in board_arrs)

    if _walk_compiled_fn is None:
        # Create the compiled function once. The closure captures walk
        # args by reference via a mutable container so they update each call.
        _ctx = {}

        def _walk_body(walk_qubits_reg, wk_arr, bk_arr, wn_arr):
            c = _ctx
            _board = (wk_arr, bk_arr, wn_arr)
            r_a(c["h"], c["br"], _board, c["opl"], c["mdpl"], c["md"])
            r_b(c["h"], c["br"], _board, c["opl"], c["mdpl"], c["md"])

        _walk_compiled_fn = ql_compile(key=lambda r, w, b, n: _ctx.get("total", 0))(_walk_body)
        _walk_compiled_fn._walk_ctx = _ctx  # stash reference

    # Update context for this call
    ctx = _walk_compiled_fn._walk_ctx
    ctx["h"] = h_reg
    ctx["br"] = branch_regs
    ctx["opl"] = oracle_per_level
    ctx["mdpl"] = move_data_per_level
    ctx["md"] = max_depth
    ctx["total"] = total

    wk, bk, wn = board_arrs
    _walk_compiled_fn(mega_reg, wk, bk, wn)
