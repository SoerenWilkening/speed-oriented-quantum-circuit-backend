"""Quantum predicate factories for chess move conditions.

Provides factory functions that create compiled quantum predicates for
evaluating chess move legality in superposition. Each factory performs
classical precomputation (enumerating valid source squares, filtering
off-board destinations) then returns a @ql.compile(inverse=True) function
that operates only on quantum data.

Predicates follow the same factory pattern as _make_apply_move in
chess_encoding.py: classical closure data outside @ql.compile, quantum
operations inside.

Piece types supported:
    - Piece-exists: checks if a piece occupies any source square with a
      valid destination for a given move offset (dr, df).
    - No-friendly-capture: checks that the target square for a move is
      not occupied by a same-color (friendly) piece.
"""

import quantum_language as ql

__all__ = [
    "make_piece_exists_predicate",
    "make_no_friendly_capture_predicate",
    "make_check_detection_predicate",
    "make_combined_predicate",
]


def make_piece_exists_predicate(piece_id, dr, df, board_rows, board_cols):
    """Create a compiled predicate that checks if a piece can make move (dr, df).

    Returns a @ql.compile(inverse=True) function that flips result qbool
    to |1> if piece_qarray has a piece at any square (r, f) such that
    (r+dr, f+df) is within bounds.

    Classical precomputation enumerates all (r, f) with valid destinations
    at construction time. Off-board moves produce no quantum gates (classical
    skip). If no source square has the piece, the result stays |0> naturally.

    XOR vs OR note: For the KNK endgame, each piece type has exactly one
    instance per board qarray (1 white king, 1 black king). White knights
    qarray may have 2 knights, but the predicate is called per-move-offset
    (dr, df), and for a given (dr, df) each knight independently checks its
    own source square. Since each square has at most one piece, only one
    ``with board[r,f]:`` fires per basis state, so XOR = OR and the simple
    ``~result`` pattern is correct.

    Parameters
    ----------
    piece_id : str
        Identifier for debugging/logging (not used in quantum logic).
    dr : int
        Rank delta for the move.
    df : int
        File delta for the move.
    board_rows : int
        Number of rows on the board.
    board_cols : int
        Number of columns on the board.

    Returns
    -------
    CompiledFunc
        A @ql.compile(inverse=True) function with signature
        ``piece_exists(piece_qarray, result)`` where result is a qbool
        that gets flipped to |1> if the condition holds.
    """
    # Classical precomputation: which (r, f) have valid destinations
    valid_sources = []
    for r in range(board_rows):
        for f in range(board_cols):
            tr, tf = r + dr, f + df
            if 0 <= tr < board_rows and 0 <= tf < board_cols:
                valid_sources.append((r, f))

    @ql.compile(inverse=True)
    def piece_exists(piece_qarray, result):
        """Flip result to |1> if piece_qarray has a piece at any valid source.

        For each valid source (r, f), conditionally flips result when
        piece_qarray[r, f] is |1>. Uses flat sequential ``with`` blocks
        (no nesting) to respect the Toffoli-AND limitation.
        """
        for r, f in valid_sources:
            with piece_qarray[r, f]:
                ~result  # noqa: B018 -- quantum NOT (calls __invert__)

    return piece_exists


def make_no_friendly_capture_predicate(dr, df, board_rows, board_cols):
    """Create a compiled predicate that checks no friendly piece at target.

    Returns a @ql.compile(inverse=True) function that flips result qbool
    to |1> if piece_qarray has a piece at some valid source (r, f) AND
    no friendly board has a piece at the target square (r+dr, f+df).

    The predicate combines piece-existence with friendly-capture rejection
    in a single compiled function. Result = |1> means the move is safe
    (piece exists and target is free of friendlies). Result stays |0> if
    no piece exists at a valid source or if a friendly piece blocks the
    target.

    Implementation avoids nested ``with`` blocks (which raise
    NotImplementedError) by using the ``&`` operator on qbool values.
    For each valid source (r, f, tr, tf), the circuit:

      1. Accumulates a ``friendly_flag`` ancilla qbool -- set to |1>
         if ANY friendly board has a piece at target (tr, tf).
      2. Flips result if piece exists at source (piece-exists half).
      3. Un-flips result if piece at source AND friendly_flag,
         using ``piece_qarray[r, f] & friendly_flag`` (compiles to
         a Toffoli gate with auto-uncomputed ancilla).
      4. Uncomputes the friendly_flag ancilla.

    The per-source friendly_flag avoids ancilla-reuse interference
    that occurs when iterating ``&`` over multiple friendly boards
    in a single loop.

    XOR vs OR note: Same as piece-exists -- at most one source fires
    per basis state in the KNK endgame (one piece per board square).
    For the friendly flag, at most one friendly piece per target square
    across all boards, so the XOR accumulation equals OR.

    Parameters
    ----------
    dr : int
        Rank delta for the move.
    df : int
        File delta for the move.
    board_rows : int
        Number of rows on the board.
    board_cols : int
        Number of columns on the board.

    Returns
    -------
    CompiledFunc
        A @ql.compile(inverse=True) function with signature
        ``no_friendly_capture(piece_qarray, *friendly_qarrays, result)``
        where result is a qbool that gets flipped to |1> if the
        condition holds (piece exists and target is friendly-free).
    """
    # Classical precomputation: which (r, f) have valid destinations
    valid_sources = []
    for r in range(board_rows):
        for f in range(board_cols):
            tr, tf = r + dr, f + df
            if 0 <= tr < board_rows and 0 <= tf < board_cols:
                valid_sources.append((r, f, tr, tf))

    @ql.compile(inverse=True)
    def no_friendly_capture(piece_qarray, *args):
        """Flip result to |1> if piece at valid source and target is friendly-free.

        Arguments after piece_qarray are friendly board qarrays followed by
        the result qbool as the last argument. This variadic signature allows
        passing 1 or more friendly boards.

        For each valid source (r, f) with target (tr, tf):
          - Compute friendly_flag = OR of all friendly boards at (tr, tf)
          - Flip result if piece at (r, f)
          - Un-flip result if piece at (r, f) AND friendly_flag
          - Uncompute friendly_flag

        Net effect: result = |1> iff piece exists at a valid source AND
        no friendly piece at the corresponding target.
        """
        friendly_qarrays = args[:-1]
        result = args[-1]

        for r, f, tr, tf in valid_sources:
            # Accumulate friendly presence at target into ancilla
            friendly_flag = ql.qbool()
            for fq in friendly_qarrays:
                with fq[tr, tf]:
                    ~friendly_flag  # noqa: B018 -- XOR = OR (1 piece/square)

            # Flip result if piece at source
            with piece_qarray[r, f]:
                ~result  # noqa: B018 -- quantum NOT

            # Un-flip if piece AND friendly at target (Toffoli via & operator)
            cond = piece_qarray[r, f] & friendly_flag
            with cond:
                ~result  # noqa: B018 -- quantum NOT (Toffoli-controlled)

            # Uncompute friendly_flag (mirror the accumulation)
            for fq in friendly_qarrays:
                with fq[tr, tf]:
                    ~friendly_flag  # noqa: B018 -- uncompute

    return no_friendly_capture


def _compute_attack_table(moving_piece_type, dr, df, board_rows, board_cols, enemy_attacks):
    """Precompute attack table: for each king position, list all attacker squares.

    For king moves, the check-square is the destination (kr+dr, kf+df).
    For non-king moves, the check-square is the king's current position (kr, kf).

    Parameters
    ----------
    moving_piece_type : str
        "king" or "knight" -- determines which square to check for attacks.
    dr, df : int
        Move offset (rank delta, file delta).
    board_rows, board_cols : int
        Board dimensions.
    enemy_attacks : list[tuple[list[tuple[int,int]], str]]
        Each element is (offset_list, piece_type_label). offset_list contains
        (row_delta, file_delta) pairs representing attack patterns.

    Returns
    -------
    list[tuple[int, int, list[tuple[int, int, int]]]]
        Each entry is (king_r, king_f, [(attacker_r, attacker_f, enemy_idx), ...]).
    """
    table = []
    for kr in range(board_rows):
        for kf in range(board_cols):
            if moving_piece_type == "king":
                check_r, check_f = kr + dr, kf + df
                if not (0 <= check_r < board_rows and 0 <= check_f < board_cols):
                    continue  # destination off-board -> skip
            else:
                check_r, check_f = kr, kf

            attackers = []
            for enemy_idx, (offsets, _label) in enumerate(enemy_attacks):
                for adr, adf in offsets:
                    ar, af = check_r + adr, check_f + adf
                    if 0 <= ar < board_rows and 0 <= af < board_cols:
                        attackers.append((ar, af, enemy_idx))

            table.append((kr, kf, attackers))
    return table


def make_check_detection_predicate(
    moving_piece_type, dr, df, board_rows, board_cols, enemy_attacks
):
    """Create a compiled predicate that checks if the king is NOT in check.

    Returns a @ql.compile(inverse=True) function that flips result qbool
    to |1> if no enemy piece attacks the relevant king square. For king
    moves, the relevant square is the destination (kr+dr, kf+df). For
    non-king moves, the relevant square is the king's current position.

    The predicate uses the flip-and-unflip pattern: optimistically flip
    result to |1> (safe) for each possible king position, then un-flip
    if an enemy attacker is found at an attacking square.

    Parameters
    ----------
    moving_piece_type : str
        "king" or "knight".
    dr, df : int
        Move offset.
    board_rows, board_cols : int
        Board dimensions.
    enemy_attacks : list[tuple[list[tuple[int,int]], str]]
        Attack offset tables per enemy type.

    Returns
    -------
    CompiledFunc
        A @ql.compile(inverse=True) function with signature
        ``check_safe(king_qarray, *enemy_qarrays, result)`` where result
        is a qbool that gets flipped to |1> if king is safe.
    """
    attack_table = _compute_attack_table(
        moving_piece_type, dr, df, board_rows, board_cols, enemy_attacks
    )

    @ql.compile(inverse=True)
    def check_safe(king_qarray, *args):
        """Flip result to |1> if king is safe (not in check).

        Arguments after king_qarray are enemy board qarrays followed by
        the result qbool as the last argument.

        For each possible king position (kr, kf):
          1. Accumulate attack_flag: set to |1> if ANY enemy occupies an
             attacking square (XOR = OR since one piece per square).
          2. Optimistic flip: if king at (kr, kf), flip result to |1>.
          3. Un-flip if king here AND attack_flag (Toffoli via & operator).
          4. Uncompute attack_flag (mirror the accumulation).

        Uses per-position attack_flag accumulation (like no-friendly-capture's
        friendly_flag) instead of per-attacker enemy_flag to minimize ancilla
        count: one attack_flag + one cond per king position, not per attacker.
        """
        enemy_qarrays = args[:-1]
        result = args[-1]

        for kr, kf, attackers in attack_table:
            # Accumulate: is any enemy at an attacking square?
            attack_flag = ql.qbool()
            for ar, af, enemy_idx in attackers:
                with enemy_qarrays[enemy_idx][ar, af]:
                    ~attack_flag  # noqa: B018 -- XOR = OR (1 piece/square)

            # Optimistic: flip to |1> if king is at this position
            with king_qarray[kr, kf]:
                ~result  # noqa: B018 -- quantum NOT

            # Un-flip if king here AND any attacker found
            cond = king_qarray[kr, kf] & attack_flag
            with cond:
                ~result  # noqa: B018 -- un-flip: king in check

            # Uncompute attack_flag (mirror the accumulation)
            for ar, af, enemy_idx in attackers:
                with enemy_qarrays[enemy_idx][ar, af]:
                    ~attack_flag  # noqa: B018 -- uncompute

    return check_safe


def make_combined_predicate(
    piece_type, dr, df, board_rows, board_cols, enemy_attacks, n_friendly, n_enemy
):
    """Create a compiled predicate that ANDs piece-exists, no-friendly-capture, and check-safe.

    Returns a @ql.compile(inverse=True) function that flips result qbool
    to |1> only when ALL three conditions pass:
      1. Piece exists at a valid source square for move (dr, df)
      2. No friendly piece occupies the target square
      3. King is not in check after the move

    The three sub-predicates are built at construction time and called
    inside the compiled function. Intermediate qbool results are allocated,
    ANDed via the ``&`` operator, and then uncomputed.

    Parameters
    ----------
    piece_type : str
        "king" or "knight" -- determines check detection behavior.
    dr, df : int
        Move offset (rank delta, file delta).
    board_rows, board_cols : int
        Board dimensions.
    enemy_attacks : list[tuple[list[tuple[int,int]], str]]
        Attack offset tables per enemy type.
    n_friendly : int
        Number of friendly board qarrays (known at construction time).
    n_enemy : int
        Number of enemy board qarrays (known at construction time).

    Returns
    -------
    CompiledFunc
        A @ql.compile(inverse=True) function with signature
        ``is_legal(piece_qarray, *friendly_qarrays, king_qarray, *enemy_qarrays, result)``
        where result is a qbool that gets flipped to |1> if all conditions hold.
    """
    pe_pred = make_piece_exists_predicate(piece_type, dr, df, board_rows, board_cols)
    nfc_pred = make_no_friendly_capture_predicate(dr, df, board_rows, board_cols)
    cd_pred = make_check_detection_predicate(
        piece_type, dr, df, board_rows, board_cols, enemy_attacks
    )

    @ql.compile(inverse=True)
    def is_legal(piece_qarray, *args):
        """Flip result to |1> if all three legality conditions pass.

        Arguments after piece_qarray are:
          friendly_qarrays[0..n_friendly-1], king_qarray,
          enemy_qarrays[0..n_enemy-1], result

        Allocates 3 intermediate qbools for sub-predicate results,
        then ANDs them with chained ``&`` operator.
        """
        friendly_qarrays = args[:n_friendly]
        king_qarray = args[n_friendly]
        enemy_qarrays = args[n_friendly + 1 : n_friendly + 1 + n_enemy]
        result = args[-1]

        # Allocate intermediate result qbools
        pe_result = ql.qbool()
        nfc_result = ql.qbool()
        cd_result = ql.qbool()

        # Call sub-predicates
        pe_pred(piece_qarray, pe_result)
        nfc_pred(piece_qarray, *friendly_qarrays, nfc_result)
        cd_pred(king_qarray, *enemy_qarrays, cd_result)

        # Three-way AND via chained & operator
        ab = pe_result & nfc_result
        abc = ab & cd_result
        with abc:
            ~result  # noqa: B018 -- quantum NOT (flip result if all pass)

        # Uncompute sub-predicate results (reverse order)
        cd_pred.adjoint(king_qarray, *enemy_qarrays, cd_result)
        nfc_pred.adjoint(piece_qarray, *friendly_qarrays, nfc_result)
        pe_pred.adjoint(piece_qarray, pe_result)

    return is_legal
