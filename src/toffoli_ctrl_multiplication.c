/**
 * @file toffoli_ctrl_multiplication.c
 * @brief Controlled Toffoli-based multiplication (QQ and CQ variants).
 *
 * Refactored from: Quantum_Assembly/c_backend/src/ToffoliMultiplication.c
 * Issue: refactor-51o
 *
 * Implements externally-controlled shift-and-add multiplication using
 * dynamically emitted CDKM ripple-carry adders.  All gates are emitted
 * directly into circuit_ctx_t* ctx.
 *
 * Two public functions:
 *   qc_toffoli_cmul_qq  -- controlled quantum * quantum
 *   qc_toffoli_cmul_cq  -- controlled quantum * classical
 *
 * Qubit arrays: LSB-first (index 0 = LSB, index n-1 = MSB).
 *
 * Split from toffoli_multiplication.c to stay within the 500-line limit.
 */

#include "internal.h"

#include <stdlib.h>

/* ====================================================================== */
/* Controlled QQ Multiplication                                            */
/* ====================================================================== */

/**
 * @brief Controlled Toffoli QQ multiplication: result += a * b, gated by ext_ctrl.
 *
 * For each multiplier bit b[j], computes AND(b[j], ext_ctrl) into an ancilla,
 * then uses that ancilla as the control for a CDKM controlled addition of
 * a[0..width-1] into result[j..j+width-1], then uncomputes the AND.
 *
 * Width-1 special case: decomposes MCX(3 controls: a[0], b[j], ext_ctrl)
 * via AND-ancilla pattern (3 CCX gates).
 *
 * @param ctx         Circuit context.
 * @param result      Result register (accumulator), length = result_bits.
 * @param result_bits Width of result register.
 * @param a           Multiplicand register (preserved), length = a_bits.
 * @param a_bits      Width of multiplicand.
 * @param b           Multiplier register (preserved), length = b_bits.
 * @param b_bits      Width of multiplier.
 * @param ext_ctrl    External control qubit.
 * @return QC_OK on success, error code otherwise.
 */
QC_API qc_error_t qc_toffoli_cmul_qq(circuit_ctx_t *ctx, const uint32_t *result,
                                       uint32_t result_bits, const uint32_t *a,
                                       uint32_t a_bits, const uint32_t *b,
                                       uint32_t b_bits, uint32_t ext_ctrl) {
    if (!ctx) return QC_ERR_NULL;
    if (!result || !a || !b) return QC_ERR_NULL;
    if (result_bits == 0 || a_bits == 0 || b_bits == 0) return QC_ERR_WIDTH;

    uint32_t n = result_bits;

    /* Allocate AND-ancilla (reused across loop iterations) */
    uint32_t and_anc;
    if (qc_qubit_alloc(ctx, &and_anc) != QC_OK) return QC_ERR_ALLOC;

    for (uint32_t j = 0; j < n; j++) {
        uint32_t add_width = n - j;
        if (add_width > a_bits)
            add_width = a_bits;
        if (add_width < 1) break;
        if (j >= b_bits) break; /* No more multiplier bits */

        if (add_width == 1) {
            /* 1-bit case: MCX(3 controls) decomposed via AND-ancilla.
             *   CCX(and_anc, a[0], b[j])           -- compute partial AND
             *   CCX(result[n-1], and_anc, ext_ctrl) -- apply
             *   CCX(and_anc, a[0], b[j])           -- uncompute AND */
            qc_emit_ccx_or_decomp(ctx, and_anc, a[0], b[j]);
            qc_emit_ccx_or_decomp(ctx, result[n - 1], and_anc, ext_ctrl);
            qc_emit_ccx_or_decomp(ctx, and_anc, a[0], b[j]);
        } else {
            /* General case: AND-ancilla pattern for controlled addition.
             * Step 1: Compute AND: and_anc = b[j] AND ext_ctrl */
            qc_emit_ccx_or_decomp(ctx, and_anc, b[j], ext_ctrl);

            /* Step 2: Controlled addition with and_anc as control */
            qc_dynamic_cqq_add(ctx, a, &result[j], add_width, and_anc);

            /* Step 3: Uncompute AND (CCX is self-inverse) */
            qc_emit_ccx_or_decomp(ctx, and_anc, b[j], ext_ctrl);
        }
    }

    qc_qubit_free(ctx, and_anc);
    return QC_OK;
}

/* ====================================================================== */
/* Controlled CQ Multiplication                                            */
/* ====================================================================== */

/**
 * @brief Controlled Toffoli CQ multiplication: result += target * value, gated by ext_ctrl.
 *
 * For each set bit j of the classical value, performs a controlled addition
 * of target[0..width-1] into result[j..j+width-1], controlled by ext_ctrl.
 * No AND-ancilla needed because the classical bit selection is compile-time.
 *
 * Width-1 special case: CCX(result[n-1], target[0], ext_ctrl).
 *
 * @param ctx         Circuit context.
 * @param result      Result register (accumulator), length = result_bits.
 * @param result_bits Width of result register.
 * @param target      Multiplicand register (preserved), length = target_bits.
 * @param target_bits Width of multiplicand.
 * @param value       Classical integer to multiply by.
 * @param ext_ctrl    External control qubit.
 * @return QC_OK on success, error code otherwise.
 */
QC_API qc_error_t qc_toffoli_cmul_cq(circuit_ctx_t *ctx, const uint32_t *result,
                                       uint32_t result_bits, const uint32_t *target,
                                       uint32_t target_bits, int64_t value,
                                       uint32_t ext_ctrl) {
    if (!ctx) return QC_ERR_NULL;
    if (!result || !target) return QC_ERR_NULL;
    if (result_bits == 0 || target_bits == 0) return QC_ERR_WIDTH;

    uint32_t n = result_bits;

    int *bin = qc_two_complement(value, (int)n);
    if (!bin) return QC_ERR_ALLOC;

    for (uint32_t j = 0; j < n; j++) {
        /* bin is MSB-first: bit j (weight 2^j) is at bin[n-1-j] */
        if (bin[n - 1 - j] == 0)
            continue;

        uint32_t add_width = n - j;
        if (add_width > target_bits)
            add_width = target_bits;
        if (add_width < 1) break;

        if (add_width == 1) {
            /* 1-bit controlled addition: CCX */
            qc_emit_ccx_or_decomp(ctx, result[n - 1], target[0], ext_ctrl);
        } else {
            /* Controlled addition with ext_ctrl as the single control */
            qc_dynamic_cqq_add(ctx, target, &result[j], add_width, ext_ctrl);
        }
    }

    free(bin);
    return QC_OK;
}
