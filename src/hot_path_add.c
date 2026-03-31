/**
 * @file hot_path_add.c
 * @brief Split-register arithmetic for comparison operations.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/hot_path_add.c
 * Module: 1.12 (Phase 1)
 * Issue: refactor-ovp
 *
 * The split-register pattern treats [a_0..a_{n-1}, msb_qubit] as an
 * (n+1)-bit register. This is the building block for comparison operators.
 *
 * Provides QFT-mode split-register addition/subtraction sequences.
 * All functions return qc_sequence_t sequences using abstract qubit indices.
 *
 * Thread safety: Sequence builders are stateless and thread-safe.
 */

#include "internal.h"

#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* External: from integer.c */
extern int *qc_two_complement(int64_t x, int n);

/* ====================================================================== */
/* Gate initializers for sequence building (local)                         */
/* ====================================================================== */

static void hp_gate_init(qc_gate_internal_t *g) {
    memset(g, 0, sizeof(qc_gate_internal_t));
    g->large_control = NULL;
}

static void hp_gate_h(qc_gate_internal_t *g, uint32_t target) {
    hp_gate_init(g);
    g->Gate = QC_IGATE_H;
    g->Target = target;
    g->NumControls = 0;
}

static void hp_gate_p(qc_gate_internal_t *g, uint32_t target, double angle) {
    hp_gate_init(g);
    g->Gate = QC_IGATE_P;
    g->Target = target;
    g->GateValue = angle;
    g->NumControls = 0;
}

static void hp_gate_cp(qc_gate_internal_t *g, uint32_t target, uint32_t control,
                        double angle) {
    hp_gate_init(g);
    g->Gate = QC_IGATE_P;
    g->Target = target;
    g->GateValue = angle;
    g->NumControls = 1;
    g->Control[0] = control;
}

/* ====================================================================== */
/* Sequence allocation helper                                              */
/* ====================================================================== */

static qc_sequence_t *hp_alloc_seq(int num_layers, int max_gpg) {
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

/* ====================================================================== */
/* QFT and IQFT helpers                                                    */
/* ====================================================================== */

/**
 * @brief Append QFT on w qubits to the sequence.
 *
 * Matches the monolith QFT layout: 2*w-1 layers with diamond-shaped packing.
 * Processes qubits MSB (w-1) to LSB (0). Loop j=0..w-1 maps to qubit q=w-1-j.
 * Layer 2*j gets the H gate; layers 2*j+1, 2*j+2, ... get controlled-P gates.
 *
 * Total layers: 2*w - 1.
 */
static void hp_qft(qc_sequence_t *seq, int w) {
    uint32_t base = seq->used_layer;
    for (int j = 0; j < w; ++j) {
        int q = w - 1 - j;
        uint32_t layer = base + (uint32_t)(2 * j);
        hp_gate_h(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)q);
        for (int i = 0; i < w - 1 - j; ++i) {
            layer = base + (uint32_t)(2 * j + i + 1);
            hp_gate_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                       (uint32_t)q, (uint32_t)(q - i - 1),
                       M_PI / pow(2.0, (double)(i + 1)));
        }
    }
    seq->used_layer += (uint32_t)(2 * w - 1);
}

/**
 * @brief Append inverse QFT on w qubits to the sequence.
 *
 * Matches the monolith QFT_inverse layout: 2*w-1 layers.
 * Reverse gate order of QFT. Processes LSB to MSB.
 *
 * Total layers: 2*w - 1.
 */
static void hp_iqft(qc_sequence_t *seq, int w) {
    uint32_t base = seq->used_layer;
    for (int j = 0; j < w; ++j) {
        int q = w - 1 - j;
        /* Inverse controlled-P gates */
        for (int i = 0; i < w - 1 - j; ++i) {
            uint32_t layer = base + (uint32_t)(2 * w - 1 - (2 * j + i + 1) - 1);
            hp_gate_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                       (uint32_t)q, (uint32_t)(q - i - 1),
                       -M_PI / pow(2.0, (double)(i + 1)));
        }
        /* H gate */
        uint32_t layer = base + (uint32_t)(2 * w - 1 - 2 * j - 1);
        hp_gate_h(&seq->seq[layer][seq->gates_per_layer[layer]++], (uint32_t)q);
    }
    seq->used_layer += (uint32_t)(2 * w - 1);
}

/* ====================================================================== */
/* Split-register QFT addition                                             */
/* ====================================================================== */

/**
 * @brief Split-register QFT addition: [a, msb] += classical value.
 *
 * Treats [a_0..a_{bits-1}, msb_qubit] as an (bits+1)-bit register and adds
 * a classical value using the Draper QFT adder.
 *
 * Qubit layout: [0..bits-1] = register a, [bits] = msb qubit.
 *
 * @param bits   Width of base register a (1-63).
 * @param value  Classical value to add.
 * @return Fresh sequence, or NULL on error. Caller must free.
 */
qc_sequence_t *qc_split_cq_add_seq(int bits, int64_t value) {
    int w = bits + 1;
    if (w < 2 || w > 64) {
        return NULL;
    }

    int *bin = qc_two_complement(value, w);
    if (bin == NULL) {
        return NULL;
    }

    double *rotations = calloc((size_t)w, sizeof(double));
    if (rotations == NULL) {
        free(bin);
        return NULL;
    }
    for (int bit_idx = 0; bit_idx < w; ++bit_idx) {
        for (int qubit = bit_idx; qubit < w; ++qubit) {
            rotations[qubit] += bin[w - 1 - bit_idx] * 2.0 * M_PI /
                                pow(2.0, (double)(qubit - bit_idx + 1));
        }
    }
    free(bin);

    /* Total layers: QFT(2w-1) + rotations(w) + IQFT(2w-1) = 5w-2 */
    int num_layers = 5 * w - 2;

    qc_sequence_t *seq = hp_alloc_seq(num_layers, 2 * w);
    if (seq == NULL) {
        free(rotations);
        return NULL;
    }

    /* QFT */
    hp_qft(seq, w);

    /* Phase rotations */
    int start_layer = 2 * w - 1;
    for (int i = 0; i < w; ++i) {
        hp_gate_p(&seq->seq[start_layer + i][seq->gates_per_layer[start_layer + i]++],
                  (uint32_t)i, rotations[i]);
    }
    free(rotations);
    seq->used_layer += (uint32_t)w;

    /* Inverse QFT */
    hp_iqft(seq, w);

    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

/**
 * @brief Split-register QFT subtraction: [a, msb] -= classical value.
 *
 * Equivalent to qc_split_cq_add_seq(bits, -value).
 */
qc_sequence_t *qc_split_cq_sub_seq(int bits, int64_t value) {
    return qc_split_cq_add_seq(bits, -value);
}
