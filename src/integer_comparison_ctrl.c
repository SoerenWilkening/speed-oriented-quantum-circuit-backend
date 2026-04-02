/**
 * @file integer_comparison_ctrl.c
 * @brief Controlled CQ comparison sequence builders and public API wrappers.
 *
 * Split from integer_comparison.c to stay under 500-line limit.
 * Issue: refactor-4xn
 *
 * Contains:
 *   - Controlled CQ equality/less/greater sequence builders
 *   - Public API wrappers for CQ equality/less/greater
 */

#include "comparison_internal.h"

/* ====================================================================== */
/* Extern declarations for uncontrolled _seq builders (in integer_comparison.c) */
/* ====================================================================== */

extern qc_sequence_t *qc_cmp_cq_equal_seq(int bits, int64_t value);
extern qc_sequence_t *qc_cmp_cq_less_seq(int bits, int64_t value);
extern qc_sequence_t *qc_cmp_cq_greater_seq(int bits, int64_t value);

/* ====================================================================== */
/* Controlled CQ equality: result = (A == value) conditioned on ctrl       */
/* ====================================================================== */

/**
 * @brief Build a controlled CQ equality comparison sequence.
 *
 * Same algorithm as qc_cmp_cq_equal_seq but X gates become CX(target, ctrl)
 * and the MCX gets one extra control.
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1]=control
 * For bits >= 2: [bits+2..] = AND-ancilla
 *
 * @param bits   Width of quantum operand (1-64).
 * @param value  Classical value to compare.
 * @return Sequence (caller must free), or NULL on error.
 */
qc_sequence_t *qc_c_cmp_cq_equal_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 64) {
        return NULL;
    }

    /* Range check */
    uint64_t max_val = (bits == 64) ? UINT64_MAX : ((1ULL << bits) - 1);
    if (value < 0) {
        int64_t min_val = -(1LL << (bits - 1));
        if (value < min_val) {
            qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
            return seq;
        }
    } else {
        if ((uint64_t)value > max_val) {
            qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
            return seq;
        }
    }

    int *bin = qc_two_complement(value, bits);
    if (bin == NULL) {
        return NULL;
    }

    int num_cx_gates = 0;
    for (int i = 0; i < bits; i++) {
        if (bin[i] == 0) {
            num_cx_gates++;
        }
    }

    /* MCX needs bits+1 controls (operand bits + external ctrl) */
    int mcx_layers;
    if (bits == 1) {
        mcx_layers = 1;  /* CCX */
    } else {
        mcx_layers = qc_cmp_mcx_decomp_layers(bits + 1);
    }
    int num_layers = num_cx_gates + mcx_layers + num_cx_gates;

    qc_sequence_t *seq = qc_cmp_alloc_sequence(num_layers, (int)bits + 2);
    if (seq == NULL) {
        free(bin);
        return NULL;
    }

    int current_layer = 0;
    int control_qubit = bits + 1;

    /* Phase 1: Controlled X (CX) gates where classical bit is 0 */
    for (int i = 0; i < bits; i++) {
        if (bin[i] == 0) {
            qc_cmp_seq_gate_cx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                            (uint32_t)(i + 1), (uint32_t)control_qubit);
            seq->gates_per_layer[current_layer]++;
            current_layer++;
            seq->used_layer++;
        }
    }

    /* Phase 2: Controlled MCX to set result qubit */
    if (bits == 1) {
        /* CCX with control_qubit and qubit[1] controlling qubit[0] */
        qc_cmp_seq_gate_ccx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                         0, (uint32_t)control_qubit, 1);
        seq->gates_per_layer[current_layer]++;
        current_layer++;
        seq->used_layer++;
    } else {
        /* MCX(bits+1) decomposed via AND-ancilla */
        uint32_t controls[128];
        controls[0] = (uint32_t)control_qubit;
        for (int i = 0; i < bits; i++) {
            controls[i + 1] = (uint32_t)(i + 1);
        }
        int anc_start = bits + 2;
        qc_cmp_emit_mcx_decomp(seq, &current_layer, 0, controls, bits + 1, anc_start);
    }

    /* Phase 3: Uncompute controlled X gates */
    for (int i = 0; i < bits; i++) {
        if (bin[i] == 0) {
            qc_cmp_seq_gate_cx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                            (uint32_t)(i + 1), (uint32_t)control_qubit);
            seq->gates_per_layer[current_layer]++;
            current_layer++;
            seq->used_layer++;
        }
    }

    free(bin);
    qc_sequence_compute_total_gate_count(seq);
    seq->total_qubits = bits == 1 ? bits + 2 : 2 * bits + 1;
    return seq;
}

/* ====================================================================== */
/* Controlled CQ less-than: result = (A < value) conditioned on ctrl       */
/* ====================================================================== */

/**
 * @brief Build a controlled CQ less-than comparison sequence.
 *
 * Same borrow-ancilla algorithm as qc_cmp_cq_less_seq but the CX that
 * copies the borrow to the result becomes a CCX(result, borrow, ctrl).
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1]=borrow, [bits+2]=control
 *
 * @param bits   Width of quantum operand A (1-63).
 * @param value  Classical value to compare against.
 * @return Sequence (caller must free), or NULL on error.
 */
qc_sequence_t *qc_c_cmp_cq_less_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 63) {
        return NULL;
    }

    qc_sequence_t *sub_seq = qc_split_cq_sub_seq(bits, value);
    qc_sequence_t *add_seq = qc_split_cq_add_seq(bits, value);
    if (sub_seq == NULL || add_seq == NULL) {
        qc_sequence_free(sub_seq);
        qc_sequence_free(add_seq);
        return NULL;
    }

    int total_layers = (int)sub_seq->used_layer + 1 + (int)add_seq->used_layer;
    int max_gpg = bits + 3;

    qc_sequence_t *seq = qc_cmp_alloc_sequence(total_layers, max_gpg);
    if (seq == NULL) {
        qc_sequence_free(sub_seq);
        qc_sequence_free(add_seq);
        return NULL;
    }

    int current_layer = 0;

    /* Step 1: Subtract -- copy sub_seq layers with qubit offset +1 */
    for (int l = 0; l < (int)sub_seq->used_layer; l++) {
        for (uint32_t g = 0; g < sub_seq->gates_per_layer[l]; g++) {
            qc_gate_internal_t *dg = &seq->seq[current_layer][seq->gates_per_layer[current_layer]];
            memcpy(dg, &sub_seq->seq[l][g], sizeof(qc_gate_internal_t));
            dg->Target += 1;
            for (uint32_t c = 0; c < dg->NumControls; c++) {
                dg->Control[c] += 1;
            }
            seq->gates_per_layer[current_layer]++;
        }
        current_layer++;
        seq->used_layer++;
    }

    /* Step 2: CCX(target=0, control1=bits+1, control2=bits+2) -- controlled copy borrow to result */
    qc_cmp_seq_gate_ccx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]++],
                     0, (uint32_t)(bits + 1), (uint32_t)(bits + 2));
    current_layer++;
    seq->used_layer++;

    /* Step 3: Add back */
    for (int l = 0; l < (int)add_seq->used_layer; l++) {
        for (uint32_t g = 0; g < add_seq->gates_per_layer[l]; g++) {
            qc_gate_internal_t *dg = &seq->seq[current_layer][seq->gates_per_layer[current_layer]];
            memcpy(dg, &add_seq->seq[l][g], sizeof(qc_gate_internal_t));
            dg->Target += 1;
            for (uint32_t c = 0; c < dg->NumControls; c++) {
                dg->Control[c] += 1;
            }
            seq->gates_per_layer[current_layer]++;
        }
        current_layer++;
        seq->used_layer++;
    }

    qc_sequence_free(sub_seq);
    qc_sequence_free(add_seq);
    qc_sequence_compute_total_gate_count(seq);
    seq->total_qubits = bits + 3;
    return seq;
}

/* ====================================================================== */
/* Controlled CQ greater-than: result = (A > value) conditioned on ctrl    */
/* ====================================================================== */

/**
 * @brief Build a controlled CQ greater-than comparison sequence.
 *
 * A > value iff A < value+1. Delegates to qc_c_cmp_cq_less_seq.
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1]=borrow, [bits+2]=control
 *
 * @param bits   Width of quantum operand A (1-63).
 * @param value  Classical value to compare against.
 * @return Sequence (caller must free), or NULL on error.
 */
qc_sequence_t *qc_c_cmp_cq_greater_seq(int bits, int64_t value) {
    int64_t max_val = (bits == 64) ? (int64_t)UINT64_MAX
                                   : (int64_t)((1ULL << bits) - 1);
    if (value >= max_val) {
        qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
        return seq;
    }
    return qc_c_cmp_cq_less_seq(bits, value + 1);
}

/* ====================================================================== */
/* Public API wrappers                                                     */
/* ====================================================================== */

/**
 * @brief CQ equality: result qubit set to |1> if A == value.
 *
 * For width >= 3, the MCX decomposition requires (width - 2) AND-ancilla
 * qubits at abstract indices [width+1 .. width+width-2]. This wrapper
 * allocates them, maps them in qubit_array, and frees after execution.
 */
qc_error_t qc_cmp_cq_equal(circuit_ctx_t *ctx, const uint32_t *a,
                            uint32_t width, int64_t value, uint32_t result) {
    if (ctx == NULL || a == NULL) {
        return QC_ERR_NULL;
    }
    if (width < 1 || width > 64) {
        return QC_ERR_WIDTH;
    }

    qc_sequence_t *seq = qc_cmp_cq_equal_seq((int)width, value);
    if (seq == NULL) {
        return QC_ERR_ALLOC;
    }

    /* Empty sequence (out-of-range value): no-op, result stays |0> */
    if (qc_sequence_gate_count(seq) == 0) {
        qc_sequence_free(seq);
        return QC_OK;
    }

    /* Build qubit mapping: abstract[0]=result, abstract[1..width]=a[0..width-1] */
    uint32_t qubit_array[128];
    qubit_array[0] = result;
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i + 1] = a[i];
    }

    /* AND-ancilla qubits for width >= 3: MCX decomposition needs (width-2) ancilla */
    uint32_t anc_start = 0;
    if (width >= 3) {
        uint32_t n_anc = width - 2;
        qc_error_t err = qc_qubit_alloc_n(ctx, n_anc, &anc_start);
        if (err != QC_OK) {
            qc_sequence_free(seq);
            return QC_ERR_ALLOC;
        }
        for (uint32_t i = 0; i < n_anc; i++) {
            qubit_array[width + 1 + i] = anc_start + i;
        }
    }

    qc_run_instruction(ctx, seq, qubit_array, 0);
    qc_sequence_free(seq);

    if (width >= 3) {
        qc_qubit_free_n(ctx, anc_start, width - 2);
    }
    return QC_OK;
}

/**
 * @brief CQ less-than: result qubit set to |1> if A < value.
 *
 * The borrow-ancilla subtraction pattern requires one ancilla qubit at
 * abstract index [width+1]. This wrapper allocates it, maps it in
 * qubit_array, and frees after execution.
 */
qc_error_t qc_cmp_cq_less(circuit_ctx_t *ctx, const uint32_t *a,
                           uint32_t width, int64_t value, uint32_t result) {
    if (ctx == NULL || a == NULL) {
        return QC_ERR_NULL;
    }
    if (width < 1 || width > 63) {
        return QC_ERR_WIDTH;
    }

    qc_sequence_t *seq = qc_cmp_cq_less_seq((int)width, value);
    if (seq == NULL) {
        return QC_ERR_ALLOC;
    }

    /* Empty sequence (e.g. value == 0 means "a < 0" trivially false): no-op */
    if (qc_sequence_gate_count(seq) == 0) {
        qc_sequence_free(seq);
        return QC_OK;
    }

    /* Qubit mapping: [0]=result, [1..width]=a, [width+1]=borrow ancilla */
    uint32_t qubit_array[128];
    qubit_array[0] = result;
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i + 1] = a[i];
    }

    /* Allocate borrow ancilla */
    uint32_t borrow;
    qc_error_t err = qc_qubit_alloc(ctx, &borrow);
    if (err != QC_OK) {
        qc_sequence_free(seq);
        return QC_ERR_ALLOC;
    }
    qubit_array[width + 1] = borrow;

    qc_run_instruction(ctx, seq, qubit_array, 0);
    qc_sequence_free(seq);
    qc_qubit_free(ctx, borrow);
    return QC_OK;
}

/**
 * @brief CQ greater-than: result qubit set to |1> if A > value.
 *
 * Delegates to cq_less(value+1). The borrow-ancilla subtraction pattern
 * requires one ancilla qubit at abstract index [width+1]. This wrapper
 * allocates it, maps it, and frees after execution.
 */
qc_error_t qc_cmp_cq_greater(circuit_ctx_t *ctx, const uint32_t *a,
                              uint32_t width, int64_t value, uint32_t result) {
    if (ctx == NULL || a == NULL) {
        return QC_ERR_NULL;
    }
    if (width < 1 || width > 63) {
        return QC_ERR_WIDTH;
    }

    qc_sequence_t *seq = qc_cmp_cq_greater_seq((int)width, value);
    if (seq == NULL) {
        return QC_ERR_ALLOC;
    }

    /* Empty sequence (value >= max_val means "a > max_val" trivially false): no-op */
    if (qc_sequence_gate_count(seq) == 0) {
        qc_sequence_free(seq);
        return QC_OK;
    }

    uint32_t qubit_array[128];
    qubit_array[0] = result;
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i + 1] = a[i];
    }

    /* Allocate borrow ancilla */
    uint32_t borrow;
    qc_error_t err = qc_qubit_alloc(ctx, &borrow);
    if (err != QC_OK) {
        qc_sequence_free(seq);
        return QC_ERR_ALLOC;
    }
    qubit_array[width + 1] = borrow;

    qc_run_instruction(ctx, seq, qubit_array, 0);
    qc_sequence_free(seq);
    qc_qubit_free(ctx, borrow);
    return QC_OK;
}
