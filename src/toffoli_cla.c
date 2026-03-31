/**
 * @file toffoli_cla.c
 * @brief Dynamic Brent-Kung CLA Toffoli addition for all widths.
 *
 * Module 1.10 (Phase 1) - refactor-hgl
 *
 * Refactored from: Quantum_Assembly/c_backend/src/ToffoliAdditionCLA.c
 *
 * Implements the Brent-Kung parallel prefix CLA adder with O(log n) depth
 * carry computation. All functions build qc_sequence_t structures which are
 * then applied via qc_run_instruction().
 *
 * Variants:
 *   - QQ  (quantum-quantum): b += a
 *   - CQ  (classical-quantum): self += classical_value
 *   - cQQ (controlled quantum-quantum): b += a, controlled
 *   - cCQ (controlled classical-quantum): self += classical_value, controlled
 *
 * Key design:
 *   - All functions take circuit_ctx_t* ctx (no global state)
 *   - Sequences are built dynamically for any width (no hardcoded files)
 *   - QQ/cQQ sequences are cached per-width; CQ/cCQ are value-dependent
 *   - Kogge-Stone stubs return NULL (fall through to CDKM/RCA)
 *
 * References:
 *   Brent & Kung, "A Regular Layout for Parallel Adders" (1982)
 *   Draper et al., "A logarithmic-depth quantum carry-lookahead adder" (2006)
 */

#include "internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* BK merge descriptor                                                     */
/* ====================================================================== */

typedef struct {
    int pos;     /**< position being updated (target of group generate) */
    int partner; /**< position being merged from (source) */
    int level;   /**< tree level (0-based) */
    int is_down; /**< 0 = up-sweep, 1 = down-sweep/tail */
} qc_bk_merge_t;

/* ====================================================================== */
/* Precompiled caches (per-width, not value-dependent)                     */
/* ====================================================================== */

static qc_sequence_t *cached_qq_add_bk[65]  = {NULL};
static qc_sequence_t *cached_cqq_add_bk[65] = {NULL};

/* ====================================================================== */
/* Gate initializer helpers (stack-allocated gates into sequence layers)    */
/* ====================================================================== */

static void seq_x(qc_sequence_t *seq, int layer, uint32_t target) {
    qc_gate_internal_t *g = &seq->seq[layer][seq->gates_per_layer[layer]];
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 0;
    seq->gates_per_layer[layer]++;
}

static void seq_cx(qc_sequence_t *seq, int layer,
                   uint32_t target, uint32_t control) {
    qc_gate_internal_t *g = &seq->seq[layer][seq->gates_per_layer[layer]];
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 1;
    g->Control[0] = control;
    seq->gates_per_layer[layer]++;
}

static void seq_ccx(qc_sequence_t *seq, int layer,
                    uint32_t target, uint32_t ctrl1, uint32_t ctrl2) {
    qc_gate_internal_t *g = &seq->seq[layer][seq->gates_per_layer[layer]];
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 2;
    g->Control[0] = ctrl1;
    g->Control[1] = ctrl2;
    seq->gates_per_layer[layer]++;
}

/* ====================================================================== */
/* BK prefix tree merge computation                                        */
/* ====================================================================== */

/**
 * @brief Compute BK prefix tree merge list.
 *
 * Generates ordered list of merge operations for the Brent-Kung parallel
 * prefix tree on n_carries carry positions.
 *
 * @param n_carries  Number of carry positions (bits - 1)
 * @param merges     Output array (caller-allocated, max 128 entries)
 * @param max_merges Maximum entries in merges array
 * @return Number of merges written
 */
static int bk_compute_merges(int n_carries, qc_bk_merge_t *merges,
                             int max_merges) {
    if (n_carries <= 1)
        return 0;

    int count = 0;
    int up_sweep[128] = {0};
    int covered[128] = {0};
    covered[0] = 1;

    int leftmost[128];
    for (int i = 0; i < n_carries && i < 128; i++)
        leftmost[i] = i;

    /* Up-sweep: reduce phase */
    int max_level = -1;
    int k = 0;
    while (1) {
        int stride = 1 << (k + 1);
        int half = 1 << k;
        int found = 0;
        int pos = stride - 1;
        while (pos < n_carries && count < max_merges) {
            int partner = pos - half;
            if (partner >= 0) {
                merges[count].pos = pos;
                merges[count].partner = partner;
                merges[count].level = k;
                merges[count].is_down = 0;
                count++;
                up_sweep[pos] = 1;
                leftmost[pos] = leftmost[partner];
                if (leftmost[pos] == 0)
                    covered[pos] = 1;
                found = 1;
                if (k > max_level)
                    max_level = k;
            }
            pos += stride;
        }
        if (!found)
            break;
        k++;
    }

    /* Down-sweep: propagate phase */
    for (int dk = max_level - 1; dk >= 0; dk--) {
        int half = 1 << dk;
        int stride = half << 1;
        int pos = stride + half - 1;
        while (pos < n_carries && count < max_merges) {
            if (!up_sweep[pos]) {
                int partner = pos - half;
                if (partner >= 0) {
                    merges[count].pos = pos;
                    merges[count].partner = partner;
                    merges[count].level = dk;
                    merges[count].is_down = 1;
                    count++;
                    up_sweep[pos] = 1;
                    leftmost[pos] = leftmost[partner];
                    if (leftmost[pos] == 0)
                        covered[pos] = 1;
                }
            }
            pos += stride;
        }
    }

    /* Tail merges: chain from last complete position */
    for (int i = 1; i < n_carries && count < max_merges; i++) {
        if (!covered[i]) {
            for (int j = i - 1; j >= 0; j--) {
                if (covered[j]) {
                    merges[count].pos = i;
                    merges[count].partner = j;
                    merges[count].level = 0;
                    merges[count].is_down = 1;
                    count++;
                    covered[i] = 1;
                    break;
                }
            }
        }
    }

    return count;
}

/* ====================================================================== */
/* BK CLA ancilla count                                                    */
/* ====================================================================== */

/**
 * @brief Compute ancilla count for Brent-Kung CLA adder.
 *
 * For n-bit addition: 2*(n-1) + num_merges ancilla qubits.
 *
 * @param bits Width of operands (>= 2)
 * @return Number of ancilla qubits needed (0 for bits < 2)
 */
int qc_bk_cla_ancilla_count(int bits) {
    if (bits < 2)
        return 0;

    int n_carries = bits - 1;
    qc_bk_merge_t merges[128];
    int num_merges = bk_compute_merges(n_carries, merges, 128);

    return 2 * n_carries + num_merges;
}

/* ====================================================================== */
/* BK CLA QQ Adder: b += a                                                */
/* ====================================================================== */

/**
 * @brief Build Brent-Kung CLA QQ adder sequence: b += a.
 *
 * Uses compute-copy-uncompute pattern with BK parallel prefix tree.
 *
 * Qubit layout:
 *   [0..n-1]              = register a (source, PRESERVED)
 *   [n..2n-1]             = register b (target, gets a+b)
 *   [2n..3n-2]            = generate ancilla g[0..n-2]
 *   [3n-1..3n-2+tree_sz]  = tree propagate-product ancilla
 *   [3n-1+tree_sz..end]   = carry-copy ancilla c[0..n-2]
 *
 * @param bits Width of operands (2-64)
 * @return Cached sequence (do not free), or NULL for invalid input
 */
static qc_sequence_t *build_qq_add_bk(int bits) {
    if (bits < 2 || bits > 64)
        return NULL;

    if (cached_qq_add_bk[bits] != NULL)
        return cached_qq_add_bk[bits];

    int n = bits;
    int n_carries = n - 1;

    qc_bk_merge_t merges[128];
    int num_merges = bk_compute_merges(n_carries, merges, 128);

    int tree_base = 3 * n - 1;
    int carry_base = tree_base + num_merges;

    int prop_src[128];
    for (int i = 0; i < n_carries && i < 128; i++)
        prop_src[i] = n + i;

    int num_layers = 7 * n - 4 + 4 * num_merges;

    qc_sequence_t *seq = qc_toffoli_seq_alloc(num_layers);
    if (seq == NULL)
        return NULL;

    int layer = 0;
    int tree_anc_idx = 0;

    /* Phase A: Initialize generate and propagate */
    for (int i = 0; i < n_carries; i++) {
        seq_ccx(seq, layer, (uint32_t)(2 * n + i),
                (uint32_t)i, (uint32_t)(n + i));
        layer++;
    }
    for (int i = 0; i < n; i++) {
        seq_cx(seq, layer, (uint32_t)(n + i), (uint32_t)i);
        layer++;
    }

    /* Phase B: BK prefix tree */
    int phase_b_gates[512][3];
    int phase_b_count = 0;

    for (int m = 0; m < num_merges; m++) {
        int pos = merges[m].pos;
        int partner = merges[m].partner;
        int tree_qubit = tree_base + tree_anc_idx;

        int t1 = 2 * n + pos;
        int c1a = prop_src[pos];
        int c1b = 2 * n + partner;
        phase_b_gates[phase_b_count][0] = t1;
        phase_b_gates[phase_b_count][1] = c1a;
        phase_b_gates[phase_b_count][2] = c1b;
        phase_b_count++;
        seq_ccx(seq, layer, (uint32_t)t1, (uint32_t)c1a, (uint32_t)c1b);
        layer++;

        int t2 = tree_qubit;
        int c2a = prop_src[pos];
        int c2b = prop_src[partner];
        phase_b_gates[phase_b_count][0] = t2;
        phase_b_gates[phase_b_count][1] = c2a;
        phase_b_gates[phase_b_count][2] = c2b;
        phase_b_count++;
        seq_ccx(seq, layer, (uint32_t)t2, (uint32_t)c2a, (uint32_t)c2b);
        layer++;

        prop_src[pos] = tree_qubit;
        tree_anc_idx++;
    }

    /* Phase C: Copy carries */
    for (int i = 0; i < n_carries; i++) {
        seq_cx(seq, layer, (uint32_t)(carry_base + i),
               (uint32_t)(2 * n + i));
        layer++;
    }

    /* Phase D: Reverse BK prefix tree */
    for (int g_idx = phase_b_count - 1; g_idx >= 0; g_idx--) {
        seq_ccx(seq, layer, (uint32_t)phase_b_gates[g_idx][0],
                (uint32_t)phase_b_gates[g_idx][1],
                (uint32_t)phase_b_gates[g_idx][2]);
        layer++;
    }

    /* Phase E: Uncompute propagates and generates */
    for (int i = n - 1; i >= 0; i--) {
        seq_cx(seq, layer, (uint32_t)(n + i), (uint32_t)i);
        layer++;
    }
    for (int i = n_carries - 1; i >= 0; i--) {
        seq_ccx(seq, layer, (uint32_t)(2 * n + i),
                (uint32_t)i, (uint32_t)(n + i));
        layer++;
    }

    /* Phase F: Sum extraction */
    for (int i = 0; i < n; i++) {
        seq_cx(seq, layer, (uint32_t)(n + i), (uint32_t)i);
        layer++;
    }
    for (int i = 1; i < n; i++) {
        seq_cx(seq, layer, (uint32_t)(n + i),
               (uint32_t)(carry_base + i - 1));
        layer++;
    }

    seq->used_layer = (uint32_t)layer;
    qc_sequence_compute_total_gate_count(seq);

    cached_qq_add_bk[bits] = seq;
    return seq;
}

/* ====================================================================== */
/* BK CLA CQ Adder: self += classical_value                               */
/* ====================================================================== */

/**
 * @brief Build Brent-Kung CLA CQ adder sequence: self += classical_value.
 *
 * Classical-bit gate simplification: gates controlled by classical |0>
 * are eliminated; gates controlled by classical |1> are simplified.
 *
 * @param bits  Width of target operand (2-64)
 * @param value Classical integer value to add
 * @return Fresh sequence (caller must free via qc_toffoli_seq_free), or NULL
 */
static qc_sequence_t *build_cq_add_bk(int bits, int64_t value) {
    if (bits < 2 || bits > 64)
        return NULL;

    qc_sequence_t *seq_qq = build_qq_add_bk(bits);
    if (seq_qq == NULL)
        return NULL;

    int n = bits;
    int n_carries = n - 1;

    int *bin = qc_two_complement(value, bits);
    if (bin == NULL)
        return NULL;

    int x_count = 0;
    for (int i = 0; i < bits; i++) {
        if (bin[bits - 1 - i] == 1)
            x_count++;
    }

    qc_bk_merge_t merges[128];
    int num_merges = bk_compute_merges(n_carries, merges, 128);

    /* Compute layer counts */
    int phase_a_layers = 0;
    for (int i = 0; i < n_carries; i++) {
        int bit_i = bin[bits - 1 - i];
        if (bit_i == 1)
            phase_a_layers += 2;
    }
    for (int i = 0; i < n; i++) {
        int bit_i = bin[bits - 1 - i];
        if (bit_i == 1)
            phase_a_layers += 1;
    }
    int phase_e_layers = phase_a_layers;
    int phase_bcd_layers = 4 * num_merges + n_carries;
    int phase_f_layers = x_count + (n - 1);
    int num_layers = phase_a_layers + phase_bcd_layers + phase_e_layers +
                     phase_f_layers;

    qc_sequence_t *seq = qc_toffoli_seq_alloc(num_layers);
    if (seq == NULL) {
        free(bin);
        return NULL;
    }

    int layer = 0;
    int tree_base = 3 * n - 1;
    int carry_base = tree_base + num_merges;

    /* Phase A (simplified) */
    for (int i = 0; i < n_carries; i++) {
        int bit_i = bin[bits - 1 - i];
        if (bit_i == 1) {
            seq_x(seq, layer, (uint32_t)i);
            layer++;
            seq_cx(seq, layer, (uint32_t)(2 * n + i), (uint32_t)(n + i));
            layer++;
        }
    }
    for (int i = 0; i < n; i++) {
        int bit_i = bin[bits - 1 - i];
        if (bit_i == 1) {
            seq_x(seq, layer, (uint32_t)(n + i));
            layer++;
        }
    }

    /* Phases B, C, D: copy from cached QQ BK sequence */
    {
        int qq_phase_b_start = 2 * n - 1;
        int qq_phase_d_end = 2 * n - 1 + 4 * num_merges + n_carries;
        for (int l = qq_phase_b_start; l < qq_phase_d_end; l++) {
            for (uint32_t gi = 0; gi < seq_qq->gates_per_layer[l]; gi++) {
                qc_gate_internal_t *src = &seq_qq->seq[l][gi];
                qc_gate_internal_t *dst =
                    &seq->seq[layer][seq->gates_per_layer[layer]];
                memcpy(dst, src, sizeof(qc_gate_internal_t));
                dst->large_control = NULL;
                if (src->NumControls > QC_MAX_INLINE_CONTROLS &&
                    src->large_control != NULL) {
                    dst->large_control =
                        malloc(src->NumControls * sizeof(uint32_t));
                    if (dst->large_control != NULL) {
                        memcpy(dst->large_control, src->large_control,
                               src->NumControls * sizeof(uint32_t));
                    }
                }
                seq->gates_per_layer[layer]++;
            }
            layer++;
        }
    }

    /* Phase E (simplified) */
    for (int i = n - 1; i >= 0; i--) {
        int bit_i = bin[bits - 1 - i];
        if (bit_i == 1) {
            seq_x(seq, layer, (uint32_t)(n + i));
            layer++;
        }
    }
    for (int i = n_carries - 1; i >= 0; i--) {
        int bit_i = bin[bits - 1 - i];
        if (bit_i == 1) {
            seq_cx(seq, layer, (uint32_t)(2 * n + i), (uint32_t)(n + i));
            layer++;
            seq_x(seq, layer, (uint32_t)i);
            layer++;
        }
    }

    /* Phase F (simplified) */
    for (int i = 0; i < n; i++) {
        int bit_i = bin[bits - 1 - i];
        if (bit_i == 1) {
            seq_x(seq, layer, (uint32_t)(n + i));
            layer++;
        }
    }
    for (int i = 1; i < n; i++) {
        seq_cx(seq, layer, (uint32_t)(n + i),
               (uint32_t)(carry_base + i - 1));
        layer++;
    }

    seq->used_layer = (uint32_t)layer;
    qc_sequence_compute_total_gate_count(seq);

    free(bin);
    return seq;
}

/* ====================================================================== */
/* MCX(3) decomposition helper for controlled variants                     */
/* ====================================================================== */

/**
 * @brief Emit AND-ancilla MCX(3) decomposition into sequence layers.
 *
 * CCX(and_anc, c1, c2) -> CCX(target, and_anc, ext_ctrl) ->
 * CCX(and_anc, c1, c2)
 */
static void emit_mcx3_seq(qc_sequence_t *seq, int *layer,
                           uint32_t target, uint32_t c1, uint32_t c2,
                           uint32_t ext_ctrl, uint32_t and_anc) {
    seq_ccx(seq, *layer, and_anc, c1, c2);
    (*layer)++;
    seq_ccx(seq, *layer, target, and_anc, ext_ctrl);
    (*layer)++;
    seq_ccx(seq, *layer, and_anc, c1, c2);
    (*layer)++;
}

/* ====================================================================== */
/* Controlled BK CLA QQ Adder: b += a, controlled                         */
/* ====================================================================== */

/**
 * @brief Build controlled BK CLA QQ adder sequence.
 *
 * Every gate in the uncontrolled QQ BK sequence gets an additional control.
 * MCX(3+) gates are decomposed via AND-ancilla.
 *
 * @param bits Width of operands (2-64)
 * @return Cached sequence (do not free), or NULL
 */
static qc_sequence_t *build_cqq_add_bk(int bits) {
    if (bits < 2 || bits > 64)
        return NULL;

    if (cached_cqq_add_bk[bits] != NULL)
        return cached_cqq_add_bk[bits];

    qc_sequence_t *seq_qq = build_qq_add_bk(bits);
    if (seq_qq == NULL)
        return NULL;

    int ext_ctrl = 2 * bits + qc_bk_cla_ancilla_count(bits);

    /* Count total layers after MCX decomposition */
    int total_layers = 0;
    for (uint32_t l = 0; l < seq_qq->used_layer; l++) {
        for (uint32_t gi = 0; gi < seq_qq->gates_per_layer[l]; gi++) {
            qc_gate_internal_t *src = &seq_qq->seq[l][gi];
            if (src->NumControls == 2)
                total_layers += 3; /* CCX -> MCX(3) decomposed */
            else
                total_layers += 1; /* X->CX or CX->CCX */
        }
    }

    int and_anc = ext_ctrl + 1;

    qc_sequence_t *seq = qc_toffoli_seq_alloc(total_layers);
    if (seq == NULL)
        return NULL;

    int layer = 0;

    for (uint32_t l = 0; l < seq_qq->used_layer; l++) {
        for (uint32_t gi = 0; gi < seq_qq->gates_per_layer[l]; gi++) {
            qc_gate_internal_t *src = &seq_qq->seq[l][gi];

            if (src->NumControls == 0) {
                seq_cx(seq, layer, src->Target, (uint32_t)ext_ctrl);
                layer++;
            } else if (src->NumControls == 1) {
                seq_ccx(seq, layer, src->Target,
                        src->Control[0], (uint32_t)ext_ctrl);
                layer++;
            } else if (src->NumControls == 2) {
                emit_mcx3_seq(seq, &layer, src->Target,
                              src->Control[0], src->Control[1],
                              (uint32_t)ext_ctrl, (uint32_t)and_anc);
            }
        }
    }

    seq->used_layer = (uint32_t)layer;
    qc_sequence_compute_total_gate_count(seq);

    cached_cqq_add_bk[bits] = seq;
    return seq;
}

/* ====================================================================== */
/* Public API: qc_toffoli_qq_add_bk                                        */
/* ====================================================================== */

/**
 * @brief Brent-Kung CLA QQ addition: b += a (O(log n) depth).
 *
 * Builds (or retrieves cached) BK CLA sequence and applies it via
 * qc_run_instruction(). The caller provides qubit arrays a[] and b[]
 * plus any ancilla allocated via the allocator.
 *
 * @param ctx   Circuit context
 * @param a     Source register qubit indices (preserved)
 * @param b     Target register qubit indices (modified: b += a)
 * @param width Bit width (2-64; returns QC_ERR_WIDTH otherwise)
 * @return QC_OK on success, error code on failure
 */
qc_error_t qc_toffoli_qq_add_bk(circuit_ctx_t *ctx, const uint32_t *a,
                                  const uint32_t *b, uint32_t width) {
    if (ctx == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 2 || width > 64)
        return QC_ERR_WIDTH;

    qc_sequence_t *seq = build_qq_add_bk((int)width);
    if (seq == NULL)
        return QC_ERR_ALLOC;

    int anc_count = qc_bk_cla_ancilla_count((int)width);

    /* Build qubit mapping array:
     *   [0..n-1]   -> a[i]
     *   [n..2n-1]  -> b[i]
     *   [2n..end]  -> ancilla (allocated from ctx) */
    int total_qubits = 2 * (int)width + anc_count;
    uint32_t *qmap = malloc((size_t)total_qubits * sizeof(uint32_t));
    if (qmap == NULL)
        return QC_ERR_ALLOC;

    for (uint32_t i = 0; i < width; i++) {
        qmap[i] = a[i];
        qmap[width + i] = b[i];
    }

    /* Allocate ancilla */
    uint32_t anc_start;
    qc_error_t err = qc_qubit_alloc_n(ctx, (uint32_t)anc_count, &anc_start);
    if (err != QC_OK) {
        free(qmap);
        return err;
    }
    for (int i = 0; i < anc_count; i++)
        qmap[2 * (int)width + i] = anc_start + (uint32_t)i;

    qc_run_instruction(ctx, seq, qmap, 0);

    qc_qubit_free_n(ctx, anc_start, (uint32_t)anc_count);
    free(qmap);
    return QC_OK;
}

/* ====================================================================== */
/* Kogge-Stone stubs (fall through to CDKM/RCA)                            */
/* ====================================================================== */

qc_sequence_t *qc_toffoli_qq_add_ks_seq(int bits) {
    (void)bits;
    return NULL;
}

qc_sequence_t *qc_toffoli_cq_add_ks_seq(int bits, int64_t value) {
    (void)bits;
    (void)value;
    return NULL;
}

qc_sequence_t *qc_toffoli_cqq_add_ks_seq(int bits) {
    (void)bits;
    return NULL;
}

qc_sequence_t *qc_toffoli_ccq_add_ks_seq(int bits, int64_t value) {
    (void)bits;
    (void)value;
    return NULL;
}
