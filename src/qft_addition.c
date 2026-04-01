/**
 * @file qft_addition.c
 * @brief Dynamic QFT-based addition for all widths 1-64.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/IntegerAddition.c
 * Module: 1.8 (Phase 1)
 * Issue: refactor-h41
 *
 * Implements Draper QFT addition circuits as qc_sequence_t objects that
 * are applied to the circuit via qc_run_instruction(). All hardcoded
 * sequence file dependencies (add_seq_*.c) are eliminated; sequences
 * are generated dynamically for any width 1-64.
 *
 * Functions:
 *   - qc_seq_qft:        Build QFT sub-sequence into an existing sequence
 *   - qc_seq_qft_inv:    Build inverse QFT sub-sequence
 *   - qc_seq_qq_add:     Build QQ addition sequence (a += b)
 *   - qc_seq_cq_add:     Build CQ addition sequence (target += classical)
 *   - qc_seq_cqq_add:    Build controlled QQ addition sequence
 *   - qc_seq_ccq_add:    Build controlled CQ addition sequence
 *   - qc_arith_qq_add:   Public API for quantum-quantum addition
 *   - qc_arith_cq_add:   Public API for classical-quantum addition
 *   - qc_arith_cqq_add:  Public API for controlled QQ addition
 *   - qc_arith_ccq_add:  Public API for controlled CQ addition
 *
 * Thread safety: Each circuit_ctx_t is independent. No global state.
 */

#include "internal.h"

#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Maximum layer estimate for multiplication sequences */
#define QC_QFT_MAX_LAYERS 300000

/* ====================================================================== */
/* Sequence memory helpers                                                 */
/* ====================================================================== */

/**
 * @brief Allocate a qc_sequence_t with the given layer count and
 *        gates_per_layer capacity. Local helper (not the public single-arg
 *        qc_sequence_alloc).
 *
 * @param num_layers   Number of layers to allocate.
 * @param gates_cap    Max gates per layer (each layer gets this many slots).
 * @return Allocated sequence, or NULL on failure.
 */
static qc_sequence_t *qft_sequence_alloc(uint32_t num_layers, uint32_t gates_cap) {
    qc_sequence_t *seq = malloc(sizeof(qc_sequence_t));
    if (seq == NULL)
        return NULL;

    seq->used_layer = 0;
    seq->num_layer = num_layers;
    seq->total_gate_count = 0;

    seq->gates_per_layer = calloc(num_layers, sizeof(uint32_t));
    if (seq->gates_per_layer == NULL) {
        free(seq);
        return NULL;
    }

    seq->seq = calloc(num_layers, sizeof(qc_gate_internal_t *));
    if (seq->seq == NULL) {
        free(seq->gates_per_layer);
        free(seq);
        return NULL;
    }

    for (uint32_t i = 0; i < num_layers; ++i) {
        seq->seq[i] = calloc(gates_cap, sizeof(qc_gate_internal_t));
        if (seq->seq[i] == NULL) {
            for (uint32_t j = 0; j < i; ++j)
                free(seq->seq[j]);
            free(seq->seq);
            free(seq->gates_per_layer);
            free(seq);
            return NULL;
        }
    }

    return seq;
}

/* ====================================================================== */
/* Gate helpers (write into sequence slots)                                 */
/* ====================================================================== */

static void seq_h(qc_gate_internal_t *g, uint32_t target) {
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_H;
    g->Target = target;
    g->NumControls = 0;
}

static void seq_p(qc_gate_internal_t *g, uint32_t target, double value) {
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_P;
    g->GateValue = value;
    g->Target = target;
    g->NumControls = 0;
}

static void seq_cp(qc_gate_internal_t *g, uint32_t target, uint32_t control,
                   double value) {
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_P;
    g->GateValue = value;
    g->Target = target;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void seq_cx(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->NumControls = 1;
    g->GateValue = 1;
    g->Control[0] = control;
}

/* ====================================================================== */
/* Two's complement helper                                                 */
/* ====================================================================== */

/* qc_two_complement is defined in integer.c and declared in internal.h */

/* ====================================================================== */
/* QFT sub-sequence builders                                               */
/* ====================================================================== */

/**
 * @brief Build the QFT sub-sequence into an existing sequence.
 *
 * Textbook QFT (no swaps, MSB-first processing).
 * Uses layers [seq->used_layer, seq->used_layer + 2*n - 2].
 */
static void qc_seq_qft(qc_sequence_t *seq, int n) {
    for (int j = 0; j < n; ++j) {
        int q = n - 1 - j; /* actual qubit: MSB-first processing */
        uint32_t layer = seq->used_layer + (uint32_t)(2 * j);
        seq_h(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)q);

        for (int i = 0; i < n - 1 - j; ++i) {
            uint32_t cp_layer = seq->used_layer + (uint32_t)(2 * j + i + 1);
            seq_cp(&seq->seq[cp_layer][seq->gates_per_layer[cp_layer]++],
                   (uint32_t)q, (uint32_t)(q - i - 1),
                   M_PI / pow(2, i + 1));
        }
    }
    seq->used_layer += (uint32_t)(2 * n - 1);
}

/**
 * @brief Build the inverse QFT sub-sequence into an existing sequence.
 *
 * Reverse of textbook QFT: processes LSB to MSB with negated phases.
 */
static void qc_seq_qft_inv(qc_sequence_t *seq, int n) {
    for (int j = 0; j < n; ++j) {
        int q = n - 1 - j;
        for (int i = 0; i < n - 1 - j; ++i) {
            uint32_t layer = seq->used_layer + (uint32_t)(2 * n - 2 - (2 * j + i + 1));
            seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                   (uint32_t)q, (uint32_t)(q - i - 1),
                   -M_PI / pow(2, i + 1));
        }
        uint32_t h_layer = seq->used_layer + (uint32_t)(2 * n - 2 - 2 * j);
        seq_h(&seq->seq[h_layer][seq->gates_per_layer[h_layer]++], (uint32_t)q);
    }
    seq->used_layer += (uint32_t)(2 * n - 1);
}

/* ====================================================================== */
/* QQ_add sequence builder (a += b)                                        */
/* ====================================================================== */

/**
 * @brief Build a QQ addition sequence for the given width.
 *
 * Qubit layout:
 *   [0, bits-1]:     Target register (a, modified in place)
 *   [bits, 2*bits-1]: Source register (b, unchanged)
 *
 * Total layers: QFT(2n-1) + rotations(n) + IQFT(2n-1) = 5n-2
 */
qc_sequence_t *qc_arith_qq_add_seq(int bits) {
    uint32_t num_layers = (uint32_t)(5 * bits - 2);
    qc_sequence_t *seq = qft_sequence_alloc(num_layers, (uint32_t)(2 * bits));
    if (seq == NULL)
        return NULL;

    /* QFT on target register */
    qc_seq_qft(seq, bits);

    /* Draper controlled-phase rotations */
    int rounds = 0;
    for (int bit = bits - 1; bit >= 0; --bit) {
        for (int i = 0; i < bits - rounds; ++i) {
            uint32_t layer = (uint32_t)(2 * bits + i + 2 * rounds - 1);
            uint32_t target = (uint32_t)(rounds + i);
            uint32_t control = (uint32_t)(2 * bits - 1 - bit);
            double value = 2 * M_PI / pow(2, i + 1);
            seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                   target, control, value);
        }
        rounds++;
    }
    seq->used_layer += (uint32_t)bits;

    /* Inverse QFT */
    qc_seq_qft_inv(seq, bits);

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* CQ_add sequence builder (target += classical_value)                     */
/* ====================================================================== */

/**
 * @brief Build a CQ addition sequence.
 *
 * Qubit layout:
 *   [0, bits-1]: Target register (modified in place)
 *
 * The classical value is encoded as phase rotation angles.
 * Total layers: 5*bits - 2
 */
qc_sequence_t *qc_arith_cq_add_seq(int bits, int64_t value) {
    int *bin = qc_two_complement(value, bits);
    if (bin == NULL)
        return NULL;

    /* Compute rotation angles */
    double *rotations = calloc((size_t)bits, sizeof(double));
    if (rotations == NULL) {
        free(bin);
        return NULL;
    }
    for (int bit_idx = 0; bit_idx < bits; ++bit_idx) {
        for (int qubit = bit_idx; qubit < bits; ++qubit) {
            rotations[qubit] += bin[bits - 1 - bit_idx] * 2 * M_PI /
                                pow(2, qubit - bit_idx + 1);
        }
    }
    free(bin);

    uint32_t num_layers = (uint32_t)(5 * bits - 2);
    qc_sequence_t *seq = qft_sequence_alloc(num_layers, (uint32_t)(2 * bits));
    if (seq == NULL) {
        free(rotations);
        return NULL;
    }

    /* QFT */
    qc_seq_qft(seq, bits);

    /* Phase rotations encoding the classical value */
    uint32_t start_layer = (uint32_t)(2 * bits - 1);
    for (int i = 0; i < bits; ++i) {
        seq_p(&seq->seq[start_layer + (uint32_t)i]
                  [seq->gates_per_layer[start_layer + (uint32_t)i]++],
              (uint32_t)i, rotations[i]);
    }
    free(rotations);
    seq->used_layer += (uint32_t)bits;

    /* Inverse QFT */
    qc_seq_qft_inv(seq, bits);

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* cQQ_add sequence builder (controlled a += b)                            */
/* ====================================================================== */

/**
 * @brief Build a controlled QQ addition sequence.
 *
 * Qubit layout:
 *   [0, bits-1]:       Target register (a, modified in place)
 *   [bits, 2*bits-1]:  Source register (b)
 *   [2*bits]:          Control qubit
 *
 * Uses CCP decomposition: CCP(theta) = CP(theta/2,a) + CX + CP(-theta/2,a) + CX + CP(theta/2,b)
 */
qc_sequence_t *qc_arith_cqq_add_seq(int bits) {
    /* Estimate layers conservatively */
    uint32_t est_layers = (uint32_t)(bits * (bits + 1) / 2 * 4 + 4 * bits - 2
                                     - bits / 4 * 4 + 3);
    /* Add QFT + IQFT layers */
    est_layers += (uint32_t)(4 * bits - 2);
    /* Extra buffer for safety */
    if (est_layers < (uint32_t)(10 * bits * bits))
        est_layers = (uint32_t)(10 * bits * bits);

    qc_sequence_t *seq = qft_sequence_alloc(est_layers, (uint32_t)(2 * bits));
    if (seq == NULL)
        return NULL;

    /* QFT on target register */
    qc_seq_qft(seq, bits);

    uint32_t control = (uint32_t)(2 * bits);
    uint32_t layer = (uint32_t)(2 * bits - 1);

    /* Block 1: unconditional half-rotations on Fourier qubits */
    for (int bit = bits - 1; bit >= 0; --bit) {
        int target_q = bits - 1 - bit;
        double val = 0;
        for (int i = 0; i < bits - bit; ++i) {
            val += 2 * M_PI / pow(2, i + 1) / 2;
        }
        seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
               (uint32_t)target_q, control, val);
        layer++;
    }

    /* Block 2: CNOT + negative half-rotations + CNOT */
    int rounds = 0;
    for (int bit = bits - 1; bit >= 0; --bit) {
        uint32_t source_q = (uint32_t)(bits + (bits - 1 - bit));
        seq_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
               source_q, control);
        layer++;
        for (int i = 0; i < bits - rounds; ++i) {
            double val = 2 * M_PI / pow(2, i + 1) / 2;
            uint32_t tgt = (uint32_t)(rounds + i);
            seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                   tgt, source_q, -val);
            layer++;
        }
        seq_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
               source_q, control);
        layer++;
        rounds++;
    }

    /* Block 3: controlled rotations from b register */
    rounds = 0;
    for (int bit = bits - 1; bit >= 0; --bit) {
        uint32_t source_q = (uint32_t)(bits + (bits - 1 - bit));
        for (int i = 0; i < bits - rounds; ++i) {
            double val = 2 * M_PI / pow(2, i + 1) / 2;
            uint32_t tgt = (uint32_t)(rounds + i);
            seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                   tgt, source_q, val);
            layer++;
        }
        layer -= (uint32_t)(bits - rounds);
        rounds++;
    }
    seq->used_layer = layer + 1;

    /* Inverse QFT */
    qc_seq_qft_inv(seq, bits);

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* cCQ_add sequence builder (controlled target += classical_value)          */
/* ====================================================================== */

/**
 * @brief Build a controlled CQ addition sequence.
 *
 * Qubit layout:
 *   [0, bits-1]: Target register (modified in place)
 *   [bits]:      Control qubit
 *
 * Total layers: 5*bits - 2
 */
qc_sequence_t *qc_arith_ccq_add_seq(int bits, int64_t value) {
    int *bin = qc_two_complement(value, bits);
    if (bin == NULL)
        return NULL;

    /* Compute rotation angles */
    double *rotations = calloc((size_t)bits, sizeof(double));
    if (rotations == NULL) {
        free(bin);
        return NULL;
    }
    for (int bit_idx = 0; bit_idx < bits; ++bit_idx) {
        for (int qubit = bit_idx; qubit < bits; ++qubit) {
            rotations[qubit] += bin[bits - 1 - bit_idx] * 2 * M_PI /
                                pow(2, qubit - bit_idx + 1);
        }
    }
    free(bin);

    uint32_t num_layers = (uint32_t)(5 * bits - 2);
    qc_sequence_t *seq = qft_sequence_alloc(num_layers, (uint32_t)(2 * bits));
    if (seq == NULL) {
        free(rotations);
        return NULL;
    }

    /* QFT */
    qc_seq_qft(seq, bits);

    /* Controlled phase rotations: target at [0..bits-1], control at [bits] */
    uint32_t start_layer = (uint32_t)(2 * bits - 1);
    for (int i = 0; i < bits; ++i) {
        seq_cp(&seq->seq[start_layer + (uint32_t)i]
                   [seq->gates_per_layer[start_layer + (uint32_t)i]++],
               (uint32_t)i, (uint32_t)bits, rotations[i]);
    }
    free(rotations);
    seq->used_layer += (uint32_t)bits;

    /* Inverse QFT */
    qc_seq_qft_inv(seq, bits);

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* Public API implementations                                              */
/* ====================================================================== */

qc_error_t qc_arith_qq_add(circuit_ctx_t *ctx, const uint32_t *a,
                            const uint32_t *b, uint32_t width) {
    if (ctx == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    qc_sequence_t *seq = qc_arith_qq_add_seq((int)width);
    if (seq == NULL)
        return QC_ERR_ALLOC;

    /* Build qubit mapping: [0..width-1] -> a[], [width..2*width-1] -> b[] */
    uint32_t qmap[128];
    for (uint32_t i = 0; i < width; ++i)
        qmap[i] = a[i];
    for (uint32_t i = 0; i < width; ++i)
        qmap[width + i] = b[i];

    qc_run_instruction(ctx, seq, qmap, 0);

    qc_sequence_free(seq);
    return QC_OK;
}

qc_error_t qc_arith_cq_add(circuit_ctx_t *ctx, const uint32_t *target,
                            uint32_t width, int64_t value) {
    if (ctx == NULL || target == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    qc_sequence_t *seq = qc_arith_cq_add_seq((int)width, value);
    if (seq == NULL)
        return QC_ERR_ALLOC;

    /* Qubit mapping: [0..width-1] -> target[] */
    uint32_t qmap[128];
    for (uint32_t i = 0; i < width; ++i)
        qmap[i] = target[i];

    qc_run_instruction(ctx, seq, qmap, 0);

    qc_sequence_free(seq);
    return QC_OK;
}

qc_error_t qc_arith_cqq_add(circuit_ctx_t *ctx, const uint32_t *a,
                             const uint32_t *b, uint32_t width,
                             uint32_t control) {
    if (ctx == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    qc_sequence_t *seq = qc_arith_cqq_add_seq((int)width);
    if (seq == NULL)
        return QC_ERR_ALLOC;

    /* Qubit mapping: [0..w-1] -> a[], [w..2w-1] -> b[], [2w] -> control */
    uint32_t qmap[130];
    for (uint32_t i = 0; i < width; ++i)
        qmap[i] = a[i];
    for (uint32_t i = 0; i < width; ++i)
        qmap[width + i] = b[i];
    qmap[2 * width] = control;

    qc_run_instruction(ctx, seq, qmap, 0);

    qc_sequence_free(seq);
    return QC_OK;
}

qc_error_t qc_arith_ccq_add(circuit_ctx_t *ctx, const uint32_t *target,
                             uint32_t width, int64_t value,
                             uint32_t control) {
    if (ctx == NULL || target == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    qc_sequence_t *seq = qc_arith_ccq_add_seq((int)width, value);
    if (seq == NULL)
        return QC_ERR_ALLOC;

    /* Qubit mapping: [0..w-1] -> target[], [w] -> control */
    uint32_t qmap[128];
    for (uint32_t i = 0; i < width; ++i)
        qmap[i] = target[i];
    qmap[width] = control;

    qc_run_instruction(ctx, seq, qmap, 0);

    qc_sequence_free(seq);
    return QC_OK;
}
