/**
 * @file logic_operations.c
 * @brief Logic and bitwise operations refactored for the micro-package backend.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/LogicOperations.c
 * Module: 1.12 (Phase 1)
 * Issue: refactor-ovp
 *
 * Provides width-parameterized bitwise operations as qc_sequence_t sequences
 * and public API wrappers that apply them to a circuit_ctx_t.
 *
 * Operations:
 *   - NOT (bitwise): X on each qubit
 *   - XOR (bitwise): CNOT from source to target
 *   - AND (bitwise): Toffoli per bit
 *   - OR  (bitwise): via De Morgan (NOT + AND + NOT)
 *
 * Thread safety: Sequence builders are stateless. Each circuit_ctx_t is
 * independent.
 */

#include "internal.h"

#include <stdint.h>

/* ====================================================================== */
/* Gate initializers for sequence building (local copies)                   */
/* ====================================================================== */

static void seq_gate_init(qc_gate_internal_t *g) {
    memset(g, 0, sizeof(qc_gate_internal_t));
    g->large_control = NULL;
}

static void seq_gate_x(qc_gate_internal_t *g, uint32_t target) {
    seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 0;
}

static void seq_gate_cx(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void seq_gate_ccx(qc_gate_internal_t *g, uint32_t target,
                          uint32_t ctrl1, uint32_t ctrl2) {
    seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 2;
    g->Control[0] = ctrl1;
    g->Control[1] = ctrl2;
}

/* ====================================================================== */
/* Sequence allocation helper                                              */
/* ====================================================================== */

static qc_sequence_t *logic_alloc_seq(int num_layers, int max_gpg) {
    if (num_layers <= 0) {
        return NULL;
    }
    qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
    if (seq == NULL) {
        return NULL;
    }
    seq->num_layer = (uint32_t)num_layers;
    seq->used_layer = 0;
    seq->total_gate_count = 0;
    seq->gates_per_layer = calloc((size_t)num_layers, sizeof(uint32_t));
    seq->seq = calloc((size_t)num_layers, sizeof(qc_gate_internal_t *));
    if (seq->gates_per_layer == NULL || seq->seq == NULL) {
        free(seq->gates_per_layer);
        free(seq->seq);
        free(seq);
        return NULL;
    }
    for (int i = 0; i < num_layers; i++) {
        seq->seq[i] = calloc((size_t)max_gpg, sizeof(qc_gate_internal_t));
        if (seq->seq[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(seq->seq[j]);
            }
            free(seq->seq);
            free(seq->gates_per_layer);
            free(seq);
            return NULL;
        }
    }
    return seq;
}

static void logic_free_seq(qc_sequence_t *seq) {
    if (seq == NULL) {
        return;
    }
    if (seq->seq != NULL) {
        for (uint32_t i = 0; i < seq->num_layer; i++) {
            free(seq->seq[i]);
        }
        free(seq->seq);
    }
    free(seq->gates_per_layer);
    free(seq);
}

/* ====================================================================== */
/* NOT sequence: X on each bit (parallel, O(1) depth)                      */
/* ====================================================================== */

/**
 * @brief Build a bitwise NOT sequence.
 *
 * Qubit layout: [0..bits-1] = target (inverted in place).
 *
 * @param bits  Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_not_seq(int bits) {
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    qc_sequence_t *seq = logic_alloc_seq(1, bits);
    if (seq == NULL) {
        return NULL;
    }

    seq->used_layer = 1;
    seq->gates_per_layer[0] = (uint32_t)bits;
    for (int i = 0; i < bits; i++) {
        seq_gate_x(&seq->seq[0][i], (uint32_t)i);
    }

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* XOR sequence: CNOT from source to target (parallel, O(1) depth)         */
/* ====================================================================== */

/**
 * @brief Build a bitwise XOR sequence: target ^= source.
 *
 * Qubit layout: [0..bits-1]=target, [bits..2*bits-1]=source.
 *
 * @param bits  Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_xor_seq(int bits) {
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    qc_sequence_t *seq = logic_alloc_seq(1, bits);
    if (seq == NULL) {
        return NULL;
    }

    seq->used_layer = 1;
    seq->gates_per_layer[0] = (uint32_t)bits;
    for (int i = 0; i < bits; i++) {
        seq_gate_cx(&seq->seq[0][i], (uint32_t)i, (uint32_t)(bits + i));
    }

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* AND sequence: Toffoli per bit, result = a & b                           */
/* ====================================================================== */

/**
 * @brief Build a bitwise AND sequence: result[i] = a[i] & b[i].
 *
 * Qubit layout: [0..bits-1]=result, [bits..2*bits-1]=a, [2*bits..3*bits-1]=b.
 * Each bit is a Toffoli gate (sequential depth).
 *
 * @param bits  Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_and_seq(int bits) {
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    qc_sequence_t *seq = logic_alloc_seq(bits, 1);
    if (seq == NULL) {
        return NULL;
    }

    seq->used_layer = (uint32_t)bits;
    for (int i = 0; i < bits; i++) {
        seq->gates_per_layer[i] = 1;
        seq_gate_ccx(&seq->seq[i][0],
                      (uint32_t)i,                     /* result[i] */
                      (uint32_t)(bits + i),             /* a[i] */
                      (uint32_t)(2 * bits + i));        /* b[i] */
    }

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* OR sequence: result = a | b via De Morgan: ~(~a & ~b)                   */
/* ====================================================================== */

/**
 * @brief Build a bitwise OR sequence: result[i] = a[i] | b[i].
 *
 * Qubit layout: [0..bits-1]=result, [bits..2*bits-1]=a, [2*bits..3*bits-1]=b.
 *
 * Uses De Morgan: a | b = ~(~a & ~b).
 * Steps: NOT a, NOT b, AND(result = a & b), NOT a, NOT b, NOT result.
 *
 * @param bits  Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_or_seq(int bits) {
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    /* Layers: 1(NOT a+b) + bits(AND) + 1(NOT a+b) + 1(NOT result) = bits+3 */
    int num_layers = bits + 3;
    int max_gpg = 2 * bits; /* layer 0 and layer bits+1 have up to 2*bits gates */

    qc_sequence_t *seq = logic_alloc_seq(num_layers, max_gpg);
    if (seq == NULL) {
        return NULL;
    }

    int layer = 0;

    /* Step 1: NOT a and NOT b (parallel) */
    for (int i = 0; i < bits; i++) {
        seq_gate_x(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)(bits + i));
    }
    for (int i = 0; i < bits; i++) {
        seq_gate_x(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)(2 * bits + i));
    }
    layer++;
    seq->used_layer++;

    /* Step 2: AND (Toffoli per bit) */
    for (int i = 0; i < bits; i++) {
        seq->gates_per_layer[layer] = 1;
        seq_gate_ccx(&seq->seq[layer][0],
                      (uint32_t)i,
                      (uint32_t)(bits + i),
                      (uint32_t)(2 * bits + i));
        layer++;
        seq->used_layer++;
    }

    /* Step 3: Uncompute NOT a and NOT b */
    for (int i = 0; i < bits; i++) {
        seq_gate_x(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)(bits + i));
    }
    for (int i = 0; i < bits; i++) {
        seq_gate_x(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)(2 * bits + i));
    }
    layer++;
    seq->used_layer++;

    /* Step 4: NOT result (invert the AND result to get OR) */
    for (int i = 0; i < bits; i++) {
        seq_gate_x(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)i);
    }
    layer++;
    seq->used_layer++;

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* Controlled NOT sequence: CX per bit (sequential, O(bits) depth)         */
/* ====================================================================== */

/**
 * @brief Build a controlled bitwise NOT sequence.
 *
 * Qubit layout: [0..bits-1] = target, [bits] = control.
 * Applies CX(target[i], control) for each bit i.
 *
 * @param bits  Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_c_not_seq(int bits) {
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    qc_sequence_t *seq = logic_alloc_seq(bits, 1);
    if (seq == NULL) {
        return NULL;
    }

    seq->used_layer = (uint32_t)bits;
    for (int i = 0; i < bits; i++) {
        seq->gates_per_layer[i] = 1;
        seq_gate_cx(&seq->seq[i][0], (uint32_t)i, (uint32_t)bits);
    }

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* Controlled XOR sequence: CCX per bit (sequential, O(bits) depth)        */
/* ====================================================================== */

/**
 * @brief Build a controlled bitwise XOR sequence: if ctrl then target ^= source.
 *
 * Qubit layout: [0..bits-1]=target, [bits..2*bits-1]=source, [2*bits]=control.
 *
 * @param bits  Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_c_xor_seq(int bits) {
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    qc_sequence_t *seq = logic_alloc_seq(bits, 1);
    if (seq == NULL) {
        return NULL;
    }

    seq->used_layer = (uint32_t)bits;
    for (int i = 0; i < bits; i++) {
        seq->gates_per_layer[i] = 1;
        seq_gate_ccx(&seq->seq[i][0],
                      (uint32_t)i,                     /* target[i] */
                      (uint32_t)(bits + i),             /* source[i] */
                      (uint32_t)(2 * bits));            /* control */
    }

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* Controlled AND sequence: MCX(3) per bit                                 */
/* ====================================================================== */

/**
 * @brief Build a controlled bitwise AND sequence.
 *
 * Qubit layout: [0..bits-1]=result, [bits..2*bits-1]=a,
 *               [2*bits..3*bits-1]=b, [3*bits]=control.
 * Each bit uses MCX with 3 controls (a[i], b[i], control) -> result[i].
 *
 * @param bits  Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_c_and_seq(int bits) {
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    /* Sequential: one MCX(3) per bit, each needs its own layer */
    qc_sequence_t *seq = logic_alloc_seq(bits, 1);
    if (seq == NULL) {
        return NULL;
    }

    seq->used_layer = (uint32_t)bits;
    for (int i = 0; i < bits; i++) {
        seq->gates_per_layer[i] = 1;
        qc_gate_internal_t *g = &seq->seq[i][0];
        seq_gate_init(g);
        g->Gate = QC_IGATE_X;
        g->Target = (uint32_t)i;                       /* result[i] */
        g->GateValue = 1;
        g->NumControls = 3;
        /* 3 controls > QC_MAX_INLINE_CONTROLS(2), must use large_control */
        g->large_control = malloc(3 * sizeof(uint32_t));
        if (g->large_control == NULL) {
            /* Cleanup on allocation failure */
            for (int j = 0; j < i; j++) {
                free(seq->seq[j][0].large_control);
            }
            for (int j = 0; j < bits; j++) {
                free(seq->seq[j]);
            }
            free(seq->seq);
            free(seq->gates_per_layer);
            free(seq);
            return NULL;
        }
        g->large_control[0] = (uint32_t)(bits + i);     /* a[i] */
        g->large_control[1] = (uint32_t)(2 * bits + i); /* b[i] */
        g->large_control[2] = (uint32_t)(3 * bits);     /* control */
    }

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* Controlled OR sequence: controlled De Morgan                            */
/* ====================================================================== */

/**
 * @brief Build a controlled bitwise OR sequence.
 *
 * Qubit layout: [0..bits-1]=result, [bits..2*bits-1]=a,
 *               [2*bits..3*bits-1]=b, [3*bits]=control.
 *
 * Uses controlled De Morgan: if ctrl then result = a | b.
 * Steps: CX(ctrl->a[i]) NOT a, CX(ctrl->b[i]) NOT b,
 *         controlled AND, uncompute NOTs, NOT result.
 *
 * @param bits  Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_c_or_seq(int bits) {
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    /* Layers: 1(controlled NOT a+b) + bits(controlled AND) +
               1(controlled un-NOT a+b) + 1(controlled NOT result) = bits+3 */
    int num_layers = bits + 3;
    int max_gpg = 2 * bits;

    qc_sequence_t *seq = logic_alloc_seq(num_layers, max_gpg > 1 ? max_gpg : 1);
    if (seq == NULL) {
        return NULL;
    }

    int layer = 0;
    uint32_t ctrl = (uint32_t)(3 * bits);

    /* Step 1: Controlled NOT a and NOT b */
    for (int i = 0; i < bits; i++) {
        seq_gate_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
                     (uint32_t)(bits + i), ctrl);
    }
    for (int i = 0; i < bits; i++) {
        seq_gate_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
                     (uint32_t)(2 * bits + i), ctrl);
    }
    layer++;
    seq->used_layer++;

    /* Step 2: Controlled AND (MCX(3) per bit) */
    for (int i = 0; i < bits; i++) {
        seq->gates_per_layer[layer] = 1;
        qc_gate_internal_t *g = &seq->seq[layer][0];
        seq_gate_init(g);
        g->Gate = QC_IGATE_X;
        g->Target = (uint32_t)i;
        g->GateValue = 1;
        g->NumControls = 3;
        /* 3 controls > QC_MAX_INLINE_CONTROLS(2), must use large_control */
        g->large_control = malloc(3 * sizeof(uint32_t));
        if (g->large_control == NULL) {
            /* Simplified cleanup: qc_sequence_free handles partial sequences */
            qc_sequence_compute_total_gate_count(seq);
            qc_sequence_free(seq);
            return NULL;
        }
        g->large_control[0] = (uint32_t)(bits + i);
        g->large_control[1] = (uint32_t)(2 * bits + i);
        g->large_control[2] = ctrl;
        layer++;
        seq->used_layer++;
    }

    /* Step 3: Uncompute controlled NOT a and NOT b */
    for (int i = 0; i < bits; i++) {
        seq_gate_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
                     (uint32_t)(bits + i), ctrl);
    }
    for (int i = 0; i < bits; i++) {
        seq_gate_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
                     (uint32_t)(2 * bits + i), ctrl);
    }
    layer++;
    seq->used_layer++;

    /* Step 4: Controlled NOT result */
    for (int i = 0; i < bits; i++) {
        seq_gate_cx(&seq->seq[layer][seq->gates_per_layer[layer]++],
                     (uint32_t)i, ctrl);
    }
    layer++;
    seq->used_layer++;

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* Public API wrappers                                                     */
/* ====================================================================== */

qc_error_t qc_bitwise_not(circuit_ctx_t *ctx, const uint32_t *target,
                           uint32_t width) {
    if (ctx == NULL || target == NULL) {
        return QC_ERR_NULL;
    }
    if (width < 1 || width > 64) {
        return QC_ERR_WIDTH;
    }

    qc_sequence_t *seq = qc_not_seq((int)width);
    if (seq == NULL) {
        return QC_ERR_ALLOC;
    }

    /* Qubit mapping: abstract[i] = target[i] */
    qc_run_instruction(ctx, seq, target, 0);
    logic_free_seq(seq);
    return QC_OK;
}

qc_error_t qc_bitwise_xor(circuit_ctx_t *ctx, const uint32_t *a,
                           const uint32_t *b, uint32_t width) {
    if (ctx == NULL || a == NULL || b == NULL) {
        return QC_ERR_NULL;
    }
    if (width < 1 || width > 64) {
        return QC_ERR_WIDTH;
    }

    qc_sequence_t *seq = qc_xor_seq((int)width);
    if (seq == NULL) {
        return QC_ERR_ALLOC;
    }

    /* Qubit mapping: abstract[0..w-1]=a, abstract[w..2w-1]=b */
    uint32_t qubit_array[128];
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i] = a[i];
    }
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[width + i] = b[i];
    }

    qc_run_instruction(ctx, seq, qubit_array, 0);
    logic_free_seq(seq);
    return QC_OK;
}

qc_error_t qc_bitwise_and(circuit_ctx_t *ctx, const uint32_t *result,
                           const uint32_t *a, const uint32_t *b,
                           uint32_t width) {
    if (ctx == NULL || result == NULL || a == NULL || b == NULL) {
        return QC_ERR_NULL;
    }
    if (width < 1 || width > 64) {
        return QC_ERR_WIDTH;
    }

    qc_sequence_t *seq = qc_and_seq((int)width);
    if (seq == NULL) {
        return QC_ERR_ALLOC;
    }

    /* Qubit mapping: abstract[0..w-1]=result, [w..2w-1]=a, [2w..3w-1]=b */
    uint32_t qubit_array[192];
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i] = result[i];
    }
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[width + i] = a[i];
    }
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[2 * width + i] = b[i];
    }

    qc_run_instruction(ctx, seq, qubit_array, 0);
    logic_free_seq(seq);
    return QC_OK;
}

qc_error_t qc_bitwise_or(circuit_ctx_t *ctx, const uint32_t *result,
                          const uint32_t *a, const uint32_t *b,
                          uint32_t width) {
    if (ctx == NULL || result == NULL || a == NULL || b == NULL) {
        return QC_ERR_NULL;
    }
    if (width < 1 || width > 64) {
        return QC_ERR_WIDTH;
    }

    qc_sequence_t *seq = qc_or_seq((int)width);
    if (seq == NULL) {
        return QC_ERR_ALLOC;
    }

    uint32_t qubit_array[192];
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i] = result[i];
    }
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[width + i] = a[i];
    }
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[2 * width + i] = b[i];
    }

    qc_run_instruction(ctx, seq, qubit_array, 0);
    logic_free_seq(seq);
    return QC_OK;
}
