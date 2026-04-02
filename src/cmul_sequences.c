/**
 * @file cmul_sequences.c
 * @brief Capture-based controlled multiplication sequence builders.
 *
 * Issue: refactor-s0y
 *
 * Controlled multiplication operations use AND-ancillae and dynamic
 * controlled additions internally, making traditional pre-built sequence
 * construction impractical.  We use the same capture approach as
 * divmod_sequences.c:
 *
 *   1. Create a temporary circuit_ctx_t with virtual qubit indices.
 *   2. Run the controlled multiplication (which emits gates + ancillae).
 *   3. Copy the resulting internal gates into a qc_sequence_t.
 *   4. Destroy the temporary circuit.
 *
 * Qubit layouts:
 *   c_arith_cq_mul:  [0..n-1] result, [n..2n-1] target, [2n] ext_ctrl
 *   c_arith_qq_mul:  [0..n-1] result, [n..2n-1] a, [2n..3n-1] b, [3n] ext_ctrl
 *
 * Functions:
 *   - qc_c_arith_cq_mul_seq  -- controlled CQ multiplication
 *   - qc_c_arith_qq_mul_seq  -- controlled QQ multiplication
 */

#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Helper: capture gates from a temp circuit into a qc_sequence_t          */
/* ====================================================================== */

/* Reuse the same capture pattern as divmod_sequences.c */
static qc_sequence_t *cmul_capture_circuit_to_sequence(circuit_ctx_t *ctx) {
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

/* ====================================================================== */
/* Helper: create temp circuit for gate capture                            */
/* ====================================================================== */

static circuit_ctx_t *cmul_create_capture_ctx(uint32_t initial_qubits) {
    circuit_ctx_t *ctx = qc_circuit_create(initial_qubits);
    if (!ctx) return NULL;
    qc_circuit_set_simulate(ctx, true);
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);
    return ctx;
}

/* ====================================================================== */
/* qc_c_arith_cq_mul_seq -- controlled CQ multiplication sequence          */
/* ====================================================================== */

/**
 * @brief Build a controlled CQ multiplication sequence.
 *
 * Qubit layout: [0..n-1] result, [n..2n-1] target, [2n] ext_ctrl.
 * Ancillae may be mapped to higher virtual indices.
 *
 * @param bits   Width (1-64).
 * @param value  Classical multiplier.
 * @return Sequence, or NULL on error. Caller must free.
 */
QC_API qc_sequence_t *qc_c_arith_cq_mul_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 64) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] result, [n..2n-1] target, [2n] ext_ctrl */
    uint32_t reg_qubits = 2 * n + 1;
    uint32_t headroom = n * 4 + 128;
    circuit_ctx_t *ctx = cmul_create_capture_ctx(reg_qubits + headroom);
    if (!ctx) return NULL;

    uint32_t start;
    if (qc_qubit_alloc_n(ctx, reg_qubits, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t result[64], target[64];
    for (uint32_t i = 0; i < n; i++) {
        result[i] = i;
        target[i] = n + i;
    }
    uint32_t ext_ctrl = 2 * n;

    qc_error_t err = qc_toffoli_cmul_cq(ctx, result, n,
                                          target, n, value, ext_ctrl);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = cmul_capture_circuit_to_sequence(ctx);
    if (seq) seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}

/* ====================================================================== */
/* qc_c_arith_qq_mul_seq -- controlled QQ multiplication sequence          */
/* ====================================================================== */

/**
 * @brief Build a controlled QQ multiplication sequence.
 *
 * Qubit layout: [0..n-1] result, [n..2n-1] a, [2n..3n-1] b, [3n] ext_ctrl.
 * Ancillae may be mapped to higher virtual indices.
 *
 * @param bits   Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
QC_API qc_sequence_t *qc_c_arith_qq_mul_seq(int bits) {
    if (bits <= 0 || bits > 64) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] result, [n..2n-1] a, [2n..3n-1] b, [3n] ext_ctrl */
    uint32_t reg_qubits = 3 * n + 1;
    uint32_t headroom = n * 8 + 128;
    circuit_ctx_t *ctx = cmul_create_capture_ctx(reg_qubits + headroom);
    if (!ctx) return NULL;

    uint32_t start;
    if (qc_qubit_alloc_n(ctx, reg_qubits, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t result[64], a[64], b[64];
    for (uint32_t i = 0; i < n; i++) {
        result[i] = i;
        a[i]      = n + i;
        b[i]      = 2 * n + i;
    }
    uint32_t ext_ctrl = 3 * n;

    qc_error_t err = qc_toffoli_cmul_qq(ctx, result, n,
                                          a, n, b, n, ext_ctrl);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = cmul_capture_circuit_to_sequence(ctx);
    if (seq) seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}
