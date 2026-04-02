/**
 * @file integer_comparison_toffoli.c
 * @brief Toffoli-variant QQ comparison sequence builders.
 *
 * Provides qc_cmp_qq_less_toffoli_seq (uncontrolled) and
 * qc_c_cmp_qq_less_toffoli_seq (controlled) using dynamic CDKM adders
 * called directly on the capture context.
 *
 * Uses qc_dynamic_qq_add/sub for uncontrolled and qc_dynamic_cqq_add/sub
 * for controlled variants, avoiding the nested capture issue that occurs
 * when replaying sub-sequences inside a capture context.
 *
 * Issue: refactor-4ma
 */

#include "capture_helpers.h"

#include <stdlib.h>

/* ====================================================================== */
/* qc_cmp_qq_less_toffoli_seq -- uncontrolled toffoli QQ less-than         */
/* ====================================================================== */

/**
 * @brief Build a toffoli QQ less-than comparison sequence.
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1..2*bits]=B,
 *               [2*bits+1]=borrow, [2*bits+2]=zero_ext
 *
 * Algorithm (borrow-detect via extended addition):
 *   1. A -= B           (dynamic CDKM subtraction on n bits)
 *   2. [A,borrow] += [B,zero_ext]  (dynamic CDKM addition on n+1 bits)
 *   3. CX(borrow -> result)
 *   4. Undo step 2      (dynamic CDKM subtraction on n+1 bits)
 *   5. A += B           (dynamic CDKM addition on n bits, restore)
 *
 * @param bits  Width of operands (1-63).
 * @return Sequence (caller must free), or NULL on error.
 */
qc_sequence_t *qc_cmp_qq_less_toffoli_seq(int bits) {
    if (bits <= 0 || bits > 63)
        return NULL;

    uint32_t n = (uint32_t)bits;
    uint32_t total_reg = 2 * n + 3;  /* result + A + B + borrow + zero_ext */

    /* Create capture circuit with plenty of room for ancilla */
    circuit_ctx_t *ctx = cmp_create_capture_ctx(total_reg + 256);
    if (!ctx) return NULL;

    uint32_t alloc_start;
    qc_qubit_alloc_n(ctx, total_reg, &alloc_start);

    /* Build register arrays */
    uint32_t a[64], b[64], a_ext[65], b_ext[65];
    for (uint32_t i = 0; i < n; i++) {
        a[i] = i + 1;          /* A register: qubits 1..n */
        b[i] = n + 1 + i;      /* B register: qubits n+1..2n */
        a_ext[i] = i + 1;
        b_ext[i] = n + 1 + i;
    }
    a_ext[n] = 2 * n + 1;      /* borrow qubit */
    b_ext[n] = 2 * n + 2;      /* zero_ext qubit */

    /* Step 1: A -= B (dynamic CDKM subtraction, b[] is subtracted from a[]) */
    qc_dynamic_qq_sub(ctx, b, a, n);

    /* Step 2: [A,borrow] += [B,zero_ext] (dynamic CDKM addition on n+1 bits) */
    qc_dynamic_qq_add(ctx, b_ext, a_ext, n + 1);

    /* Step 3: CX(control=borrow, target=result) */
    qc_circuit_cx(ctx, 2 * n + 1, 0);

    /* Step 4: Undo step 2 (subtract extended) */
    qc_dynamic_qq_sub(ctx, b_ext, a_ext, n + 1);

    /* Step 5: A += B (restore) */
    qc_dynamic_qq_add(ctx, b, a, n);

    /* Capture and cleanup */
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);

    if (seq)
        seq->total_qubits = total_reg;
    return seq;
}

/* ====================================================================== */
/* qc_c_cmp_qq_less_toffoli_seq -- controlled toffoli QQ less-than         */
/* ====================================================================== */

/**
 * @brief Build a controlled toffoli QQ less-than comparison sequence.
 *
 * Qubit layout: [0]=result, [1..bits]=A, [bits+1..2*bits]=B,
 *               [2*bits+1]=borrow, [2*bits+2]=zero_ext, [2*bits+3]=control
 *
 * Same algorithm as uncontrolled, but uses controlled CDKM adders
 * and CCX instead of CX.
 *
 * @param bits  Width of operands (1-63).
 * @return Sequence (caller must free), or NULL on error.
 */
qc_sequence_t *qc_c_cmp_qq_less_toffoli_seq(int bits) {
    if (bits <= 0 || bits > 63)
        return NULL;

    uint32_t n = (uint32_t)bits;
    uint32_t total_reg = 2 * n + 4;  /* result + A + B + borrow + zero_ext + control */
    uint32_t ctrl = 2 * n + 3;

    /* Create capture circuit with plenty of room for ancilla */
    circuit_ctx_t *ctx = cmp_create_capture_ctx(total_reg + 256);
    if (!ctx) return NULL;

    uint32_t alloc_start;
    qc_qubit_alloc_n(ctx, total_reg, &alloc_start);

    /* Build register arrays */
    uint32_t a[64], b[64], a_ext[65], b_ext[65];
    for (uint32_t i = 0; i < n; i++) {
        a[i] = i + 1;          /* A register: qubits 1..n */
        b[i] = n + 1 + i;      /* B register: qubits n+1..2n */
        a_ext[i] = i + 1;
        b_ext[i] = n + 1 + i;
    }
    a_ext[n] = 2 * n + 1;      /* borrow qubit */
    b_ext[n] = 2 * n + 2;      /* zero_ext qubit */

    /* Step 1: A -= B controlled (dynamic controlled CDKM subtraction) */
    qc_dynamic_cqq_sub(ctx, b, a, n, ctrl);

    /* Step 2: [A,borrow] += [B,zero_ext] controlled (n+1 bits) */
    qc_dynamic_cqq_add(ctx, b_ext, a_ext, n + 1, ctrl);

    /* Step 3: CCX(control, borrow, result) */
    qc_circuit_ccx(ctx, ctrl, 2 * n + 1, 0);

    /* Step 4: Undo step 2 controlled */
    qc_dynamic_cqq_sub(ctx, b_ext, a_ext, n + 1, ctrl);

    /* Step 5: A += B controlled (restore) */
    qc_dynamic_cqq_add(ctx, b, a, n, ctrl);

    /* Capture and cleanup */
    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    qc_circuit_destroy(ctx);

    if (seq)
        seq->total_qubits = total_reg;
    return seq;
}
