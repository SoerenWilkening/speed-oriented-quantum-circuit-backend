/**
 * @file capture_helpers.h
 * @brief Shared static-inline capture helpers for sequence builders.
 *
 * Provides create_capture_ctx() and capture_circuit_to_sequence() as
 * static inline functions.  Included by integer_comparison.c and
 * integer_comparison_ctrl.c so the ~90-line pattern is not duplicated.
 *
 * Pattern copied from divmod_sequences.c (refactor-a1b).
 */

#ifndef QC_CAPTURE_HELPERS_H
#define QC_CAPTURE_HELPERS_H

#include "internal.h"
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Helper: create temp circuit configured for gate capture                 */
/* ====================================================================== */

static inline circuit_ctx_t *cmp_create_capture_ctx(uint32_t initial_qubits) {
    circuit_ctx_t *ctx = qc_circuit_create(initial_qubits);
    if (!ctx) return NULL;
    qc_circuit_set_simulate(ctx, true);
    return ctx;
}

/* ====================================================================== */
/* Helper: capture gates from a temp circuit into a qc_sequence_t          */
/* ====================================================================== */

/**
 * @brief Copy internal gates from a circuit context into a new qc_sequence_t.
 *
 * Iterates over all used layers in ctx, copying each gate (including
 * large_control arrays) into a freshly allocated sequence.
 *
 * @param ctx  Source circuit (consumed, not destroyed here).
 * @return     New sequence, or NULL on allocation failure.
 */
static inline qc_sequence_t *cmp_capture_circuit_to_sequence(circuit_ctx_t *ctx) {
    if (!ctx || ctx->used_layer == 0)
        return NULL;

    uint32_t num_layers = ctx->used_layer;

    qc_sequence_t *seq = malloc(sizeof(qc_sequence_t));
    if (!seq) return NULL;

    seq->num_layer = num_layers;
    seq->used_layer = num_layers;
    seq->total_gate_count = 0;
    seq->total_qubits = 0;

    seq->gates_per_layer = calloc(num_layers, sizeof(uint32_t));
    if (!seq->gates_per_layer) {
        free(seq);
        return NULL;
    }

    seq->seq = calloc(num_layers, sizeof(qc_gate_internal_t *));
    if (!seq->seq) {
        free(seq->gates_per_layer);
        free(seq);
        return NULL;
    }

    for (uint32_t layer = 0; layer < num_layers; layer++) {
        uint32_t gate_count = ctx->used_gates_per_layer[layer];
        seq->gates_per_layer[layer] = gate_count;

        if (gate_count == 0) {
            seq->seq[layer] = calloc(1, sizeof(qc_gate_internal_t));
            if (!seq->seq[layer]) goto fail;
            continue;
        }

        seq->seq[layer] = calloc(gate_count, sizeof(qc_gate_internal_t));
        if (!seq->seq[layer]) goto fail;

        for (uint32_t gi = 0; gi < gate_count; gi++) {
            const qc_gate_internal_t *src = &ctx->sequence[layer][gi];
            qc_gate_internal_t *dst = &seq->seq[layer][gi];

            dst->Gate = src->Gate;
            dst->GateValue = src->GateValue;
            dst->Target = src->Target;
            dst->NumControls = src->NumControls;
            dst->NumBasisGates = src->NumBasisGates;
            dst->large_control = NULL;

            if (src->NumControls <= QC_MAX_INLINE_CONTROLS) {
                for (uint32_t ci = 0; ci < src->NumControls; ci++)
                    dst->Control[ci] = src->Control[ci];
            } else if (src->large_control) {
                dst->large_control = malloc(
                    src->NumControls * sizeof(uint32_t));
                if (!dst->large_control) goto fail;
                memcpy(dst->large_control, src->large_control,
                       src->NumControls * sizeof(uint32_t));
            }
        }
    }

    qc_sequence_compute_total_gate_count(seq);
    return seq;

fail:
    if (seq->seq) {
        for (uint32_t i = 0; i < num_layers; i++) {
            if (seq->seq[i]) {
                for (uint32_t g = 0; g < seq->gates_per_layer[i]; g++) {
                    if (seq->seq[i][g].large_control)
                        free(seq->seq[i][g].large_control);
                }
                free(seq->seq[i]);
            }
        }
        free(seq->seq);
    }
    free(seq->gates_per_layer);
    free(seq);
    return NULL;
}

#endif /* QC_CAPTURE_HELPERS_H */
