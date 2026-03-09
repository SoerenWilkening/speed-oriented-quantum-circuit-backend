"""Chess board encoding and legal move generation for quantum walk demo.

Square indexing convention:
    - Square index 0-63 where sq = rank * 8 + file
    - rank 0 = chess rank 1 (bottom of board from white's perspective)
    - rank, file = divmod(sq, 8)
    - qarray indexing: board[rank, file]

This module provides classical move generation for a simplified chess endgame
(2 kings + white knights). All move generation is purely classical since
starting positions are known at circuit construction time. The quantum part
(board encoding as qarrays) interfaces with quantum_language for Phase 104's
quantum walk.

Piece types supported:
    - White king (1 piece)
    - Black king (1 piece)
    - White knights (configurable, typically 1-2)
"""

import numpy as np

import quantum_language as ql

__all__ = [
    "encode_position",
    "knight_attacks",
    "king_attacks",
    "legal_moves_white",
    "legal_moves_black",
    "legal_moves",
    "print_position",
    "print_moves",
    "sq_to_algebraic",
    "build_move_table",
]

# Knight L-shaped move offsets (rank_delta, file_delta)
_KNIGHT_OFFSETS = [
    (-2, -1),
    (-2, 1),
    (-1, -2),
    (-1, 2),
    (1, -2),
    (1, 2),
    (2, -1),
    (2, 1),
]

# King adjacent move offsets (rank_delta, file_delta)
_KING_OFFSETS = [
    (-1, -1),
    (-1, 0),
    (-1, 1),
    (0, -1),
    (0, 1),
    (1, -1),
    (1, 0),
    (1, 1),
]


def build_move_table(
    pieces: list[tuple[str, str]],
) -> list[tuple[str, int, int]]:
    """Build a position-independent move enumeration table.

    Creates a fixed mapping from branch register index to (piece_id, dr, df)
    triples. Each piece contributes exactly 8 entries (all geometric offsets
    for its type), regardless of board position. Edge-of-board filtering is
    handled later by the quantum legality predicate.

    Parameters
    ----------
    pieces : list[tuple[str, str]]
        List of (piece_id, piece_type) tuples. piece_type must be
        ``"knight"`` or ``"king"``.

    Returns
    -------
    list[tuple[str, int, int]]
        Table of (piece_id, rank_delta, file_delta) triples.
        Length is always ``8 * len(pieces)``.
        Index in list corresponds to branch register value.
    """
    table: list[tuple[str, int, int]] = []
    for piece_id, piece_type in pieces:
        offsets = _KNIGHT_OFFSETS if piece_type == "knight" else _KING_OFFSETS
        for dr, df in offsets:
            table.append((piece_id, dr, df))
    return table


def encode_position(wk_sq, bk_sq, wn_squares):
    """Encode a chess position as qarrays.

    Creates separate qarrays for each piece type, with X-gates applied
    on occupied squares (classical initialization).

    Parameters
    ----------
    wk_sq : int
        White king square (0-63).
    bk_sq : int
        Black king square (0-63).
    wn_squares : list[int]
        White knight square(s), each 0-63.

    Returns
    -------
    dict
        Keys: 'white_king', 'black_king', 'white_knights'.
        Values: ql.qarray objects (8x8 qbool arrays).
    """
    wk = np.zeros((8, 8), dtype=int)
    wk[wk_sq // 8, wk_sq % 8] = 1

    bk = np.zeros((8, 8), dtype=int)
    bk[bk_sq // 8, bk_sq % 8] = 1

    wn = np.zeros((8, 8), dtype=int)
    for sq in wn_squares:
        wn[sq // 8, sq % 8] = 1

    return {
        "white_king": ql.qarray(wk, dtype=ql.qbool),
        "black_king": ql.qarray(bk, dtype=ql.qbool),
        "white_knights": ql.qarray(wn, dtype=ql.qbool),
    }


def knight_attacks(square):
    """Return sorted list of squares attacked by a knight on ``square``.

    Parameters
    ----------
    square : int
        Square index 0-63 (rank * 8 + file).

    Returns
    -------
    list[int]
        Attacked square indices, sorted ascending.
    """
    rank, file = divmod(square, 8)
    attacks = []
    for dr, df in _KNIGHT_OFFSETS:
        nr, nf = rank + dr, file + df
        if 0 <= nr < 8 and 0 <= nf < 8:
            attacks.append(nr * 8 + nf)
    return sorted(attacks)


def king_attacks(square):
    """Return sorted list of squares attacked by a king on ``square``.

    Parameters
    ----------
    square : int
        Square index 0-63 (rank * 8 + file).

    Returns
    -------
    list[int]
        Attacked square indices, sorted ascending.
    """
    rank, file = divmod(square, 8)
    attacks = []
    for dr, df in _KING_OFFSETS:
        nr, nf = rank + dr, file + df
        if 0 <= nr < 8 and 0 <= nf < 8:
            attacks.append(nr * 8 + nf)
    return sorted(attacks)


def legal_moves_white(wk_sq, bk_sq, wn_squares):
    """Return enumerated legal moves for white.

    Generates all legal moves for white's pieces (knights + king),
    filtering out moves to friendly-occupied squares and king moves
    to squares attacked by the black king.

    Parameters
    ----------
    wk_sq : int
        White king square (0-63).
    bk_sq : int
        Black king square (0-63).
    wn_squares : list[int]
        White knight square(s).

    Returns
    -------
    list[tuple[int, int]]
        Sorted list of (piece_square, destination_square) pairs.
        Index in list = branch register value.
    """
    moves = []
    friendly_squares = set(wn_squares) | {wk_sq}

    # Black king attacks -- squares white king cannot move to
    bk_attack_set = set(king_attacks(bk_sq)) | {bk_sq}

    # Knight moves (sorted by piece square for determinism)
    for sq in sorted(wn_squares):
        for dest in knight_attacks(sq):
            if dest not in friendly_squares:
                moves.append((sq, dest))

    # King moves
    for dest in king_attacks(wk_sq):
        if dest not in friendly_squares and dest not in bk_attack_set:
            moves.append((wk_sq, dest))

    # Sort by (piece_sq, dest_sq) for deterministic ordering
    moves.sort()
    return moves


def legal_moves_black(wk_sq, bk_sq, wn_squares):
    """Return enumerated legal moves for black.

    In the simplified KNK endgame, black only has a king.
    Filters out squares attacked by any white piece.

    Parameters
    ----------
    wk_sq : int
        White king square (0-63).
    bk_sq : int
        Black king square (0-63).
    wn_squares : list[int]
        White knight square(s).

    Returns
    -------
    list[tuple[int, int]]
        Sorted list of (bk_sq, destination_square) pairs.
        Index in list = branch register value.
    """
    # Compute white attack set: all squares attacked by white pieces
    white_attack_set = set(king_attacks(wk_sq)) | {wk_sq}
    for sq in wn_squares:
        white_attack_set |= set(knight_attacks(sq))

    # Black king moves, excluding attacked squares
    moves = []
    for dest in king_attacks(bk_sq):
        if dest not in white_attack_set:
            moves.append((bk_sq, dest))

    moves.sort()
    return moves


def legal_moves(wk_sq, bk_sq, wn_squares, side_to_move):
    """Convenience wrapper returning legal moves for the specified side.

    Parameters
    ----------
    wk_sq : int
        White king square (0-63).
    bk_sq : int
        Black king square (0-63).
    wn_squares : list[int]
        White knight square(s).
    side_to_move : str
        'white' or 'black'.

    Returns
    -------
    list[tuple[int, int]]
        Enumerated legal move list (index = branch register value).
    """
    if side_to_move == "white":
        return legal_moves_white(wk_sq, bk_sq, wn_squares)
    elif side_to_move == "black":
        return legal_moves_black(wk_sq, bk_sq, wn_squares)
    else:
        raise ValueError(f"side_to_move must be 'white' or 'black', got {side_to_move!r}")


def sq_to_algebraic(sq):
    """Convert square index (0-63) to algebraic notation (e.g., 'e1').

    Parameters
    ----------
    sq : int
        Square index where sq = rank * 8 + file.

    Returns
    -------
    str
        Algebraic notation string (e.g., 'a1', 'h8').
    """
    rank, file = divmod(sq, 8)
    return f"{'abcdefgh'[file]}{rank + 1}"


def print_position(wk_sq, bk_sq, wn_squares):
    """Print an ASCII board showing piece positions.

    Displays the board from white's perspective (rank 8 at top, rank 1 at
    bottom). Uses:
    - K = white king
    - k = black king
    - N = white knight
    - . = empty square

    Parameters
    ----------
    wk_sq : int
        White king square (0-63).
    bk_sq : int
        Black king square (0-63).
    wn_squares : list[int]
        White knight square(s).
    """
    wn_set = set(wn_squares)
    lines = []
    lines.append("  a b c d e f g h")
    for rank in range(7, -1, -1):
        row = [f"{rank + 1} "]
        for file in range(8):
            sq = rank * 8 + file
            if sq == wk_sq:
                row.append("K")
            elif sq == bk_sq:
                row.append("k")
            elif sq in wn_set:
                row.append("N")
            else:
                row.append(".")
            row.append(" ")
        lines.append("".join(row).rstrip())
    lines.append("  a b c d e f g h")
    print("\n".join(lines))


def print_moves(moves, label=""):
    """Print a move list with index, algebraic notation, and square indices.

    Each line shows: index (branch register value), source and destination
    in algebraic notation, and the raw (piece_sq, dest_sq) pair.

    Parameters
    ----------
    moves : list[tuple[int, int]]
        Legal move list as (piece_sq, dest_sq) pairs.
    label : str, optional
        Label to print before the move list.
    """
    if label:
        print(f"{label} ({len(moves)} moves):")
    else:
        print(f"{len(moves)} moves:")
    for i, (src, dst) in enumerate(moves):
        src_alg = sq_to_algebraic(src)
        dst_alg = sq_to_algebraic(dst)
        print(f"  [{i:2d}] {src_alg}-{dst_alg}  ({src:2d} -> {dst:2d})")
