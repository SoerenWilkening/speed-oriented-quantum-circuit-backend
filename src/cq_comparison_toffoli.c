/**
 * @file cq_comparison_toffoli.c
 * @brief Toffoli-variant CQ comparison sequence builders.
 *
 * Issue: refactor-4ma, refactor-fdg (Issues 4+5)
 *
 * Provides CQ less-than and greater-than comparison sequences using
 * toffoli (CDKM) arithmetic. Both uncontrolled and controlled variants.
 *
 * Algorithm (borrow-detect via extended subtraction):
 *   1. [A, borrow] -= value  (CQ subtraction on n+1 bits)
 *   2. CX(borrow -> result)  (or CCX for controlled)
 *   3. [A, borrow] += value  (restore)
 *
 * CQ subtraction is implemented as qc_dynamic_cq_add(target, width, -value).
 * Controlled CQ subtraction uses qc_dynamic_ccq_add(target, width, -value, ctrl).
 *
 * Qubit layouts:
 *   uncontrolled: [0]=result, [1..n]=A, [n+1]=borrow
 *   controlled:   [0]=result, [1..n]=A, [n+1]=borrow, [n+2]=control
 */

#include "capture_helpers.h"

#include <stdlib.h>
#include <stdint.h>

/* ====================================================================== */
/* qc_cmp_cq_less_toffoli_seq -- uncontrolled toffoli CQ less-than         */
/* ====================================================================== */

/**
 * @brief Build a toffoli CQ less-than comparison sequence.
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1]=borrow_ancilla.
 *
 * @param bits   Width of operand A (1-63).
 * @param value  Classical comparand.
 * @return Sequence (caller must free), or NULL on error.
 */
QC_API qc_sequence_t *qc_cmp_cq_less_toffoli_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 63)
        return NULL;

    uint32_t n = (uint32_t)bits;
    uint32_t total_reg = n + 2;  /* result + A + borrow */

    /* Create capture circuit with headroom for CDKM ancillae */
    circuit_ctx_t *ctx = cmp_create_capture_ctx(total_reg + 256);
    if (!ctx) return NULL;

    uint32_t alloc_start;
    if (qc_qubit_alloc_n(ctx, total_reg, &alloc_start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    /* Build extended register array: a_ext[0..n-1] = A qubits, a_ext[n] = borrow */
    uint32_t a_ext[65];
    for (uint32_t i = 0; i < n; i++) {
        a_ext[i] = i + 1;      /* A register: qubits 1..n */
    }
    a_ext[n] = n + 1;          /* borrow qubit */

    /* Step 1: [A, borrow] -= value via CQ add with negated value */
    qc_dynamic_cq_add(ctx, a_ext, n + 1, -value);

    /* Step 2: CX(borrow -> result) */
    qc_circuit_cx(ctx, n + 1, 0);

    /* Step 3: [A, borrow] += value (restore) */
    qc_dynamic_cq_add(ctx, a_ext, n + 1, value);

    /* Capture and cleanup */
    uint32_t actual_qubits = ctx->allocator->next_qubit;
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);

    if (seq)
        seq->total_qubits = actual_qubits;
    return seq;
}

/* ====================================================================== */
/* qc_cmp_cq_greater_toffoli_seq -- uncontrolled toffoli CQ greater-than   */
/* ====================================================================== */

/**
 * @brief Build a toffoli CQ greater-than comparison sequence.
 *
 * A > value  iff  A < (value + 1), so delegates to less-than.
 * Returns NULL if value >= 2^bits - 1 (always false).
 *
 * @param bits   Width of operand A (1-63).
 * @param value  Classical comparand.
 * @return Sequence (caller must free), or NULL on error/always-false.
 */
QC_API qc_sequence_t *qc_cmp_cq_greater_toffoli_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 63)
        return NULL;

    /* A > value  iff  A < value+1.  If value+1 >= 2^bits, always false. */
    int64_t max_val = ((int64_t)1 << bits) - 1;
    if (value >= max_val)
        return NULL;

    return qc_cmp_cq_less_toffoli_seq(bits, value + 1);
}

/* ====================================================================== */
/* qc_c_cmp_cq_less_toffoli_seq -- controlled toffoli CQ less-than         */
/* ====================================================================== */

/**
 * @brief Build a controlled toffoli CQ less-than comparison sequence.
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1]=borrow, [bits+2]=control.
 *
 * Same algorithm as uncontrolled but uses controlled CDKM adders and CCX.
 *
 * @param bits   Width of operand A (1-63).
 * @param value  Classical comparand.
 * @return Sequence (caller must free), or NULL on error.
 */
QC_API qc_sequence_t *qc_c_cmp_cq_less_toffoli_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 63)
        return NULL;

    uint32_t n = (uint32_t)bits;
    uint32_t total_reg = n + 3;  /* result + A + borrow + control */
    uint32_t ctrl = n + 2;

    /* Create capture circuit with headroom for CDKM ancillae */
    circuit_ctx_t *ctx = cmp_create_capture_ctx(total_reg + 256);
    if (!ctx) return NULL;

    uint32_t alloc_start;
    if (qc_qubit_alloc_n(ctx, total_reg, &alloc_start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    /* Build extended register array: a_ext[0..n-1] = A qubits, a_ext[n] = borrow */
    uint32_t a_ext[65];
    for (uint32_t i = 0; i < n; i++) {
        a_ext[i] = i + 1;      /* A register: qubits 1..n */
    }
    a_ext[n] = n + 1;          /* borrow qubit */

    /* Step 1: controlled [A, borrow] -= value */
    qc_dynamic_ccq_add(ctx, a_ext, n + 1, -value, ctrl);

    /* Step 2: CCX(control, borrow, result) */
    qc_circuit_ccx(ctx, ctrl, n + 1, 0);

    /* Step 3: controlled [A, borrow] += value (restore) */
    qc_dynamic_ccq_add(ctx, a_ext, n + 1, value, ctrl);

    /* Capture and cleanup */
    uint32_t actual_qubits = ctx->allocator->next_qubit;
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);

    if (seq)
        seq->total_qubits = actual_qubits;
    return seq;
}

/* ====================================================================== */
/* qc_c_cmp_cq_greater_toffoli_seq -- controlled toffoli CQ greater-than   */
/* ====================================================================== */

/**
 * @brief Build a controlled toffoli CQ greater-than comparison sequence.
 *
 * A > value  iff  A < (value + 1), so delegates to controlled less-than.
 * Returns NULL if value >= 2^bits - 1 (always false).
 *
 * @param bits   Width of operand A (1-63).
 * @param value  Classical comparand.
 * @return Sequence (caller must free), or NULL on error/always-false.
 */
QC_API qc_sequence_t *qc_c_cmp_cq_greater_toffoli_seq(int bits, int64_t value) {
    if (bits <= 0 || bits > 63)
        return NULL;

    /* A > value  iff  A < value+1.  If value+1 >= 2^bits, always false. */
    int64_t max_val = ((int64_t)1 << bits) - 1;
    if (value >= max_val)
        return NULL;

    return qc_c_cmp_cq_less_toffoli_seq(bits, value + 1);
}
