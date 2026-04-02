/**
 * @file divmod_sequences.c
 * @brief Capture-based divmod sequence builders for compile-mode replay.
 *
 * Issue: refactor-a1b
 *
 * Divmod operations allocate ancillae dynamically inside the circuit, making
 * traditional pre-built sequence construction impractical.  Instead, we use a
 * capture approach:
 *
 *   1. Create a temporary circuit_ctx_t with virtual qubit indices.
 *   2. Run the divmod operation (which emits gates + allocates ancillae).
 *   3. Copy the resulting internal gates into a qc_sequence_t.
 *   4. Destroy the temporary circuit.
 *
 * The sequence's qubit layout is:
 *   CQ:  [0..n-1] dividend, [n..2n-1] quotient, [2n..3n-1] remainder
 *   QQ:  [0..n-1] dividend, [n..2n-1] divisor, [2n..3n-1] quotient, [3n..4n-1] remainder
 *
 * Controlled variants add one extra control qubit at the end of the layout.
 *
 * Ancillae allocated internally by divmod are mapped to higher virtual indices.
 * The caller must ensure enough qubits are available when replaying.
 *
 * Functions:
 *   - qc_divmod_cq_seq         -- uncontrolled CQ divmod
 *   - qc_divmod_qq_seq         -- uncontrolled QQ divmod
 *   - qc_c_divmod_cq_seq       -- controlled CQ divmod
 *   - qc_c_divmod_qq_seq       -- controlled QQ divmod
 */

#include "internal.h"

#include <stdlib.h>
#include <string.h>

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
static qc_sequence_t *capture_circuit_to_sequence(circuit_ctx_t *ctx) {
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
            /* Allocate at least 1 slot (matching qc_sequence_alloc convention) */
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
    /* Cleanup on allocation failure */
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
/* Helper: create temp circuit configured for gate capture                 */
/* ====================================================================== */

static circuit_ctx_t *create_capture_ctx(uint32_t initial_qubits) {
    circuit_ctx_t *ctx = qc_circuit_create(initial_qubits);
    if (!ctx) return NULL;
    qc_circuit_set_simulate(ctx, true);
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);
    return ctx;
}

/* ====================================================================== */
/* qc_divmod_cq_seq -- uncontrolled CQ divmod sequence                     */
/* ====================================================================== */

QC_API qc_sequence_t *qc_divmod_cq_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 64) return NULL;
    if (value == 0) return NULL;  /* division by zero */

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] dividend, [n..2n-1] quotient, [2n..3n-1] remainder */
    uint32_t reg_qubits = 3 * n;
    uint32_t headroom = n * 4 + 128;  /* generous ancilla headroom */
    circuit_ctx_t *ctx = create_capture_ctx(reg_qubits + headroom);
    if (!ctx) return NULL;

    /* Pre-allocate register qubits */
    uint32_t start;
    if (qc_qubit_alloc_n(ctx, reg_qubits, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t dividend[64], quotient[64], remainder[64];
    for (uint32_t i = 0; i < n; i++) {
        dividend[i]  = i;
        quotient[i]  = n + i;
        remainder[i] = 2 * n + i;
    }

    qc_error_t err = qc_toffoli_divmod_cq(ctx, dividend, n, value,
                                            quotient, remainder);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = capture_circuit_to_sequence(ctx);
    if (seq) seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}

/* ====================================================================== */
/* qc_divmod_qq_seq -- uncontrolled QQ divmod sequence                     */
/* ====================================================================== */

QC_API qc_sequence_t *qc_divmod_qq_seq(int bits) {
    if (bits <= 0 || bits > 64) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] dividend, [n..2n-1] divisor,
     *         [2n..3n-1] quotient, [3n..4n-1] remainder */
    uint32_t reg_qubits = 4 * n;
    uint32_t headroom = n * 8 + 128;
    circuit_ctx_t *ctx = create_capture_ctx(reg_qubits + headroom);
    if (!ctx) return NULL;

    uint32_t start;
    if (qc_qubit_alloc_n(ctx, reg_qubits, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t dividend[64], divisor[64], quotient[64], remainder[64];
    for (uint32_t i = 0; i < n; i++) {
        dividend[i]  = i;
        divisor[i]   = n + i;
        quotient[i]  = 2 * n + i;
        remainder[i] = 3 * n + i;
    }

    qc_error_t err = qc_toffoli_divmod_qq(ctx, dividend, n,
                                            divisor, n,
                                            quotient, remainder);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = capture_circuit_to_sequence(ctx);
    if (seq) seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}

/* ====================================================================== */
/* qc_c_divmod_cq_seq -- controlled CQ divmod sequence                     */
/* ====================================================================== */

QC_API qc_sequence_t *qc_c_divmod_cq_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 64) return NULL;
    if (value == 0) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] dividend, [n..2n-1] quotient,
     *         [2n..3n-1] remainder, [3n] ext_ctrl */
    uint32_t reg_qubits = 3 * n + 1;
    uint32_t headroom = n * 4 + 128;
    circuit_ctx_t *ctx = create_capture_ctx(reg_qubits + headroom);
    if (!ctx) return NULL;

    uint32_t start;
    if (qc_qubit_alloc_n(ctx, reg_qubits, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t dividend[64], quotient[64], remainder[64];
    for (uint32_t i = 0; i < n; i++) {
        dividend[i]  = i;
        quotient[i]  = n + i;
        remainder[i] = 2 * n + i;
    }
    uint32_t ext_ctrl = 3 * n;

    qc_error_t err = qc_toffoli_cdivmod_cq(ctx, dividend, n, value,
                                             quotient, remainder, ext_ctrl);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = capture_circuit_to_sequence(ctx);
    if (seq) seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}

/* ====================================================================== */
/* qc_c_divmod_qq_seq -- controlled QQ divmod sequence                     */
/* ====================================================================== */

QC_API qc_sequence_t *qc_c_divmod_qq_seq(int bits) {
    if (bits <= 0 || bits > 64) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] dividend, [n..2n-1] divisor,
     *         [2n..3n-1] quotient, [3n..4n-1] remainder, [4n] ext_ctrl */
    uint32_t reg_qubits = 4 * n + 1;
    uint32_t headroom = n * 8 + 128;
    circuit_ctx_t *ctx = create_capture_ctx(reg_qubits + headroom);
    if (!ctx) return NULL;

    uint32_t start;
    if (qc_qubit_alloc_n(ctx, reg_qubits, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t dividend[64], divisor[64], quotient[64], remainder[64];
    for (uint32_t i = 0; i < n; i++) {
        dividend[i]  = i;
        divisor[i]   = n + i;
        quotient[i]  = 2 * n + i;
        remainder[i] = 3 * n + i;
    }
    uint32_t ext_ctrl = 4 * n;

    qc_error_t err = qc_toffoli_cdivmod_qq(ctx, dividend, n,
                                             divisor, n,
                                             quotient, remainder, ext_ctrl);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = capture_circuit_to_sequence(ctx);
    if (seq) seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}
