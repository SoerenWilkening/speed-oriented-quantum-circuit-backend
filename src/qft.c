/**
 * @file qft.c
 * @brief Standalone bare QFT / inverse-QFT sequence builders.
 *
 * Issue: refactor-2ed
 *
 * Provides public sequence builders qc_qft_seq / qc_iqft_seq that emit a
 * textbook QFT (no final swaps, MSB-first processing) on virtual qubits
 * [0 .. n-1]. The mapping to hardware qubits is performed at application
 * time by qc_run_instruction's qmap argument.
 *
 * IMPORTANT: The rotation pattern below is intentionally duplicated from
 * the static helpers qc_seq_qft / qc_seq_qft_inv in src/qft_addition.c.
 * This duplication is load-bearing: the QFT-arithmetic builders in
 * qft_addition.c must remain bit-for-bit stable, so we do NOT route the
 * static helpers through the new public builders or vice versa. Do not
 * refactor to share code.
 */

#include "internal.h"

#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====================================================================== */
/* Local sequence allocator (two-arg, pre-sizes per-layer gate slots).    */
/* Analogous to the static qft_sequence_alloc in qft_addition.c.          */
/* The public qc_sequence_alloc is single-arg and does NOT allocate       */
/* per-layer gate slots, so we cannot use it here.                        */
/* ====================================================================== */

static qc_sequence_t *qft_local_sequence_alloc(uint32_t num_layers,
                                               uint32_t gates_cap) {
    qc_sequence_t *seq = malloc(sizeof(qc_sequence_t));
    if (seq == NULL)
        return NULL;

    seq->used_layer = 0;
    seq->num_layer = num_layers;
    seq->total_gate_count = 0;
    seq->total_qubits = 0;
    seq->gates_per_layer = NULL;
    seq->seq = NULL;

    if (num_layers == 0) {
        return seq;
    }

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
/* Local gate slot writers (duplicated from qft_addition.c on purpose).    */
/* ====================================================================== */

static void qft_local_h(qc_gate_internal_t *g, uint32_t target) {
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_H;
    g->Target = target;
    g->NumControls = 0;
}

static void qft_local_cp(qc_gate_internal_t *g, uint32_t target,
                         uint32_t control, double value) {
    memset(g, 0, sizeof(*g));
    g->Gate = QC_IGATE_P;
    g->GateValue = value;
    g->Target = target;
    g->NumControls = 1;
    g->Control[0] = control;
}

/* ====================================================================== */
/* Public bare-QFT sequence builders                                       */
/* ====================================================================== */

qc_sequence_t *qc_qft_seq(int n) {
    if (n <= 0) {
        qc_sequence_t *seq = qft_local_sequence_alloc(0, 0);
        if (seq == NULL)
            return NULL;
        seq->total_qubits = 0;
        seq->total_gate_count = 0;
        return seq;
    }

    if (n > 64)
        return NULL;

    uint32_t num_layers = (uint32_t)(2 * n - 1);
    /* Each layer holds at most one gate in a bare QFT, but allocate n
     * slots per layer for safety / symmetry with qft_addition.c. */
    uint32_t gates_cap = (uint32_t)n;
    if (gates_cap == 0)
        gates_cap = 1;

    qc_sequence_t *seq = qft_local_sequence_alloc(num_layers, gates_cap);
    if (seq == NULL)
        return NULL;

    /* Forward QFT — replicates qc_seq_qft from qft_addition.c lines 145–159,
     * with seq->used_layer == 0 (fresh sequence). */
    for (int j = 0; j < n; ++j) {
        int q = n - 1 - j; /* MSB-first processing */
        uint32_t layer = (uint32_t)(2 * j);
        qft_local_h(&seq->seq[layer][seq->gates_per_layer[layer]++],
                    (uint32_t)q);

        for (int i = 0; i < n - 1 - j; ++i) {
            uint32_t cp_layer = (uint32_t)(2 * j + i + 1);
            qft_local_cp(&seq->seq[cp_layer][seq->gates_per_layer[cp_layer]++],
                         (uint32_t)q, (uint32_t)(q - i - 1),
                         M_PI / (double)(1u << (i + 1)));
        }
    }
    seq->used_layer = (uint32_t)(2 * n - 1);
    seq->total_qubits = (uint32_t)n;
    qc_sequence_compute_total_gate_count(seq);
    return seq;
}

qc_sequence_t *qc_iqft_seq(int n) {
    if (n <= 0) {
        qc_sequence_t *seq = qft_local_sequence_alloc(0, 0);
        if (seq == NULL)
            return NULL;
        seq->total_qubits = 0;
        seq->total_gate_count = 0;
        return seq;
    }

    if (n > 64)
        return NULL;

    uint32_t num_layers = (uint32_t)(2 * n - 1);
    uint32_t gates_cap = (uint32_t)n;
    if (gates_cap == 0)
        gates_cap = 1;

    qc_sequence_t *seq = qft_local_sequence_alloc(num_layers, gates_cap);
    if (seq == NULL)
        return NULL;

    /* Inverse QFT — copied verbatim from qc_seq_qft_inv in
     * qft_addition.c lines 166–179, with seq->used_layer == 0
     * (fresh sequence). The layer index expression
     * "2*n - 2 - (2*j + i + 1)" is preserved exactly. */
    for (int j = 0; j < n; ++j) {
        int q = n - 1 - j;
        for (int i = 0; i < n - 1 - j; ++i) {
            uint32_t layer = (uint32_t)(2 * n - 2 - (2 * j + i + 1));
            qft_local_cp(&seq->seq[layer][seq->gates_per_layer[layer]++],
                         (uint32_t)q, (uint32_t)(q - i - 1),
                         -M_PI / (double)(1u << (i + 1)));
        }
        uint32_t h_layer = (uint32_t)(2 * n - 2 - 2 * j);
        qft_local_h(&seq->seq[h_layer][seq->gates_per_layer[h_layer]++],
                    (uint32_t)q);
    }
    seq->used_layer = (uint32_t)(2 * n - 1);
    seq->total_qubits = (uint32_t)n;
    qc_sequence_compute_total_gate_count(seq);
    return seq;
}
