/**
 * @file toffoli_cla_seq.c
 * @brief Capture-based sequence builders for toffoli BK CLA adders.
 *
 * Each _seq wrapper creates a temporary capture circuit with
 * arith_mode = QC_ARITH_TOFFOLI, calls the public qc_toffoli_*
 * functions on it, then captures the emitted gates into a sequence.
 *
 * Issue: refactor-4ma
 */

#include "internal.h"
#include "capture_helpers.h"

#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* qc_sequence_dup -- deep-copy a sequence                                 */
/* ====================================================================== */

qc_sequence_t *qc_sequence_dup(const qc_sequence_t *src) {
    if (!src)
        return NULL;

    qc_sequence_t *dst = malloc(sizeof(qc_sequence_t));
    if (!dst)
        return NULL;

    dst->num_layer = src->num_layer;
    dst->used_layer = src->used_layer;
    dst->total_gate_count = src->total_gate_count;
    dst->total_qubits = src->total_qubits;

    uint32_t nl = src->used_layer;
    if (nl == 0) {
        dst->gates_per_layer = NULL;
        dst->seq = NULL;
        return dst;
    }

    dst->gates_per_layer = calloc(nl, sizeof(uint32_t));
    dst->seq = calloc(nl, sizeof(qc_gate_internal_t *));
    if (!dst->gates_per_layer || !dst->seq) {
        free(dst->gates_per_layer);
        free(dst->seq);
        free(dst);
        return NULL;
    }

    for (uint32_t layer = 0; layer < nl; layer++) {
        uint32_t gc = src->gates_per_layer[layer];
        dst->gates_per_layer[layer] = gc;

        if (gc == 0) {
            dst->seq[layer] = calloc(1, sizeof(qc_gate_internal_t));
            if (!dst->seq[layer]) goto fail;
            continue;
        }

        dst->seq[layer] = calloc(gc, sizeof(qc_gate_internal_t));
        if (!dst->seq[layer]) goto fail;

        for (uint32_t gi = 0; gi < gc; gi++) {
            const qc_gate_internal_t *sg = &src->seq[layer][gi];
            qc_gate_internal_t *dg = &dst->seq[layer][gi];

            dg->Gate = sg->Gate;
            dg->GateValue = sg->GateValue;
            dg->Target = sg->Target;
            dg->NumControls = sg->NumControls;
            dg->NumBasisGates = sg->NumBasisGates;
            dg->large_control = NULL;

            if (sg->NumControls <= QC_MAX_INLINE_CONTROLS) {
                for (uint32_t ci = 0; ci < sg->NumControls; ci++)
                    dg->Control[ci] = sg->Control[ci];
            } else if (sg->large_control) {
                dg->large_control = malloc(sg->NumControls * sizeof(uint32_t));
                if (!dg->large_control) goto fail;
                memcpy(dg->large_control, sg->large_control,
                       sg->NumControls * sizeof(uint32_t));
            }
        }
    }

    return dst;

fail:
    if (dst->seq) {
        for (uint32_t i = 0; i < nl; i++) {
            if (dst->seq[i]) {
                for (uint32_t g = 0; g < dst->gates_per_layer[i]; g++) {
                    if (dst->seq[i][g].large_control)
                        free(dst->seq[i][g].large_control);
                }
                free(dst->seq[i]);
            }
        }
        free(dst->seq);
    }
    free(dst->gates_per_layer);
    free(dst);
    return NULL;
}

/* ====================================================================== */
/* qc_toffoli_qq_add_bk_seq -- QQ BK CLA add sequence (capture-based)     */
/* ====================================================================== */

qc_sequence_t *qc_toffoli_qq_add_bk_seq(int bits) {
    if (bits < 2 || bits > 64)
        return NULL;

    uint32_t n = (uint32_t)bits;
    /* QQ add BK needs: n target + n source + ancilla.
     * Use generous headroom for internal ancilla allocation. */
    uint32_t total = 4 * n + 64;

    circuit_ctx_t *ctx = cmp_create_capture_ctx(total);
    if (!ctx)
        return NULL;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);
    qc_circuit_set_cla_override(ctx, 1);

    /* Pre-allocate register qubits: [0..n-1]=a, [n..2n-1]=b */
    uint32_t start;
    if (qc_qubit_alloc_n(ctx, 2 * n, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t a[64], b[64];
    for (uint32_t i = 0; i < n; i++) {
        a[i] = i;
        b[i] = n + i;
    }

    qc_error_t err = qc_toffoli_qq_add_bk(ctx, a, b, n);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    if (seq)
        seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}

/* ====================================================================== */
/* qc_toffoli_cq_add_bk_seq -- CQ BK CLA add sequence (capture-based)     */
/* ====================================================================== */

qc_sequence_t *qc_toffoli_cq_add_bk_seq(int bits, int64_t value) {
    if (bits < 1 || bits > 64)
        return NULL;

    uint32_t n = (uint32_t)bits;
    uint32_t total = 4 * n + 64;

    circuit_ctx_t *ctx = cmp_create_capture_ctx(total);
    if (!ctx)
        return NULL;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    /* Pre-allocate register qubits: [0..n-1]=target */
    uint32_t start;
    if (qc_qubit_alloc_n(ctx, n, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t target[64];
    for (uint32_t i = 0; i < n; i++)
        target[i] = i;

    qc_error_t err = qc_toffoli_cq_add(ctx, target, n, value);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    if (seq)
        seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}

/* ====================================================================== */
/* qc_toffoli_cqq_add_bk_seq -- controlled QQ add sequence (capture)      */
/* ====================================================================== */

qc_sequence_t *qc_toffoli_cqq_add_bk_seq(int bits) {
    if (bits < 1 || bits > 64)
        return NULL;

    uint32_t n = (uint32_t)bits;
    /* Layout: [0..n-1]=a, [n..2n-1]=b, [2n]=control + ancilla */
    uint32_t total = 4 * n + 64;

    circuit_ctx_t *ctx = cmp_create_capture_ctx(total);
    if (!ctx)
        return NULL;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    /* Pre-allocate: 2n register + 1 control qubit */
    uint32_t start;
    if (qc_qubit_alloc_n(ctx, 2 * n + 1, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t a[64], b[64];
    for (uint32_t i = 0; i < n; i++) {
        a[i] = i;
        b[i] = n + i;
    }
    uint32_t control = 2 * n;

    qc_error_t err = qc_toffoli_cqq_add(ctx, a, b, n, control);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    if (seq)
        seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}
