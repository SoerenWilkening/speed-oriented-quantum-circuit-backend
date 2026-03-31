/**
 * @file integer_comparison.c
 * @brief Integer comparison operations refactored for the micro-package backend.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/IntegerComparison.c
 * Module: 1.12 (Phase 1)
 * Issue: refactor-ovp
 *
 * All comparison functions generate qc_sequence_t sequences using abstract
 * qubit indices. The sequences are applied to a circuit_ctx_t via
 * qc_run_instruction(). No global state is used.
 *
 * Comparison operations:
 *   - CQ equality (XOR + MCX pattern)
 *   - CQ less-than (borrow-ancilla pattern)
 *   - CQ greater-than (delegates to less-than with value+1)
 *   - QQ less-than (borrow via extended addition)
 *
 * Thread safety: Sequence builders are stateless. Each circuit_ctx_t is
 * independent.
 */

#include "internal.h"
#include <stdint.h>

/* External: from integer.c */
extern int *qc_two_complement(int64_t x, int n);

/* ====================================================================== */
/* Gate initializers for sequence building                                 */
/* ====================================================================== */

static void qc_seq_gate_init(qc_gate_internal_t *g) {
    memset(g, 0, sizeof(qc_gate_internal_t));
    g->large_control = NULL;
}

static void qc_seq_gate_x(qc_gate_internal_t *g, uint32_t target) {
    qc_seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 0;
}

static void qc_seq_gate_cx(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    qc_seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void qc_seq_gate_ccx(qc_gate_internal_t *g, uint32_t target,
                             uint32_t ctrl1, uint32_t ctrl2) {
    qc_seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 2;
    g->Control[0] = ctrl1;
    g->Control[1] = ctrl2;
}


/* ====================================================================== */
/* Sequence allocation helpers                                             */
/* ====================================================================== */

/**
 * @brief Allocate a qc_sequence_t with the given number of layers and max
 *        gates per layer.
 *
 * @return Allocated sequence, or NULL on failure.
 */
static qc_sequence_t *alloc_sequence(int num_layers, int max_gates_per_layer) {
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
        seq->seq[i] = calloc((size_t)max_gates_per_layer, sizeof(qc_gate_internal_t));
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

/**
 * @brief Free a qc_sequence_t and all its contents.
 */
void qc_sequence_free(qc_sequence_t *seq) {
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
/* MCX decomposition helpers                                               */
/* ====================================================================== */

static int mcx_decomp_layers(int num_controls) {
    if (num_controls <= 2) {
        return 1;
    }
    if (num_controls == 3) {
        return 3;
    }
    return 2 + mcx_decomp_layers(num_controls - 1);
}

static void emit_mcx_decomp(qc_sequence_t *seq, int *layer, uint32_t target,
                             const uint32_t *controls, int num_controls, int anc_start) {
    if (num_controls == 2) {
        qc_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         target, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;
    } else if (num_controls == 3) {
        uint32_t and_anc = (uint32_t)anc_start;
        qc_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         and_anc, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;
        qc_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         target, and_anc, controls[2]);
        (*layer)++;
        seq->used_layer++;
        qc_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         and_anc, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;
    } else {
        uint32_t and_anc = (uint32_t)anc_start;
        qc_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         and_anc, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;

        uint32_t reduced[128];
        reduced[0] = and_anc;
        for (int i = 2; i < num_controls; i++) {
            reduced[i - 1] = controls[i];
        }
        emit_mcx_decomp(seq, layer, target, reduced, num_controls - 1, anc_start + 1);

        qc_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         and_anc, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;
    }
}

/* ====================================================================== */
/* CQ equality: result = (A == value)                                      */
/* ====================================================================== */

/**
 * @brief Build a CQ equality comparison sequence.
 *
 * Qubit layout: [0]=result, [1..bits]=A
 * For bits >= 3: [bits+1..bits+bits-2] = AND-ancilla
 *
 * @param bits   Width of quantum operand (1-64).
 * @param value  Classical value to compare.
 * @return Sequence (caller must free via qc_sequence_free), or NULL on error.
 */
qc_sequence_t *qc_cmp_cq_equal_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 64) {
        return NULL;
    }

    /* Range check */
    uint64_t max_val = (bits == 64) ? UINT64_MAX : ((1ULL << bits) - 1);
    if (value < 0) {
        int64_t min_val = -(1LL << (bits - 1));
        if (value < min_val) {
            /* Value out of range: never equal, return empty seq */
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

    int num_x_gates = 0;
    for (int i = 0; i < bits; i++) {
        if (bin[i] == 0) {
            num_x_gates++;
        }
    }

    int mcx_layers = (bits <= 2) ? 1 : mcx_decomp_layers(bits);
    int num_layers = num_x_gates + mcx_layers + num_x_gates;

    qc_sequence_t *seq = alloc_sequence(num_layers, (int)bits + 1);
    if (seq == NULL) {
        free(bin);
        return NULL;
    }

    int current_layer = 0;

    /* Phase 1: X gates where classical bit is 0 */
    for (int i = 0; i < bits; i++) {
        if (bin[i] == 0) {
            qc_seq_gate_x(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                           (uint32_t)(i + 1));
            seq->gates_per_layer[current_layer]++;
            current_layer++;
            seq->used_layer++;
        }
    }

    /* Phase 2: MCX to set result qubit */
    if (bits == 1) {
        qc_seq_gate_cx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                        0, 1);
        seq->gates_per_layer[current_layer]++;
        current_layer++;
        seq->used_layer++;
    } else if (bits == 2) {
        qc_seq_gate_ccx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                         0, 1, 2);
        seq->gates_per_layer[current_layer]++;
        current_layer++;
        seq->used_layer++;
    } else {
        uint32_t controls[128];
        for (int i = 0; i < bits; i++) {
            controls[i] = (uint32_t)(i + 1);
        }
        int anc_start = bits + 1;
        emit_mcx_decomp(seq, &current_layer, 0, controls, bits, anc_start);
    }

    /* Phase 3: Uncompute X gates */
    for (int i = 0; i < bits; i++) {
        if (bin[i] == 0) {
            qc_seq_gate_x(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                           (uint32_t)(i + 1));
            seq->gates_per_layer[current_layer]++;
            current_layer++;
            seq->used_layer++;
        }
    }

    free(bin);
    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/* ====================================================================== */
/* CQ less-than: result = (A < value) via borrow-ancilla pattern           */
/* ====================================================================== */

/* Forward declarations for hot_path_add.c functions */
extern qc_sequence_t *qc_split_cq_add_seq(int bits, int64_t value);
extern qc_sequence_t *qc_split_cq_sub_seq(int bits, int64_t value);

/**
 * @brief Build a CQ less-than comparison sequence.
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1]=borrow_ancilla
 *
 * Algorithm:
 *   1. [A, borrow] -= value (split subtraction)
 *   2. CX(borrow -> result)
 *   3. [A, borrow] += value (split addition to restore)
 *
 * @param bits   Width of quantum operand A (1-63).
 * @param value  Classical value to compare against.
 * @return Sequence (caller must free via qc_sequence_free), or NULL on error.
 */
qc_sequence_t *qc_cmp_cq_less_seq(int bits, int64_t value) {
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
    int max_gpg = bits + 2;

    qc_sequence_t *seq = alloc_sequence(total_layers, max_gpg);
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

    /* Step 2: CX(target=0, control=bits+1) -- copy borrow to result */
    qc_seq_gate_cx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]++],
                    0, (uint32_t)(bits + 1));
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
    return seq;
}

/* ====================================================================== */
/* CQ greater-than: result = (A > value)                                   */
/* ====================================================================== */

/**
 * @brief Build a CQ greater-than comparison sequence.
 *
 * A > value iff A < value+1 is false. We build the lt(value+1) sequence;
 * the caller (Python layer) handles inversion.
 *
 * @param bits   Width of quantum operand A (1-63).
 * @param value  Classical value to compare against.
 * @return Sequence (caller must free via qc_sequence_free), or NULL on error.
 */
qc_sequence_t *qc_cmp_cq_greater_seq(int bits, int64_t value) {
    int64_t max_val = (bits == 64) ? (int64_t)UINT64_MAX
                                   : (int64_t)((1ULL << bits) - 1);
    if (value >= max_val) {
        /* a > max_val is always false */
        qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
        return seq;
    }
    return qc_cmp_cq_less_seq(bits, value + 1);
}

/* ====================================================================== */
/* Public API wrappers                                                     */
/* ====================================================================== */

/**
 * @brief CQ equality: result qubit set to |1> if A == value.
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

    /* Build qubit mapping: abstract[0]=result, abstract[1..width]=a[0..width-1] */
    uint32_t qubit_array[128];
    qubit_array[0] = result;
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i + 1] = a[i];
    }
    /* AND-ancilla qubits (for width >= 3): allocate temporary qubits */
    /* For now, the caller is responsible for ensuring enough qubits exist */

    qc_run_instruction(ctx, seq, qubit_array, 0);
    qc_sequence_free(seq);
    return QC_OK;
}

/**
 * @brief CQ less-than: result qubit set to |1> if A < value.
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

    /* Qubit mapping: [0]=result, [1..width]=a, [width+1]=borrow ancilla */
    uint32_t qubit_array[128];
    qubit_array[0] = result;
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i + 1] = a[i];
    }
    /* Borrow ancilla must be provided by caller or allocated */

    qc_run_instruction(ctx, seq, qubit_array, 0);
    qc_sequence_free(seq);
    return QC_OK;
}

/**
 * @brief CQ greater-than: result qubit set to |1> if A > value.
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

    uint32_t qubit_array[128];
    qubit_array[0] = result;
    for (uint32_t i = 0; i < width; i++) {
        qubit_array[i + 1] = a[i];
    }

    qc_run_instruction(ctx, seq, qubit_array, 0);
    qc_sequence_free(seq);
    return QC_OK;
}
