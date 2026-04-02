/**
 * @file toffoli_mul_sequences.c
 * @brief Capture-based toffoli multiplication sequence builders.
 *
 * Issue: refactor-4ma, refactor-fdg (Issues 4+5)
 *
 * Provides qc_toffoli_cq_mul_seq and qc_toffoli_qq_mul_seq, which build
 * pre-compiled gate sequences for uncontrolled toffoli multiplication.
 *
 * These follow the same capture approach as cmul_sequences.c:
 *   1. Create a temporary circuit_ctx_t with virtual qubit indices.
 *   2. Run the toffoli multiplication (which emits gates + ancillae).
 *   3. Copy the resulting internal gates into a qc_sequence_t.
 *   4. Destroy the temporary circuit.
 *
 * Qubit layouts:
 *   toffoli_cq_mul:   [0..n-1] result, [n..2n-1] target
 *   toffoli_qq_mul:   [0..n-1] result, [n..2n-1] a, [2n..3n-1] b
 *   toffoli_cmul_cq:  [0..n-1] result, [n..2n-1] target, [2n] control
 *   toffoli_cmul_qq:  [0..n-1] result, [n..2n-1] a, [2n..3n-1] b, [3n] control
 */

#include "capture_helpers.h"

#include <stdlib.h>

/* ====================================================================== */
/* qc_toffoli_cq_mul_seq -- uncontrolled toffoli CQ multiplication         */
/* ====================================================================== */

/**
 * @brief Build a toffoli CQ multiplication sequence.
 *
 * Qubit layout: [0..n-1] result, [n..2n-1] target.
 * Ancillae may be mapped to higher virtual indices.
 *
 * @param bits   Width (1-64).
 * @param value  Classical multiplier.
 * @return Sequence, or NULL on error. Caller must free.
 */
QC_API qc_sequence_t *qc_toffoli_cq_mul_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 64) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] result, [n..2n-1] target */
    uint32_t reg_qubits = 2 * n;
    uint32_t headroom = n * 4 + 128;
    circuit_ctx_t *ctx = cmp_create_capture_ctx(reg_qubits + headroom);
    if (!ctx) return NULL;

    /* Toffoli mul functions use toffoli gates directly, no need to set
     * QC_ARITH_TOFFOLI on the capture context (reviewer note #2). */

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

    qc_error_t err = qc_toffoli_cq_mul(ctx, result, n, target, n, value);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t actual_qubits = ctx->allocator->next_qubit;
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);

    if (seq) seq->total_qubits = actual_qubits;
    return seq;
}

/* ====================================================================== */
/* qc_toffoli_qq_mul_seq -- uncontrolled toffoli QQ multiplication         */
/* ====================================================================== */

/**
 * @brief Build a toffoli QQ multiplication sequence.
 *
 * Qubit layout: [0..n-1] result, [n..2n-1] a, [2n..3n-1] b.
 * Ancillae may be mapped to higher virtual indices.
 *
 * @param bits   Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
QC_API qc_sequence_t *qc_toffoli_qq_mul_seq(int bits) {
    if (bits <= 0 || bits > 64) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] result, [n..2n-1] a, [2n..3n-1] b */
    uint32_t reg_qubits = 3 * n;
    uint32_t headroom = n * 8 + 128;
    circuit_ctx_t *ctx = cmp_create_capture_ctx(reg_qubits + headroom);
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

    qc_error_t err = qc_toffoli_qq_mul(ctx, result, n, a, n, b, n);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t actual_qubits = ctx->allocator->next_qubit;
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);

    if (seq) seq->total_qubits = actual_qubits;
    return seq;
}

/* ====================================================================== */
/* qc_toffoli_cmul_cq_seq -- controlled toffoli CQ multiplication         */
/* ====================================================================== */

/**
 * @brief Build a controlled toffoli CQ multiplication sequence.
 *
 * Qubit layout: [0..n-1] result, [n..2n-1] target, [2n] control.
 * Ancillae may be mapped to higher virtual indices.
 *
 * @param bits   Width (1-64).
 * @param value  Classical multiplier.
 * @return Sequence, or NULL on error. Caller must free.
 */
QC_API qc_sequence_t *qc_toffoli_cmul_cq_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 64) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] result, [n..2n-1] target, [2n] control */
    uint32_t reg_qubits = 2 * n + 1;
    uint32_t headroom = n * 4 + 128;
    circuit_ctx_t *ctx = cmp_create_capture_ctx(reg_qubits + headroom);
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
    uint32_t control = 2 * n;

    qc_error_t err = qc_toffoli_cmul_cq(ctx, result, n, target, n, value, control);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t actual_qubits = ctx->allocator->next_qubit;
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);

    if (seq) seq->total_qubits = actual_qubits;
    return seq;
}

/* ====================================================================== */
/* qc_toffoli_cmul_qq_seq -- controlled toffoli QQ multiplication         */
/* ====================================================================== */

/**
 * @brief Build a controlled toffoli QQ multiplication sequence.
 *
 * Qubit layout: [0..n-1] result, [n..2n-1] a, [2n..3n-1] b, [3n] control.
 * Ancillae may be mapped to higher virtual indices.
 *
 * @param bits   Width (1-64).
 * @return Sequence, or NULL on error. Caller must free.
 */
QC_API qc_sequence_t *qc_toffoli_cmul_qq_seq(int bits) {
    if (bits <= 0 || bits > 64) return NULL;

    uint32_t n = (uint32_t)bits;

    /* Layout: [0..n-1] result, [n..2n-1] a, [2n..3n-1] b, [3n] control */
    uint32_t reg_qubits = 3 * n + 1;
    uint32_t headroom = n * 8 + 128;
    circuit_ctx_t *ctx = cmp_create_capture_ctx(reg_qubits + headroom);
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
    uint32_t control = 3 * n;

    qc_error_t err = qc_toffoli_cmul_qq(ctx, result, n, a, n, b, n, control);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t actual_qubits = ctx->allocator->next_qubit;
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);

    if (seq) seq->total_qubits = actual_qubits;
    return seq;
}
