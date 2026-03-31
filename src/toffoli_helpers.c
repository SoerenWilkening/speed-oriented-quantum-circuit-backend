/**
 * @file toffoli_helpers.c
 * @brief Helper functions for Toffoli addition sequences.
 *
 * Module 1.9 (Phase 1) - refactor-w6f
 *
 * Provides:
 *   - qc_toffoli_seq_alloc: allocate a qc_sequence_t with given layers
 *   - qc_toffoli_seq_free: free a qc_sequence_t (including large_control)
 *
 * These replace the monolith's alloc_sequence and toffoli_sequence_free
 * functions, adapted for qc_sequence_t / qc_gate_internal_t.
 * qc_two_complement is provided separately in integer.c.
 */

#include "internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* qc_toffoli_seq_alloc                                                    */
/* ====================================================================== */

/**
 * @brief Allocate a qc_sequence_t with the given number of layers, 1 gate
 *        slot per layer initially.
 *
 * @param num_layers  Number of layers to allocate.
 * @return Allocated sequence, or NULL on failure.
 */
qc_sequence_t *qc_toffoli_seq_alloc(int num_layers) {
    if (num_layers <= 0)
        return NULL;

    qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
    if (seq == NULL)
        return NULL;

    seq->num_layer = (uint32_t)num_layers;
    seq->used_layer = 0;
    seq->total_gate_count = 0;

    seq->gates_per_layer = calloc((size_t)num_layers, sizeof(uint32_t));
    if (seq->gates_per_layer == NULL) {
        free(seq);
        return NULL;
    }

    seq->seq = calloc((size_t)num_layers, sizeof(qc_gate_internal_t *));
    if (seq->seq == NULL) {
        free(seq->gates_per_layer);
        free(seq);
        return NULL;
    }

    for (int i = 0; i < num_layers; i++) {
        seq->seq[i] = calloc(1, sizeof(qc_gate_internal_t));
        if (seq->seq[i] == NULL) {
            for (int j = 0; j < i; j++)
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
/* qc_toffoli_seq_free                                                     */
/* ====================================================================== */

/**
 * @brief Free a qc_sequence_t and all internal arrays.
 *
 * Frees large_control arrays for gates with >2 controls, then per-layer
 * gate arrays, then the sequence itself.
 *
 * @param seq  Sequence to free (NULL is safe no-op).
 */
void qc_toffoli_seq_free(qc_sequence_t *seq) {
    if (seq == NULL)
        return;

    if (seq->seq != NULL) {
        for (uint32_t i = 0; i < seq->num_layer; i++) {
            if (seq->seq[i] != NULL) {
                if (seq->gates_per_layer != NULL) {
                    for (uint32_t g = 0; g < seq->gates_per_layer[i]; g++) {
                        if (seq->seq[i][g].NumControls > QC_MAX_INLINE_CONTROLS &&
                            seq->seq[i][g].large_control != NULL) {
                            free(seq->seq[i][g].large_control);
                        }
                    }
                }
                free(seq->seq[i]);
            }
        }
        free(seq->seq);
    }

    free(seq->gates_per_layer);
    free(seq);
}

/* qc_two_complement is defined in integer.c and declared in internal.h */
