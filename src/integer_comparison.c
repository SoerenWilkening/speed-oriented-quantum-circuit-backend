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
 * Controlled variants and public API wrappers live in
 * integer_comparison_ctrl.c (refactor-4xn).
 *
 * Thread safety: Sequence builders are stateless. Each circuit_ctx_t is
 * independent.
 */

#include "comparison_internal.h"
#include "capture_helpers.h"

/* ====================================================================== */
/* Gate initializers for sequence building                                 */
/* ====================================================================== */

void qc_cmp_seq_gate_init(qc_gate_internal_t *g) {
    memset(g, 0, sizeof(qc_gate_internal_t));
    g->large_control = NULL;
}

void qc_cmp_seq_gate_x(qc_gate_internal_t *g, uint32_t target) {
    qc_cmp_seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 0;
}

void qc_cmp_seq_gate_cx(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    qc_cmp_seq_gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 1;
    g->Control[0] = control;
}

void qc_cmp_seq_gate_ccx(qc_gate_internal_t *g, uint32_t target,
                          uint32_t ctrl1, uint32_t ctrl2) {
    qc_cmp_seq_gate_init(g);
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
qc_sequence_t *qc_cmp_alloc_sequence(int num_layers, int max_gates_per_layer) {
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

/* qc_sequence_free is now provided by toffoli_helpers.c (public API) */

/* ====================================================================== */
/* MCX decomposition helpers                                               */
/* ====================================================================== */

int qc_cmp_mcx_decomp_layers(int num_controls) {
    if (num_controls <= 2) {
        return 1;
    }
    if (num_controls == 3) {
        return 3;
    }
    return 2 + qc_cmp_mcx_decomp_layers(num_controls - 1);
}

void qc_cmp_emit_mcx_decomp(qc_sequence_t *seq, int *layer, uint32_t target,
                             const uint32_t *controls, int num_controls, int anc_start) {
    if (num_controls == 2) {
        qc_cmp_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         target, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;
    } else if (num_controls == 3) {
        uint32_t and_anc = (uint32_t)anc_start;
        qc_cmp_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         and_anc, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;
        qc_cmp_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         target, and_anc, controls[2]);
        (*layer)++;
        seq->used_layer++;
        qc_cmp_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         and_anc, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;
    } else {
        uint32_t and_anc = (uint32_t)anc_start;
        qc_cmp_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
                         and_anc, controls[0], controls[1]);
        (*layer)++;
        seq->used_layer++;

        uint32_t reduced[128];
        reduced[0] = and_anc;
        for (int i = 2; i < num_controls; i++) {
            reduced[i - 1] = controls[i];
        }
        qc_cmp_emit_mcx_decomp(seq, layer, target, reduced, num_controls - 1, anc_start + 1);

        qc_cmp_seq_gate_ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++],
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

    int mcx_layers = (bits <= 2) ? 1 : qc_cmp_mcx_decomp_layers(bits);
    int num_layers = num_x_gates + mcx_layers + num_x_gates;

    qc_sequence_t *seq = qc_cmp_alloc_sequence(num_layers, (int)bits + 1);
    if (seq == NULL) {
        free(bin);
        return NULL;
    }

    int current_layer = 0;

    /* Phase 1: X gates where classical bit is 0 */
    for (int i = 0; i < bits; i++) {
        if (bin[i] == 0) {
            qc_cmp_seq_gate_x(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                           (uint32_t)(i + 1));
            seq->gates_per_layer[current_layer]++;
            current_layer++;
            seq->used_layer++;
        }
    }

    /* Phase 2: MCX to set result qubit */
    if (bits == 1) {
        qc_cmp_seq_gate_cx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                        0, 1);
        seq->gates_per_layer[current_layer]++;
        current_layer++;
        seq->used_layer++;
    } else if (bits == 2) {
        qc_cmp_seq_gate_ccx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
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
        qc_cmp_emit_mcx_decomp(seq, &current_layer, 0, controls, bits, anc_start);
    }

    /* Phase 3: Uncompute X gates */
    for (int i = 0; i < bits; i++) {
        if (bin[i] == 0) {
            qc_cmp_seq_gate_x(&seq->seq[current_layer][seq->gates_per_layer[current_layer]],
                           (uint32_t)(i + 1));
            seq->gates_per_layer[current_layer]++;
            current_layer++;
            seq->used_layer++;
        }
    }

    free(bin);
    qc_sequence_compute_total_gate_count(seq);
    seq->total_qubits = bits < 3 ? bits + 1 : 2 * bits - 1;
    return seq;
}

/* ====================================================================== */
/* CQ less-than: result = (A < value) via borrow-ancilla pattern           */
/* ====================================================================== */

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

    /* Step 2: CX(target=0, control=bits+1) -- copy borrow to result */
    qc_cmp_seq_gate_cx(&seq->seq[current_layer][seq->gates_per_layer[current_layer]++],
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
    seq->total_qubits = bits + 2;
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
/* QQ less-than: result = (A < B) via borrow-ancilla                       */
/* ====================================================================== */

/**
 * @brief Build a QQ less-than comparison sequence.
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1..2*bits]=B,
 *               [2*bits+1]=borrow, [2*bits+2]=zero_ext
 *
 * @param bits  Width of operands (1-63).
 * @return Sequence (caller must free), or NULL on error.
 */
qc_sequence_t *qc_cmp_qq_less_seq(int bits) {
    if (bits <= 0 || bits > 63)
        return NULL;

    uint32_t n = (uint32_t)bits;
    uint32_t total_reg = 2 * n + 3;

    /* Build sub-sequences: QQ add on n bits and n+1 bits */
    qc_sequence_t *add_n_seq = qc_arith_qq_add_seq(bits);
    qc_sequence_t *add_ext_seq = qc_arith_qq_add_seq(bits + 1);
    if (!add_n_seq || !add_ext_seq) {
        qc_sequence_free(add_n_seq);
        qc_sequence_free(add_ext_seq);
        return NULL;
    }

    /* Qubit mapping for QQ_add(n): [0..n-1]=target, [n..2n-1]=source */
    uint32_t map_n[128];
    for (uint32_t i = 0; i < n; i++) {
        map_n[i] = i + 1;           /* target -> A */
        map_n[n + i] = n + 1 + i;   /* source -> B */
    }

    /* Qubit mapping for QQ_add(n+1): [0..n]=target, [n+1..2n+1]=source */
    uint32_t map_ext[128];
    for (uint32_t i = 0; i < n; i++) {
        map_ext[i] = i + 1;             /* target LSBs -> A */
        map_ext[n + 1 + i] = n + 1 + i; /* source LSBs -> B */
    }
    map_ext[n] = 2 * n + 1;             /* target MSB -> borrow */
    map_ext[2 * n + 1] = 2 * n + 2;     /* source MSB -> zero_ext */

    /* Create capture circuit */
    circuit_ctx_t *ctx = cmp_create_capture_ctx(total_reg + 64);
    if (!ctx) {
        qc_sequence_free(add_n_seq);
        qc_sequence_free(add_ext_seq);
        return NULL;
    }
    qc_qubit_alloc_n(ctx, total_reg, &(uint32_t){0});

    /* Step 1: A -= B (inverse QQ_add on n bits) */
    qc_run_instruction(ctx, add_n_seq, map_n, 1);

    /* Step 2: [A,borrow] += [B,zero_ext] (forward QQ_add on n+1 bits) */
    qc_run_instruction(ctx, add_ext_seq, map_ext, 0);

    /* Step 3: CX(control=borrow, target=result) */
    qc_circuit_cx(ctx, 2 * n + 1, 0);

    /* Step 4: Undo step 2 (inverse extended add) */
    qc_run_instruction(ctx, add_ext_seq, map_ext, 1);

    /* Step 5: A += B (forward QQ_add on n bits, restore) */
    qc_run_instruction(ctx, add_n_seq, map_n, 0);

    /* Capture and cleanup */
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);
    qc_sequence_free(add_n_seq);
    qc_sequence_free(add_ext_seq);

    if (seq) {
        seq->total_qubits = total_reg;
    }
    return seq;
}
