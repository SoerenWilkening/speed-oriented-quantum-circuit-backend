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

__all__ = ["make_piece_exists_predicate", "make_no_friendly_capture_predicate"]


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
