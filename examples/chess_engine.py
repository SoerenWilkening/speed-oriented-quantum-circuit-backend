"""Quantum chess engine demonstrating v9.0 features.

King + 2 Knights (white) vs King (black) endgame. Builds a quantum walk
search tree over black king moves using nested with-blocks, 2D qarrays,
and compiled sub-predicates.
"""

import itertools

import quantum_language as ql

# === Configuration ===

NUM_STEPS = 3  # Walk iterations (first call compiles, rest replay from DAG)

# Initial piece positions (rank, file) -- rank 0 = chess rank 1
WHITE_KING_POS = (0, 4)  # e1
WHITE_KNIGHT_1 = (1, 1)  # b2
WHITE_KNIGHT_2 = (2, 5)  # f3
BLACK_KING_POS = (6, 4)  # e7


# === Move Tables ===

# 8 king move directions indexed by 3-bit branch register value
DIRECTIONS = [
    (-1, 0),  # 0: N
    (-1, 1),  # 1: NE
    (0, 1),  # 2: E
    (1, 1),  # 3: SE
    (1, 0),  # 4: S
    (1, -1),  # 5: SW
    (0, -1),  # 6: W
    (-1, -1),  # 7: NW
]

_KNIGHT_OFFSETS = [(-2, -1), (-2, 1), (-1, -2), (-1, 2), (1, -2), (1, 2), (2, -1), (2, 1)]

_KING_OFFSETS = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]


def potential_knight_moves(r, c, grid_size=8):
    """Return list of valid (nr, nc) squares a knight can reach from (r, c)."""
    return [
        (r + dr, c + dc)
        for dr, dc in _KNIGHT_OFFSETS
        if 0 <= r + dr < grid_size and 0 <= c + dc < grid_size
    ]


def potential_king_moves(r, c, grid_size=8):
    """Return list of valid (nr, nc) squares a king can reach from (r, c)."""
    return [
        (r + dr, c + dc)
        for dr, dc in _KING_OFFSETS
        if 0 <= r + dr < grid_size and 0 <= c + dc < grid_size
    ]


def print_position(wk, wn1, wn2, bk):
    """Print ASCII board with K=white king, N=white knight, k=black king."""
    pieces = {wk: "K", wn1: "N", wn2: "N", bk: "k"}
    print("  a b c d e f g h")
    for rank in range(7, -1, -1):
        row = [f"{rank + 1} "]
        for file in range(8):
            pos = (rank, file)
            row.append(pieces.get(pos, "."))
            row.append(" ")
        print("".join(row).rstrip())
    print("  a b c d e f g h")


# === Board Setup ===

print("=== Board Setup ===")
print_position(WHITE_KING_POS, WHITE_KNIGHT_1, WHITE_KNIGHT_2, BLACK_KING_POS)

ql.circuit()

# Piece boards: one 8x8 qbool array per piece type
white_king = ql.qarray(dim=(8, 8), dtype=ql.qbool)
white_king[0, 4] ^= ql.qbool(True)  # e1

white_knight = ql.qarray(dim=(8, 8), dtype=ql.qbool)
white_knight[1, 1] ^= ql.qbool(True)  # b2
white_knight[2, 5] ^= ql.qbool(True)  # f3

black_king = ql.qarray(dim=(8, 8), dtype=ql.qbool)
black_king[6, 4] ^= ql.qbool(True)  # e7

# Quantum registers for walk tree
branch = ql.qint(0, width=3)  # 3-bit direction index (0-7)
feasibility = ql.qarray(dim=8, dtype=ql.qbool)  # one flag per direction
height = ql.qarray(dim=2, dtype=ql.qbool)  # one-hot: [0]=leaf, [1]=root
height[0] ^= ql.qbool(True)  # start at leaf level

print("Circuit initialized.\n")


# === Compiled Predicates ===


# Compiled attack computation prevents OOM from loop expansion.
# Called twice per direction: forward computes, second call uncomputes (XOR is self-inverse).
@ql.compile(opt=1)
def compute_white_attacks(w_knight, w_king, attack_map):
    """XOR white attack targets into attack_map for check detection."""
    for r, c in itertools.product(range(8), range(8)):
        with w_knight[r, c]:
            for nr, nc in potential_knight_moves(r, c):
                attack_map[nr, nc] += 1  # toggle attack flag (controlled on piece)
        with w_king[r, c]:
            for nr, nc in potential_king_moves(r, c):
                attack_map[nr, nc] += 1  # toggle attack flag (controlled on piece)
    return attack_map


# === Walk Operators ===


@ql.compile(opt=1)
def walk_step(bk, w_knight, w_king, br, feas, h):
    """One step of the quantum walk: evaluate -> diffuse -> apply.

    R_A evaluates all 8 king move directions for legality,
    R_B applies diffusion conditioned on leaf level,
    then the selected move is applied controlled on branch and feasibility.
    """
    bk_r, bk_c = BLACK_KING_POS

    # --- Phase 1: R_A -- evaluate all 8 directions ---
    for d, (dr, dc) in enumerate(DIRECTIONS):
        to_r, to_c = bk_r + dr, bk_c + dc

        # Off-board directions: feasibility stays False (skip)
        if not (0 <= to_r < 8 and 0 <= to_c < 8):
            continue

        # Legality nesting: piece-exists, no-friendly-capture, not-in-check
        with bk[bk_r, bk_c]:  # piece exists at source
            with ~w_king[to_r, to_c]:  # no white king at dest
                with ~w_knight[to_r, to_c]:  # no white knight at dest
                    # Make move (toggle via += 1 for controlled context)
                    bk[bk_r, bk_c] += 1
                    bk[to_r, to_c] += 1

                    # Build attack map and check for check
                    attack_map = ql.qarray(dim=(8, 8), dtype=ql.qbool)
                    compute_white_attacks(w_knight, w_king, attack_map)
                    in_check = attack_map[to_r, to_c]

                    # Mark legal if not in check
                    with ~in_check:
                        feas[d] += 1

                    # Uncompute attack map (apply again -- XOR is self-inverse)
                    compute_white_attacks(w_knight, w_king, attack_map)

                    # Unmake move
                    bk[to_r, to_c] += 1
                    bk[bk_r, bk_c] += 1
        # Direction d (chess notation in comments below)

    # --- Phase 2: R_B -- diffusion amplifies legal move branches ---
    with h[0]:  # at leaf level
        ql.diffusion(br)

    # --- Phase 3: Controlled move application ---
    for d, (dr, dc) in enumerate(DIRECTIONS):
        to_r, to_c = bk_r + dr, bk_c + dc
        if not (0 <= to_r < 8 and 0 <= to_c < 8):
            continue

        branch_match = br == d
        with branch_match:
            with feas[d]:
                # Apply selected move conditioned on branch register and feasibility
                bk[bk_r, bk_c] += 1
                bk[to_r, to_c] += 1

    return bk, w_knight, w_king, br, feas, h


# === Main Execution ===

print("Building quantum walk circuit...")

for step in range(NUM_STEPS):
    print(f"  Step {step + 1}/{NUM_STEPS}...")
    walk_step(black_king, white_knight, white_king, branch, feasibility, height)

# Circuit statistics from the DAG (no simulation needed)
if walk_step._call_graph is not None:
    dag = walk_step._call_graph
    agg = dag.aggregate()
    print("\n=== Circuit Statistics ===")
    print(f"Total gates:  {agg['gates']:,}")
    print(f"Circuit depth: {agg['depth']:,}")
    print(f"Total qubits: {agg['qubits']:,}")
    print(f"T-count:      {agg['t_count']:,}")
    print(f"DAG nodes:    {dag.node_count}")

print("\nDone.")
