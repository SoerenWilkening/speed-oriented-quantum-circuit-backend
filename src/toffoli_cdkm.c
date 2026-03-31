/**
 * @file toffoli_cdkm.c
 * @brief Dynamic CDKM ripple-carry adder for all widths (1-64).
 *
 * Module 1.9 (Phase 1) - refactor-w6f
 *
 * Implements the Cuccaro-Draper-Kutin-Moulton (CDKM) ripple-carry adder
 * using MAJ (Majority) and UMA (UnMajority-and-Add) gate chains.
 *
 * Replaces the monolith's ToffoliAdditionCDKM.c, removing all hardcoded
 * sequence dependencies (toffoli_add_seq_*.c, toffoli_decomp_seq_*.c).
 * Dynamic generation works for all widths 1-64.
 *
 * All public functions take circuit_ctx_t* ctx, producing qc_sequence_t
 * sequences that are applied via qc_run_instruction().
 *
 * Provides:
 *   - qc_toffoli_qq_add:  Quantum-quantum addition (a += b)
 *   - qc_toffoli_cq_add:  Classical-quantum addition (target += value)
 *   - qc_toffoli_cqq_add: Controlled QQ addition (a += b, ctrl)
 *   - qc_toffoli_ccq_add: Controlled CQ addition (target += value, ctrl)
 *
 * References:
 *   Cuccaro et al., "A new quantum ripple-carry addition circuit" (2004)
 *   arXiv:quant-ph/0410184
 */

#include "internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* External helpers (toffoli_helpers.c)                                     */
/* ====================================================================== */

/* qc_toffoli_seq_alloc, qc_toffoli_seq_free, qc_two_complement
 * are declared in internal.h */

/* ====================================================================== */
/* Precompiled caches (separate from QFT cache, per-width)                 */
/* ====================================================================== */

static qc_sequence_t *cache_qq_add[65]  = {NULL};
static qc_sequence_t *cache_cqq_add[65] = {NULL};

/* ====================================================================== */
/* Gate-into-sequence helpers                                              */
/* ====================================================================== */

/** @brief Initialize a gate struct to zero. */
static void seq_gate_init(qc_gate_internal_t *g) {
    memset(g, 0, sizeof(qc_gate_internal_t));
    g->large_control = NULL;
}

/** @brief Emit X gate into sequence at current layer. */
static void seq_x(qc_gate_internal_t *g, uint32_t target) {
    seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 0;
}

/** @brief Emit CX gate into sequence at current layer. */
static void seq_cx(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 1;
    g->Control[0] = control;
}

/** @brief Emit CCX gate into sequence at current layer. */
static void seq_ccx(qc_gate_internal_t *g, uint32_t target,
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
/* Convenience macros for emitting into a sequence                         */
/* ====================================================================== */

#define EMIT_X(seq, layer, tgt) \
    seq_x(&(seq)->seq[*(layer)][(seq)->gates_per_layer[*(layer)]++], (tgt)); \
    (*(layer))++

#define EMIT_CX(seq, layer, tgt, ctrl) \
    seq_cx(&(seq)->seq[*(layer)][(seq)->gates_per_layer[*(layer)]++], (tgt), (ctrl)); \
    (*(layer))++

#define EMIT_CCX(seq, layer, tgt, c1, c2) \
    seq_ccx(&(seq)->seq[*(layer)][(seq)->gates_per_layer[*(layer)]++], (tgt), (c1), (c2)); \
    (*(layer))++

/* ====================================================================== */
/* MAJ / UMA primitives                                                    */
/* ====================================================================== */

/**
 * MAJ(a, b, c):  CX(b,c), CX(a,c), CCX(c, a, b)
 * After: c holds carry, a and b modified.
 */
static void emit_MAJ(qc_sequence_t *seq, int *layer, int a, int b, int c) {
    EMIT_CX(seq, layer, (uint32_t)b, (uint32_t)c);
    EMIT_CX(seq, layer, (uint32_t)a, (uint32_t)c);
    EMIT_CCX(seq, layer, (uint32_t)c, (uint32_t)a, (uint32_t)b);
}

/**
 * UMA(a, b, c):  CCX(c, a, b), CX(a, c), CX(b, a)
 * After: a restored, b = sum bit, c restored.
 */
static void emit_UMA(qc_sequence_t *seq, int *layer, int a, int b, int c) {
    EMIT_CCX(seq, layer, (uint32_t)c, (uint32_t)a, (uint32_t)b);
    EMIT_CX(seq, layer, (uint32_t)a, (uint32_t)c);
    EMIT_CX(seq, layer, (uint32_t)b, (uint32_t)a);
}

/* ====================================================================== */
/* Controlled MAJ / UMA (AND-ancilla MCX decomposition)                    */
/* ====================================================================== */

/**
 * cMAJ(a, b, c, ext_ctrl, and_anc):
 *   CCX(b, c, ext_ctrl), CCX(a, c, ext_ctrl),
 *   CCX(and_anc, a, b), CCX(c, and_anc, ext_ctrl), CCX(and_anc, a, b)
 * 5 CCX gates, zero MCX.
 */
static void emit_cMAJ(qc_sequence_t *seq, int *layer,
                       int a, int b, int c, int ext_ctrl, int and_anc) {
    EMIT_CCX(seq, layer, (uint32_t)b, (uint32_t)c, (uint32_t)ext_ctrl);
    EMIT_CCX(seq, layer, (uint32_t)a, (uint32_t)c, (uint32_t)ext_ctrl);
    EMIT_CCX(seq, layer, (uint32_t)and_anc, (uint32_t)a, (uint32_t)b);
    EMIT_CCX(seq, layer, (uint32_t)c, (uint32_t)and_anc, (uint32_t)ext_ctrl);
    EMIT_CCX(seq, layer, (uint32_t)and_anc, (uint32_t)a, (uint32_t)b);
}

/**
 * cUMA(a, b, c, ext_ctrl, and_anc):
 *   CCX(and_anc, a, b), CCX(c, and_anc, ext_ctrl), CCX(and_anc, a, b),
 *   CCX(a, c, ext_ctrl), CCX(b, a, ext_ctrl)
 * 5 CCX gates, zero MCX.
 */
static void emit_cUMA(qc_sequence_t *seq, int *layer,
                       int a, int b, int c, int ext_ctrl, int and_anc) {
    EMIT_CCX(seq, layer, (uint32_t)and_anc, (uint32_t)a, (uint32_t)b);
    EMIT_CCX(seq, layer, (uint32_t)c, (uint32_t)and_anc, (uint32_t)ext_ctrl);
    EMIT_CCX(seq, layer, (uint32_t)and_anc, (uint32_t)a, (uint32_t)b);
    EMIT_CCX(seq, layer, (uint32_t)a, (uint32_t)c, (uint32_t)ext_ctrl);
    EMIT_CCX(seq, layer, (uint32_t)b, (uint32_t)a, (uint32_t)ext_ctrl);
}

/* ====================================================================== */
/* CQ MAJ (classical-bit gate simplification)                              */
/* ====================================================================== */

/**
 * Emit CQ-simplified MAJ based on known classical bit value.
 * - bit=0, a_known_zero: entire MAJ eliminated (NOP)
 * - bit=0, !a_known_zero: emit CCX only (skip 2 CX)
 * - bit=1: X(c), X(b), X(a), CCX(c, a, b) (fold X-init)
 */
static void emit_CQ_MAJ(qc_sequence_t *seq, int *layer,
                         int a, int b, int c,
                         int classical_bit_c, int a_known_zero) {
    if (classical_bit_c == 0) {
        if (!a_known_zero) {
            EMIT_CCX(seq, layer, (uint32_t)c, (uint32_t)a, (uint32_t)b);
        }
        /* else: entirely eliminated */
    } else {
        EMIT_X(seq, layer, (uint32_t)c);
        EMIT_X(seq, layer, (uint32_t)b);
        EMIT_X(seq, layer, (uint32_t)a);
        EMIT_CCX(seq, layer, (uint32_t)c, (uint32_t)a, (uint32_t)b);
    }
}

/** @brief Count layers for CQ CDKM based on classical bit pattern. */
static int compute_CQ_layer_count(int bits, const int *bin) {
    int layers = 0;

    /* Forward MAJ sweep */
    int bit0 = bin[bits - 1]; /* LSB */
    if (bit0 == 1) layers += 4;
    /* bit0 == 0: first MAJ entirely eliminated */

    for (int i = 1; i < bits; i++) {
        int bit_i = bin[bits - 1 - i];
        layers += (bit_i == 0) ? 1 : 4;
    }

    /* Reverse UMA sweep: 3 * bits */
    layers += 3 * bits;

    /* Cleanup X gates for bit=1 positions */
    for (int i = 0; i < bits; i++) {
        if (bin[bits - 1 - i] == 1)
            layers++;
    }

    return layers;
}

/* ====================================================================== */
/* cCQ cMAJ (controlled classical-bit gate simplification)                 */
/* ====================================================================== */

/**
 * Emit cCQ-simplified controlled MAJ with AND-ancilla decomposition.
 * - bit=0, a_known_zero: entire cMAJ eliminated
 * - bit=0, !a_known_zero: AND-ancilla decomp (3 CCX)
 * - bit=1: CX-init, then standard cMAJ (1 CX + 5 CCX = 6 layers)
 */
static void emit_cCQ_MAJ(qc_sequence_t *seq, int *layer,
                          int a, int b, int c,
                          int classical_bit_c, int a_known_zero,
                          int ext_ctrl, int and_anc) {
    if (classical_bit_c == 0) {
        if (!a_known_zero) {
            EMIT_CCX(seq, layer, (uint32_t)and_anc, (uint32_t)a, (uint32_t)b);
            EMIT_CCX(seq, layer, (uint32_t)c, (uint32_t)and_anc, (uint32_t)ext_ctrl);
            EMIT_CCX(seq, layer, (uint32_t)and_anc, (uint32_t)a, (uint32_t)b);
        }
        /* else: entirely eliminated */
    } else {
        EMIT_CX(seq, layer, (uint32_t)c, (uint32_t)ext_ctrl);
        emit_cMAJ(seq, layer, a, b, c, ext_ctrl, and_anc);
    }
}

/** @brief Count layers for cCQ CDKM based on classical bit pattern. */
static int compute_cCQ_layer_count(int bits, const int *bin) {
    int layers = 0;

    int bit0 = bin[bits - 1];
    if (bit0 == 1) layers += 6;
    /* bit0 == 0: first cMAJ eliminated */

    for (int i = 1; i < bits; i++) {
        int bit_i = bin[bits - 1 - i];
        layers += (bit_i == 0) ? 3 : 6;
    }

    /* Reverse cUMA sweep: 5 * bits */
    layers += 5 * bits;

    /* Cleanup CX gates for bit=1 positions */
    for (int i = 0; i < bits; i++) {
        if (bin[bits - 1 - i] == 1)
            layers++;
    }

    return layers;
}

/* ====================================================================== */
/* Public API: qc_toffoli_qq_add                                           */
/* ====================================================================== */

qc_error_t qc_toffoli_qq_add(circuit_ctx_t *ctx, const uint32_t *a,
                              const uint32_t *b, uint32_t width) {
    if (ctx == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    int bits = (int)width;

    /* Check cache */
    if (cache_qq_add[bits] == NULL) {
        if (bits == 1) {
            /* Single CNOT: a[0] ^= b[0] */
            qc_sequence_t *seq = qc_toffoli_seq_alloc(1);
            if (seq == NULL)
                return QC_ERR_ALLOC;
            int layer = 0;
            EMIT_CX(seq, &layer, 0, 1);
            seq->used_layer = (uint32_t)layer;
            qc_sequence_compute_total_gate_count(seq);
            cache_qq_add[bits] = seq;
        } else {
            /* General CDKM: 6*bits layers */
            int num_layers = 6 * bits;
            qc_sequence_t *seq = qc_toffoli_seq_alloc(num_layers);
            if (seq == NULL)
                return QC_ERR_ALLOC;

            int layer = 0;
            int ancilla = 2 * bits; /* carry qubit */

            /* Forward MAJ sweep */
            emit_MAJ(seq, &layer, ancilla, bits + 0, 0);
            for (int i = 1; i < bits; i++)
                emit_MAJ(seq, &layer, i - 1, bits + i, i);

            /* Reverse UMA sweep */
            for (int i = bits - 1; i >= 1; i--)
                emit_UMA(seq, &layer, i - 1, bits + i, i);
            emit_UMA(seq, &layer, ancilla, bits + 0, 0);

            seq->used_layer = (uint32_t)layer;
            qc_sequence_compute_total_gate_count(seq);
            cache_qq_add[bits] = seq;
        }
    }

    /* Build qubit mapping */
    uint32_t total_qubits = (width == 1) ? 2 * width : 2 * width + 1;
    uint32_t *qmap = malloc(total_qubits * sizeof(uint32_t));
    if (qmap == NULL)
        return QC_ERR_ALLOC;

    for (uint32_t i = 0; i < width; i++)
        qmap[i] = a[i];
    for (uint32_t i = 0; i < width; i++)
        qmap[width + i] = b[i];

    if (width >= 2) {
        /* Allocate ancilla carry qubit */
        uint32_t anc;
        qc_error_t err = qc_qubit_alloc(ctx, &anc);
        if (err != QC_OK) {
            free(qmap);
            return err;
        }
        qmap[2 * width] = anc;

        qc_run_instruction(ctx, cache_qq_add[bits], qmap, 0);

        qc_qubit_free(ctx, anc);
    } else {
        qc_run_instruction(ctx, cache_qq_add[bits], qmap, 0);
    }

    free(qmap);
    return QC_OK;
}

/* ====================================================================== */
/* Public API: qc_toffoli_cq_add                                           */
/* ====================================================================== */

qc_error_t qc_toffoli_cq_add(circuit_ctx_t *ctx, const uint32_t *target,
                              uint32_t width, int64_t value) {
    if (ctx == NULL || target == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    int bits = (int)width;
    int *bin = qc_two_complement(value, bits);
    if (bin == NULL)
        return QC_ERR_ALLOC;

    /* 1-bit special case */
    if (bits == 1) {
        if (bin[0] == 1) {
            /* X(target[0]) */
            qc_circuit_x(ctx, target[0]);
        }
        /* else identity */
        free(bin);
        return QC_OK;
    }

    /* General CQ: allocate temp register + carry ancilla */
    uint32_t temp_start;
    qc_error_t err = qc_qubit_alloc_n(ctx, (uint32_t)bits, &temp_start);
    if (err != QC_OK) {
        free(bin);
        return err;
    }
    uint32_t carry;
    err = qc_qubit_alloc(ctx, &carry);
    if (err != QC_OK) {
        qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
        free(bin);
        return err;
    }

    /* Build sequence (not cached: value-dependent) */
    int num_layers = compute_CQ_layer_count(bits, bin);
    if (num_layers == 0) {
        /* All-zero value: identity */
        qc_qubit_free(ctx, carry);
        qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
        free(bin);
        return QC_OK;
    }

    qc_sequence_t *seq = qc_toffoli_seq_alloc(num_layers);
    if (seq == NULL) {
        qc_qubit_free(ctx, carry);
        qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
        free(bin);
        return QC_ERR_ALLOC;
    }

    int layer = 0;

    /* Forward CQ MAJ sweep */
    emit_CQ_MAJ(seq, &layer, 2 * bits, bits + 0, 0, bin[bits - 1], 1);
    for (int i = 1; i < bits; i++)
        emit_CQ_MAJ(seq, &layer, i - 1, bits + i, i, bin[bits - 1 - i], 0);

    /* Reverse UMA sweep (standard) */
    for (int i = bits - 1; i >= 1; i--)
        emit_UMA(seq, &layer, i - 1, bits + i, i);
    emit_UMA(seq, &layer, 2 * bits, bits + 0, 0);

    /* Cleanup X gates for bit=1 positions */
    for (int i = 0; i < bits; i++) {
        if (bin[bits - 1 - i] == 1) {
            EMIT_X(seq, &layer, (uint32_t)i);
        }
    }

    seq->used_layer = (uint32_t)layer;
    qc_sequence_compute_total_gate_count(seq);

    /* Build qubit mapping:
     * [0..bits-1] = temp, [bits..2*bits-1] = self, [2*bits] = carry */
    uint32_t total_q = 2 * (uint32_t)bits + 1;
    uint32_t *qmap = malloc(total_q * sizeof(uint32_t));
    if (qmap == NULL) {
        qc_toffoli_seq_free(seq);
        qc_qubit_free(ctx, carry);
        qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
        free(bin);
        return QC_ERR_ALLOC;
    }

    for (int i = 0; i < bits; i++)
        qmap[i] = temp_start + (uint32_t)i;
    for (int i = 0; i < bits; i++)
        qmap[bits + i] = target[i];
    qmap[2 * bits] = carry;

    qc_run_instruction(ctx, seq, qmap, 0);

    free(qmap);
    qc_toffoli_seq_free(seq);
    qc_qubit_free(ctx, carry);
    qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
    free(bin);
    return QC_OK;
}

/* ====================================================================== */
/* Public API: qc_toffoli_cqq_add                                         */
/* ====================================================================== */

qc_error_t qc_toffoli_cqq_add(circuit_ctx_t *ctx, const uint32_t *a,
                               const uint32_t *b, uint32_t width,
                               uint32_t control) {
    if (ctx == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    int bits = (int)width;

    /* Check cache */
    if (cache_cqq_add[bits] == NULL) {
        if (bits == 1) {
            /* Single CCX: a[0] ^= b[0] controlled by ext_ctrl */
            qc_sequence_t *seq = qc_toffoli_seq_alloc(1);
            if (seq == NULL)
                return QC_ERR_ALLOC;
            int layer = 0;
            EMIT_CCX(seq, &layer, 0, 1, 2);
            seq->used_layer = (uint32_t)layer;
            qc_sequence_compute_total_gate_count(seq);
            cache_cqq_add[bits] = seq;
        } else {
            /* 10*bits layers: 5n cMAJ + 5n cUMA */
            int num_layers = 10 * bits;
            qc_sequence_t *seq = qc_toffoli_seq_alloc(num_layers);
            if (seq == NULL)
                return QC_ERR_ALLOC;

            int layer = 0;
            int ancilla  = 2 * bits;
            int ext_ctrl = 2 * bits + 1;
            int and_anc  = 2 * bits + 2;

            /* Forward cMAJ sweep */
            emit_cMAJ(seq, &layer, ancilla, bits + 0, 0, ext_ctrl, and_anc);
            for (int i = 1; i < bits; i++)
                emit_cMAJ(seq, &layer, i - 1, bits + i, i, ext_ctrl, and_anc);

            /* Reverse cUMA sweep */
            for (int i = bits - 1; i >= 1; i--)
                emit_cUMA(seq, &layer, i - 1, bits + i, i, ext_ctrl, and_anc);
            emit_cUMA(seq, &layer, ancilla, bits + 0, 0, ext_ctrl, and_anc);

            seq->used_layer = (uint32_t)layer;
            qc_sequence_compute_total_gate_count(seq);
            cache_cqq_add[bits] = seq;
        }
    }

    /* Build qubit mapping */
    if (bits == 1) {
        /* [0]=a, [1]=b, [2]=ext_ctrl */
        uint32_t qmap[3] = {a[0], b[0], control};
        qc_run_instruction(ctx, cache_cqq_add[bits], qmap, 0);
    } else {
        /* [0..bits-1]=a, [bits..2*bits-1]=b, [2*bits]=carry_anc,
         * [2*bits+1]=ext_ctrl, [2*bits+2]=and_anc */
        uint32_t total_q = 2 * width + 3;
        uint32_t *qmap = malloc(total_q * sizeof(uint32_t));
        if (qmap == NULL)
            return QC_ERR_ALLOC;

        for (uint32_t i = 0; i < width; i++)
            qmap[i] = a[i];
        for (uint32_t i = 0; i < width; i++)
            qmap[width + i] = b[i];

        /* Allocate carry ancilla + AND ancilla */
        uint32_t anc_start;
        qc_error_t err = qc_qubit_alloc_n(ctx, 2, &anc_start);
        if (err != QC_OK) {
            free(qmap);
            return err;
        }
        qmap[2 * width]     = anc_start;     /* carry */
        qmap[2 * width + 1] = control;       /* ext_ctrl */
        qmap[2 * width + 2] = anc_start + 1; /* AND ancilla */

        qc_run_instruction(ctx, cache_cqq_add[bits], qmap, 0);

        qc_qubit_free_n(ctx, anc_start, 2);
        free(qmap);
    }

    return QC_OK;
}

/* ====================================================================== */
/* Public API: qc_toffoli_ccq_add                                          */
/* ====================================================================== */

qc_error_t qc_toffoli_ccq_add(circuit_ctx_t *ctx, const uint32_t *target,
                               uint32_t width, int64_t value,
                               uint32_t control) {
    if (ctx == NULL || target == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    int bits = (int)width;
    int *bin = qc_two_complement(value, bits);
    if (bin == NULL)
        return QC_ERR_ALLOC;

    /* 1-bit special case */
    if (bits == 1) {
        if (bin[0] == 1) {
            /* CX(target=self[0], control=ext_ctrl) */
            qc_circuit_cx(ctx, control, target[0]);
        }
        /* else identity */
        free(bin);
        return QC_OK;
    }

    /* Allocate temp register + carry + AND ancilla */
    uint32_t temp_start;
    qc_error_t err = qc_qubit_alloc_n(ctx, (uint32_t)bits, &temp_start);
    if (err != QC_OK) {
        free(bin);
        return err;
    }
    uint32_t anc_start;
    err = qc_qubit_alloc_n(ctx, 2, &anc_start);
    if (err != QC_OK) {
        qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
        free(bin);
        return err;
    }

    /* Build value-dependent sequence (not cached) */
    int num_layers = compute_cCQ_layer_count(bits, bin);
    if (num_layers == 0) {
        qc_qubit_free_n(ctx, anc_start, 2);
        qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
        free(bin);
        return QC_OK;
    }

    qc_sequence_t *seq = qc_toffoli_seq_alloc(num_layers);
    if (seq == NULL) {
        qc_qubit_free_n(ctx, anc_start, 2);
        qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
        free(bin);
        return QC_ERR_ALLOC;
    }

    int layer = 0;
    int carry_idx    = 2 * bits;
    int ext_ctrl_idx = 2 * bits + 1;
    int and_anc_idx  = 2 * bits + 2;

    /* Forward cCQ MAJ sweep */
    emit_cCQ_MAJ(seq, &layer, carry_idx, bits + 0, 0,
                 bin[bits - 1], 1, ext_ctrl_idx, and_anc_idx);
    for (int i = 1; i < bits; i++)
        emit_cCQ_MAJ(seq, &layer, i - 1, bits + i, i,
                     bin[bits - 1 - i], 0, ext_ctrl_idx, and_anc_idx);

    /* Reverse cUMA sweep */
    for (int i = bits - 1; i >= 1; i--)
        emit_cUMA(seq, &layer, i - 1, bits + i, i, ext_ctrl_idx, and_anc_idx);
    emit_cUMA(seq, &layer, carry_idx, bits + 0, 0, ext_ctrl_idx, and_anc_idx);

    /* Cleanup CX gates for bit=1 positions */
    for (int i = 0; i < bits; i++) {
        if (bin[bits - 1 - i] == 1) {
            EMIT_CX(seq, &layer, (uint32_t)i, (uint32_t)ext_ctrl_idx);
        }
    }

    seq->used_layer = (uint32_t)layer;
    qc_sequence_compute_total_gate_count(seq);

    /* Build qubit mapping */
    uint32_t total_q = 2 * (uint32_t)bits + 3;
    uint32_t *qmap = malloc(total_q * sizeof(uint32_t));
    if (qmap == NULL) {
        qc_toffoli_seq_free(seq);
        qc_qubit_free_n(ctx, anc_start, 2);
        qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
        free(bin);
        return QC_ERR_ALLOC;
    }

    for (int i = 0; i < bits; i++)
        qmap[i] = temp_start + (uint32_t)i;
    for (int i = 0; i < bits; i++)
        qmap[bits + i] = target[i];
    qmap[2 * bits]     = anc_start;     /* carry */
    qmap[2 * bits + 1] = control;       /* ext_ctrl */
    qmap[2 * bits + 2] = anc_start + 1; /* AND ancilla */

    qc_run_instruction(ctx, seq, qmap, 0);

    free(qmap);
    qc_toffoli_seq_free(seq);
    qc_qubit_free_n(ctx, anc_start, 2);
    qc_qubit_free_n(ctx, temp_start, (uint32_t)bits);
    free(bin);
    return QC_OK;
}
