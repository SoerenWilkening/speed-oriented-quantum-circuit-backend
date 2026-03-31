/**
 * @file qft_multiplication.c
 * @brief Dynamic QFT-based multiplication for all widths 1-64.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/IntegerMultiplication.c
 * Module: 1.8 (Phase 1)
 * Issue: refactor-h41
 *
 * Implements Draper QFT multiplication circuits as qc_sequence_t objects.
 * All hardcoded sequence file dependencies are eliminated; sequences
 * are generated dynamically for any width 1-64.
 *
 * Functions:
 *   - qc_seq_cq_mul:     Build CQ multiplication sequence (result += target * value)
 *   - qc_seq_qq_mul:     Build QQ multiplication sequence (result = a * b)
 *   - qc_arith_cq_mul:   Public API for classical-quantum multiplication
 *   - qc_arith_qq_mul:   Public API for quantum-quantum multiplication
 *
 * Thread safety: Each circuit_ctx_t is independent. No global state.
 */

#include "internal.h"

#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Max layers for multiplication sequences (large for big widths) */
#define QC_MUL_MAX_LAYERS 300000

/* ====================================================================== */
/* Sequence memory helpers (shared with qft_addition.c via linker)         */
/* ====================================================================== */

static void qc_mul_sequence_free(qc_sequence_t *seq) {
    if (seq == NULL)
        return;
    if (seq->seq != NULL) {
        for (uint32_t i = 0; i < seq->num_layer; ++i)
            free(seq->seq[i]);
        free(seq->seq);
    }
    free(seq->gates_per_layer);
    free(seq);
}

static qc_sequence_t *qc_mul_sequence_alloc(uint32_t num_layers,
                                             uint32_t gates_cap) {
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

static void mul_seq_h(qc_gate_internal_t *g, uint32_t target) {
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_H;
    g->Target = target;
}

static void mul_seq_cp(qc_gate_internal_t *g, uint32_t target,
                       uint32_t control, double value) {
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_P;
    g->GateValue = value;
    g->Target = target;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void mul_seq_cx(qc_gate_internal_t *g, uint32_t target,
                       uint32_t control) {
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
/* QFT/IQFT sub-sequence builders (local copies for this TU)               */
/* ====================================================================== */

static void mul_qft(qc_sequence_t *seq, int n) {
    for (int j = 0; j < n; ++j) {
        int q = n - 1 - j;
        uint32_t layer = seq->used_layer + (uint32_t)(2 * j);
        mul_seq_h(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)q);

        for (int i = 0; i < n - 1 - j; ++i) {
            uint32_t cp_layer = seq->used_layer + (uint32_t)(2 * j + i + 1);
            mul_seq_cp(&seq->seq[cp_layer][seq->gates_per_layer[cp_layer]++],
                       (uint32_t)q, (uint32_t)(q - i - 1),
                       M_PI / pow(2, i + 1));
        }
    }
    seq->used_layer += (uint32_t)(2 * n - 1);
}

static void mul_qft_inv(qc_sequence_t *seq, int n) {
    for (int j = 0; j < n; ++j) {
        int q = n - 1 - j;
        for (int i = 0; i < n - 1 - j; ++i) {
            uint32_t layer = seq->used_layer +
                             (uint32_t)(2 * n - 2 - (2 * j + i + 1));
            mul_seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                       (uint32_t)q, (uint32_t)(q - i - 1),
                       -M_PI / pow(2, i + 1));
        }
        uint32_t h_layer = seq->used_layer + (uint32_t)(2 * n - 2 - 2 * j);
        mul_seq_h(&seq->seq[h_layer][seq->gates_per_layer[h_layer]++],
                  (uint32_t)q);
    }
    seq->used_layer += (uint32_t)(2 * n - 1);
}

/* ====================================================================== */
/* CQ_mul sequence builder (result += target * classical_value)             */
/* ====================================================================== */

/**
 * @brief Build a CQ multiplication sequence.
 *
 * Qubit layout:
 *   [0, bits-1]:        Result register (Fourier domain target)
 *   [bits, 2*bits-1]:   Source quantum register (control qubits)
 *
 * Classical value is encoded as phase rotation angles.
 */
static qc_sequence_t *qc_seq_cq_mul(int bits, int64_t value) {
    int *bin = qc_two_complement(value, bits);
    if (bin == NULL)
        return NULL;

    /* Conservative layer estimate */
    uint32_t num_layers = QC_MUL_MAX_LAYERS;
    if (bits <= 16)
        num_layers = (uint32_t)(20 * bits * bits + 10 * bits);

    qc_sequence_t *seq = qc_mul_sequence_alloc(num_layers,
                                                (uint32_t)(10 * bits));
    if (seq == NULL) {
        free(bin);
        return NULL;
    }

    /* QFT on result register */
    mul_qft(seq, bits);
    uint32_t layer = (uint32_t)(2 * bits - 1);
    int rounds = 0;

    /* Merged CP block: for each control bit, apply phase gates to targets */
    for (int bit = bits - 1; bit >= 0; --bit) {
        layer = (uint32_t)(2 * bits + 2 * rounds - 1);
        uint32_t control = (uint32_t)(2 * bits - 1 - bit);

        for (int i = 0; i < bits - rounds; ++i) {
            uint32_t target = (uint32_t)(rounds + i);
            double phase_angle = 0;
            for (int bit_int2 = 0; bit_int2 < bits; ++bit_int2) {
                phase_angle += bin[bits - 1 - bit_int2] * 2 * M_PI /
                               pow(2, i + 1) * pow(2, bit_int2);
            }
            mul_seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                       target, control, phase_angle);
            layer++;
        }
        rounds++;
    }

    seq->used_layer = layer;
    mul_qft_inv(seq, bits);

    free(bin);
    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* QQ_mul sequence builder (result = a * b)                                */
/* ====================================================================== */

/**
 * @brief Build a QQ multiplication sequence using CCP decomposition.
 *
 * Qubit layout:
 *   [0, bits-1]:          Result register (Fourier domain)
 *   [bits, 2*bits-1]:     Operand a
 *   [2*bits, 3*bits-1]:   Operand b
 *
 * CCP(theta, a_ctrl, b_ctrl, target) decomposes into:
 *   CP(theta/2, target, a_ctrl)
 *   CX(a_ctrl, b_ctrl)
 *   CP(-theta/2, target, a_ctrl)
 *   CX(a_ctrl, b_ctrl)
 *   CP(theta/2, target, b_ctrl)
 */
static qc_sequence_t *qc_seq_qq_mul(int bits) {
    uint32_t num_layers = QC_MUL_MAX_LAYERS;
    if (bits <= 8)
        num_layers = (uint32_t)(50 * bits * bits * bits);

    qc_sequence_t *seq = qc_mul_sequence_alloc(num_layers,
                                                (uint32_t)(10 * bits));
    if (seq == NULL)
        return NULL;

    /* QFT on result register */
    mul_qft(seq, bits);
    uint32_t layer = (uint32_t)(2 * bits - 1);

    int rounds = 0;
    for (int bit = bits - 1; bit >= 0; --bit) {
        uint32_t a_ctrl = (uint32_t)(2 * bits - 1 - bit);

        /* Step 1: CP(sum_theta/2, target, a_ctrl) */
        for (int i = 0; i < bits - rounds; ++i) {
            uint32_t target = (uint32_t)(rounds + i);
            double sum_phase = M_PI * (pow(2, bits) - 1) / pow(2, i + 1);
            mul_seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                       target, a_ctrl, sum_phase);
            layer++;
        }

        /* Step 2: For each b bit: CX + negative CPs + CX */
        for (int j = 0; j < bits; ++j) {
            uint32_t b_ctrl = (uint32_t)(2 * bits + j);

            mul_seq_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
                       a_ctrl, b_ctrl);
            layer++;

            for (int i = 0; i < bits - rounds; ++i) {
                uint32_t target = (uint32_t)(rounds + i);
                double neg_phase = -M_PI * pow(2, j) / pow(2, i + 1);
                mul_seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                           target, a_ctrl, neg_phase);
                layer++;
            }

            mul_seq_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
                       a_ctrl, b_ctrl);
            layer++;
        }

        /* Step 3: CP(theta_j/2, target, b_ctrl_j) */
        for (int j = 0; j < bits; ++j) {
            uint32_t b_ctrl = (uint32_t)(2 * bits + j);

            for (int i = 0; i < bits - rounds; ++i) {
                uint32_t target = (uint32_t)(rounds + i);
                double phase = M_PI * pow(2, j) / pow(2, i + 1);
                mul_seq_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                           target, b_ctrl, phase);
                layer++;
            }
        }

        rounds++;
    }

    seq->used_layer = layer;
    mul_qft_inv(seq, bits);

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* NOTE: Controlled multiplication variants (cCQ_mul, cQQ_mul) are not     */
/* part of the current public API. They can be added in a future module     */
/* when controlled multiplication is exposed.                              */
/* ====================================================================== */

/* ====================================================================== */
/* Public API implementations                                              */
/* ====================================================================== */

qc_error_t qc_arith_qq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                            const uint32_t *a, const uint32_t *b,
                            uint32_t width) {
    if (ctx == NULL || result == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    qc_sequence_t *seq = qc_seq_qq_mul((int)width);
    if (seq == NULL)
        return QC_ERR_ALLOC;

    /* Qubit mapping: [0..w-1] -> result, [w..2w-1] -> a, [2w..3w-1] -> b */
    uint32_t qmap[192];
    for (uint32_t i = 0; i < width; ++i)
        qmap[i] = result[i];
    for (uint32_t i = 0; i < width; ++i)
        qmap[width + i] = a[i];
    for (uint32_t i = 0; i < width; ++i)
        qmap[2 * width + i] = b[i];

    qc_run_instruction(ctx, seq, qmap, 0);

    qc_mul_sequence_free(seq);
    return QC_OK;
}

qc_error_t qc_arith_cq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                            const uint32_t *target, uint32_t width,
                            int64_t value) {
    if (ctx == NULL || result == NULL || target == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    qc_sequence_t *seq = qc_seq_cq_mul((int)width, value);
    if (seq == NULL)
        return QC_ERR_ALLOC;

    /* Qubit mapping: [0..w-1] -> result, [w..2w-1] -> target */
    uint32_t qmap[128];
    for (uint32_t i = 0; i < width; ++i)
        qmap[i] = result[i];
    for (uint32_t i = 0; i < width; ++i)
        qmap[width + i] = target[i];

    qc_run_instruction(ctx, seq, qmap, 0);

    qc_mul_sequence_free(seq);
    return QC_OK;
}
