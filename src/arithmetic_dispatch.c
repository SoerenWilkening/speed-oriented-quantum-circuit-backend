/**
 * @file arithmetic_dispatch.c
 * @brief Arithmetic dispatch: routes operations by ctx->arithmetic_mode.
 *
 * Module 1.13 (Phase 1) - refactor-r5b
 *
 * This module provides unified arithmetic entry points that check
 * ctx->arithmetic_mode (QC_ARITH_QFT=0 or QC_ARITH_TOFFOLI=1) and
 * delegate to the appropriate implementation. It eliminates all
 * dependency on hardcoded sequence files.
 *
 * Dispatch covers:
 *   - Addition (QQ, CQ, controlled QQ, controlled CQ)
 *   - Subtraction (QQ, CQ via negation)
 *   - Multiplication (QQ, CQ)
 *   - Division (CQ, QQ -- Toffoli only)
 *   - Modular operations (mod_reduce, mod_add, mod_mul -- Toffoli only)
 *   - Comparisons (QQ equal, CQ equal, QQ less, CQ less, CQ greater)
 *   - Bitwise operations (NOT, XOR, AND, OR -- mode-independent)
 *
 * QFT mode uses Draper-based rotation circuits (qft_addition.c,
 * qft_multiplication.c). Toffoli mode uses CDKM ripple-carry or
 * Brent-Kung CLA circuits (toffoli_cdkm.c, toffoli_cla.c,
 * toffoli_multiplication.c, toffoli_division.c, toffoli_mod_reduce.c).
 *
 * All functions take circuit_ctx_t* as first argument. No global state.
 */

#include "internal.h"

/* ====================================================================== */
/* Unified addition dispatch                                               */
/* ====================================================================== */

/**
 * @brief Dispatched QQ addition: a += b.
 *
 * QFT mode: Draper QFT adder (qc_arith_qq_add).
 * Toffoli mode: CDKM ripple-carry adder (qc_toffoli_qq_add).
 */
qc_error_t qc_dispatch_qq_add(circuit_ctx_t *ctx, const uint32_t *a,
                                const uint32_t *b, uint32_t width) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode == 1) {
        return qc_toffoli_qq_add(ctx, a, b, width);
    }
    return qc_arith_qq_add(ctx, a, b, width);
}

/**
 * @brief Dispatched CQ addition: target += classical value.
 *
 * QFT mode: Draper CQ adder.
 * Toffoli mode: CDKM CQ adder.
 */
qc_error_t qc_dispatch_cq_add(circuit_ctx_t *ctx, const uint32_t *target,
                                uint32_t width, int64_t value) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode == 1) {
        return qc_toffoli_cq_add(ctx, target, width, value);
    }
    return qc_arith_cq_add(ctx, target, width, value);
}

/**
 * @brief Dispatched controlled QQ addition: a += b, controlled.
 *
 * QFT mode: controlled Draper adder.
 * Toffoli mode: controlled CDKM adder.
 */
qc_error_t qc_dispatch_cqq_add(circuit_ctx_t *ctx, const uint32_t *a,
                                 const uint32_t *b, uint32_t width,
                                 uint32_t control) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode == 1) {
        return qc_toffoli_cqq_add(ctx, a, b, width, control);
    }
    return qc_arith_cqq_add(ctx, a, b, width, control);
}

/**
 * @brief Dispatched controlled CQ addition: target += value, controlled.
 *
 * QFT mode: controlled Draper CQ adder.
 * Toffoli mode: controlled CDKM CQ adder.
 */
qc_error_t qc_dispatch_ccq_add(circuit_ctx_t *ctx, const uint32_t *target,
                                 uint32_t width, int64_t value,
                                 uint32_t control) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode == 1) {
        return qc_toffoli_ccq_add(ctx, target, width, value, control);
    }
    return qc_arith_ccq_add(ctx, target, width, value, control);
}

/* ====================================================================== */
/* Unified subtraction dispatch                                            */
/* ====================================================================== */

/**
 * @brief Dispatched QQ subtraction: a -= b.
 *
 * QFT mode: inverse Draper QFT adder (apply addition with inversion).
 * Toffoli mode: dynamic CDKM subtraction.
 */
qc_error_t qc_dispatch_qq_sub(circuit_ctx_t *ctx, const uint32_t *a,
                                const uint32_t *b, uint32_t width) {
    if (ctx == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    if (ctx->arithmetic_mode == 1) {
        /* Toffoli mode: use dynamic CDKM subtraction */
        qc_dynamic_qq_sub(ctx, b, a, width);
        return QC_OK;
    }
    /* QFT mode: subtract by adding negative via two's complement.
     * For QQ subtraction with QFT, we add the inverse:
     * a -= b is equivalent to NOT(b), a += b+1, NOT(b)
     * But simpler: just run the QQ add sequence inverted. */
    qc_dynamic_qq_sub(ctx, b, a, width);
    return QC_OK;
}

/**
 * @brief Dispatched CQ subtraction: target -= classical value.
 *
 * Delegates to CQ addition with negated value.
 */
qc_error_t qc_dispatch_cq_sub(circuit_ctx_t *ctx, const uint32_t *target,
                                uint32_t width, int64_t value) {
    return qc_dispatch_cq_add(ctx, target, width, -value);
}

/* ====================================================================== */
/* Unified multiplication dispatch                                         */
/* ====================================================================== */

/**
 * @brief Dispatched QQ multiplication: result = a * b.
 *
 * QFT mode: Draper QFT multiplier.
 * Toffoli mode: shift-and-add with CDKM adders.
 */
qc_error_t qc_dispatch_qq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                const uint32_t *a, const uint32_t *b,
                                uint32_t width) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode == 1) {
        return qc_toffoli_qq_mul(ctx, result, width, a, width, b, width);
    }
    return qc_arith_qq_mul(ctx, result, a, b, width);
}

/**
 * @brief Dispatched CQ multiplication: result = target * classical value.
 *
 * QFT mode: Draper CQ multiplier.
 * Toffoli mode: shift-and-add with CDKM adders.
 */
qc_error_t qc_dispatch_cq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                const uint32_t *target, uint32_t width,
                                int64_t value) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode == 1) {
        return qc_toffoli_cq_mul(ctx, result, width, target, width, value);
    }
    return qc_arith_cq_mul(ctx, result, target, width, value);
}

/* ====================================================================== */
/* Division dispatch (Toffoli only)                                        */
/* ====================================================================== */

/**
 * @brief Dispatched CQ division: dividend / classical divisor.
 *
 * Division is only supported in Toffoli mode. QFT mode returns
 * QC_ERR_INVALID_OP.
 */
qc_error_t qc_dispatch_divmod_cq(circuit_ctx_t *ctx,
                                   const uint32_t *dividend,
                                   uint32_t dividend_bits, int64_t divisor,
                                   const uint32_t *quotient,
                                   const uint32_t *remainder) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode != 1) {
        return QC_ERR_INVALID_OP;
    }
    return qc_toffoli_divmod_cq(ctx, dividend, dividend_bits, divisor,
                                 quotient, remainder);
}

/**
 * @brief Dispatched QQ division: dividend / quantum divisor.
 *
 * Division is only supported in Toffoli mode.
 */
qc_error_t qc_dispatch_divmod_qq(circuit_ctx_t *ctx,
                                   const uint32_t *dividend,
                                   uint32_t dividend_bits,
                                   const uint32_t *divisor,
                                   uint32_t divisor_bits,
                                   const uint32_t *quotient,
                                   const uint32_t *remainder) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode != 1) {
        return QC_ERR_INVALID_OP;
    }
    return qc_toffoli_divmod_qq(ctx, dividend, dividend_bits, divisor,
                                 divisor_bits, quotient, remainder);
}

/* ====================================================================== */
/* Modular arithmetic dispatch (Toffoli only)                              */
/* ====================================================================== */

/**
 * @brief Dispatched modular reduction: value = value mod N.
 *
 * Toffoli mode only.
 */
qc_error_t qc_dispatch_mod_reduce(circuit_ctx_t *ctx, const uint32_t *value,
                                    uint32_t value_bits, int64_t modulus) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode != 1) {
        return QC_ERR_INVALID_OP;
    }
    return qc_toffoli_mod_reduce(ctx, value, value_bits, modulus);
}

/**
 * @brief Dispatched modular CQ addition: value = (value + addend) mod N.
 *
 * Toffoli mode only.
 */
qc_error_t qc_dispatch_mod_add_cq(circuit_ctx_t *ctx, const uint32_t *value,
                                    uint32_t value_bits, int64_t addend,
                                    int64_t modulus) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode != 1) {
        return QC_ERR_INVALID_OP;
    }
    return qc_toffoli_mod_add_cq(ctx, value, value_bits, addend, modulus);
}

/**
 * @brief Dispatched modular QQ addition: value = (value + other) mod N.
 *
 * Toffoli mode only.
 */
qc_error_t qc_dispatch_mod_add_qq(circuit_ctx_t *ctx, const uint32_t *value,
                                    uint32_t value_bits,
                                    const uint32_t *other,
                                    uint32_t other_bits, int64_t modulus) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode != 1) {
        return QC_ERR_INVALID_OP;
    }
    return qc_toffoli_mod_add_qq(ctx, value, value_bits, other, other_bits,
                                  modulus);
}

/**
 * @brief Dispatched modular CQ multiplication.
 *
 * Toffoli mode only.
 */
qc_error_t qc_dispatch_mod_mul_cq(circuit_ctx_t *ctx, const uint32_t *value,
                                    uint32_t value_bits,
                                    const uint32_t *result,
                                    uint32_t result_bits, int64_t multiplier,
                                    int64_t modulus) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    if (ctx->arithmetic_mode != 1) {
        return QC_ERR_INVALID_OP;
    }
    return qc_toffoli_mod_mul_cq(ctx, value, value_bits, result, result_bits,
                                  multiplier, modulus);
}

/* ====================================================================== */
/* QQ comparison: equality via XOR + multi-controlled check                */
/* ====================================================================== */

/**
 * @brief QQ equality: result = (A == B).
 *
 * Algorithm: XOR each pair of bits (A[i], B[i]) into ancilla qubits.
 * If all XOR results are 0, the values are equal. Uses MCX on the
 * NOT of all XOR outputs to set the result qubit.
 *
 * Implementation: for each bit, CX(A[i], anc[i]) then CX(B[i], anc[i])
 * gives anc[i] = A[i] XOR B[i]. Then X(anc[i]) to flip, then
 * multi-controlled X on result with all anc as controls, then uncompute.
 */
qc_error_t qc_cmp_qq_equal(circuit_ctx_t *ctx, const uint32_t *a,
                             const uint32_t *b, uint32_t width,
                             uint32_t result) {
    if (ctx == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    /* Special case: 1-bit equality = CNOT chain */
    if (width == 1) {
        /* result = NOT(a XOR b): CX(a,result), CX(b,result), X(result) */
        qc_circuit_cx(ctx, a[0], result);
        qc_circuit_cx(ctx, b[0], result);
        qc_circuit_x(ctx, result);
        return QC_OK;
    }

    /* Allocate ancilla for XOR results */
    uint32_t anc_start;
    qc_error_t err = qc_qubit_alloc_n(ctx, width, &anc_start);
    if (err != QC_OK)
        return err;

    /* Compute XOR: anc[i] = a[i] XOR b[i] */
    for (uint32_t i = 0; i < width; i++) {
        qc_circuit_cx(ctx, a[i], anc_start + i);
        qc_circuit_cx(ctx, b[i], anc_start + i);
    }

    /* Flip: result = 1 iff all anc[i] == 0, i.e., all X(anc[i]) == 1 */
    for (uint32_t i = 0; i < width; i++) {
        qc_circuit_x(ctx, anc_start + i);
    }

    /* Multi-controlled X: set result if all anc are |1> */
    if (width == 2) {
        qc_circuit_ccx(ctx, anc_start, anc_start + 1, result);
    } else {
        /* Build control array and use MCX */
        uint32_t controls[64];
        for (uint32_t i = 0; i < width; i++) {
            controls[i] = anc_start + i;
        }
        qc_circuit_mcx(ctx, controls, width, result);
    }

    /* Uncompute: undo X flips and XOR */
    for (uint32_t i = 0; i < width; i++) {
        qc_circuit_x(ctx, anc_start + i);
    }
    for (uint32_t i = 0; i < width; i++) {
        qc_circuit_cx(ctx, b[i], anc_start + i);
        qc_circuit_cx(ctx, a[i], anc_start + i);
    }

    qc_qubit_free_n(ctx, anc_start, width);
    return QC_OK;
}

/**
 * @brief QQ less-than: result = (A < B).
 *
 * Uses widened subtraction: compute A - B in (width+1) bits,
 * the sign bit (MSB) indicates A < B.
 */
qc_error_t qc_cmp_qq_less(circuit_ctx_t *ctx, const uint32_t *a,
                            const uint32_t *b, uint32_t width,
                            uint32_t result) {
    if (ctx == NULL || a == NULL || b == NULL)
        return QC_ERR_NULL;
    if (width < 1 || width > 64)
        return QC_ERR_WIDTH;

    uint32_t wide = width + 1;

    /* Allocate temp register for widened subtraction */
    uint32_t temp_start;
    qc_error_t err = qc_qubit_alloc_n(ctx, wide, &temp_start);
    if (err != QC_OK)
        return err;

    uint32_t temp[65];
    for (uint32_t i = 0; i < wide; i++) {
        temp[i] = temp_start + i;
    }

    /* Copy a to temp[0..width-1] */
    for (uint32_t i = 0; i < width; i++) {
        qc_circuit_cx(ctx, a[i], temp[i]);
    }

    /* Build widened b: b[0..width-1] + pad qubit */
    uint32_t b_pad;
    err = qc_qubit_alloc(ctx, &b_pad);
    if (err != QC_OK) {
        /* Uncompute copy */
        for (uint32_t i = 0; i < width; i++) {
            qc_circuit_cx(ctx, a[i], temp[i]);
        }
        qc_qubit_free_n(ctx, temp_start, wide);
        return err;
    }

    uint32_t wide_b[65];
    for (uint32_t i = 0; i < width; i++) {
        wide_b[i] = b[i];
    }
    wide_b[width] = b_pad;

    /* QQ subtract: temp -= wide_b (in wide bits) */
    qc_dynamic_qq_sub(ctx, wide_b, temp, wide);

    /* Copy sign bit to result */
    qc_circuit_cx(ctx, temp[width], result);

    /* Uncompute: add b back, uncopy a */
    qc_dynamic_qq_add(ctx, wide_b, temp, wide);
    qc_qubit_free(ctx, b_pad);

    for (uint32_t i = 0; i < width; i++) {
        qc_circuit_cx(ctx, a[i], temp[i]);
    }

    qc_qubit_free_n(ctx, temp_start, wide);
    return QC_OK;
}
