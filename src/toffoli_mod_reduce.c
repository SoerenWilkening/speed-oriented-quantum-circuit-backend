/**
 * @file toffoli_mod_reduce.c
 * @brief Dynamic Toffoli-based modular arithmetic for all widths.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/ToffoliModReduce.c
 * Module: 1.11 (Phase 1)
 * Issue: refactor-tpz
 *
 * Implements Beauregard 8-step modular addition with clean ancilla
 * uncomputation.  All gates emitted directly via circuit_ctx_t* ctx.
 *
 * Functions:
 *   qc_toffoli_mod_reduce    -- modular reduction
 *   qc_toffoli_mod_add_cq    -- Beauregard CQ modular addition
 *   qc_toffoli_mod_add_qq    -- Beauregard QQ modular addition
 *   qc_toffoli_mod_mul_cq    -- CQ modular multiplication
 *
 * References:
 *   Beauregard (2003) "Circuit for Shor's algorithm using 2n+3 qubits"
 */

#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Modular Reduction: value = value mod N                                  */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_mod_reduce(circuit_ctx_t *ctx,
                                          const uint32_t *value,
                                          uint32_t value_bits,
                                          int64_t modulus) {
    if (!ctx) return QC_ERR_NULL;
    if (!value) return QC_ERR_NULL;
    if (value_bits == 0 || value_bits > 64) return QC_ERR_WIDTH;
    if (modulus <= 0) return QC_ERR_DIVISOR;

    uint32_t n = value_bits;
    uint32_t wide = n + 1;

    /* Allocate temp (widened) and cmp ancilla */
    uint32_t temp_start;
    if (qc_qubit_alloc_n(ctx, wide, &temp_start) != QC_OK)
        return QC_ERR_ALLOC;

    uint32_t cmp_anc;
    if (qc_qubit_alloc(ctx, &cmp_anc) != QC_OK) {
        qc_qubit_free_n(ctx, temp_start, wide);
        return QC_ERR_ALLOC;
    }

    uint32_t temp[65];
    for (uint32_t i = 0; i < wide; i++) {
        temp[i] = temp_start + i;
    }

    /* Copy value to temp */
    for (uint32_t i = 0; i < n; i++) {
        qc_circuit_cx(ctx, value[i], temp[i]);
    }

    /* Subtract modulus from temp (widened) */
    qc_dynamic_cq_add(ctx, temp, wide, -modulus);

    /* Copy sign to cmp_anc */
    qc_circuit_cx(ctx, temp[n], cmp_anc);

    /* Uncompute temp: add modulus back, uncopy */
    qc_dynamic_cq_add(ctx, temp, wide, modulus);
    for (uint32_t i = 0; i < n; i++) {
        qc_circuit_cx(ctx, value[i], temp[i]);
    }

    qc_qubit_free_n(ctx, temp_start, wide);

    /* cmp_anc: 0 if value >= N, 1 if value < N.
     * Flip to subtract when value >= N. */
    qc_circuit_x(ctx, cmp_anc);
    qc_dynamic_ccq_add(ctx, value, n, -modulus, cmp_anc);
    qc_circuit_x(ctx, cmp_anc);

    /* Persistent ancilla -- cmp_anc not freed (Phase 91 limitation) */
    (void)cmp_anc;

    return QC_OK;
}

/* ====================================================================== */
/* Beauregard 8-step Modular CQ Addition                                   */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_mod_add_cq(circuit_ctx_t *ctx,
                                          const uint32_t *value,
                                          uint32_t value_bits,
                                          int64_t addend,
                                          int64_t modulus) {
    if (!ctx) return QC_ERR_NULL;
    if (!value) return QC_ERR_NULL;
    if (value_bits == 0 || value_bits > 64) return QC_ERR_WIDTH;
    if (modulus <= 0) return QC_ERR_DIVISOR;

    uint32_t n = value_bits;

    /* Reduce addend to [0, N-1] */
    int64_t a = addend % modulus;
    if (a < 0) a += modulus;
    if (a == 0) return QC_OK;

    /* Allocate high_bit and cmp_anc */
    uint32_t high_bit;
    if (qc_qubit_alloc(ctx, &high_bit) != QC_OK)
        return QC_ERR_ALLOC;

    uint32_t cmp_anc;
    if (qc_qubit_alloc(ctx, &cmp_anc) != QC_OK) {
        qc_qubit_free(ctx, high_bit);
        return QC_ERR_ALLOC;
    }

    /* Build (n+1)-bit wide register */
    uint32_t wide_reg[65];
    for (uint32_t i = 0; i < n; i++)
        wide_reg[i] = value[i];
    wide_reg[n] = high_bit;

    /* Step 1: value += a */
    qc_dynamic_cq_add(ctx, wide_reg, n + 1, a);

    /* Step 2: value -= N */
    qc_dynamic_cq_add(ctx, wide_reg, n + 1, -modulus);

    /* Step 3: copy sign to cmp_anc */
    qc_circuit_cx(ctx, wide_reg[n], cmp_anc);

    /* Step 4: if cmp_anc=1: value += N */
    qc_dynamic_ccq_add(ctx, wide_reg, n + 1, modulus, cmp_anc);

    /* Step 5: value -= a */
    qc_dynamic_cq_add(ctx, wide_reg, n + 1, -a);

    /* Step 6: X(cmp_anc) */
    qc_circuit_x(ctx, cmp_anc);

    /* Step 7: CNOT(cmp_anc, sign) -- resets cmp_anc to 0 */
    qc_circuit_cx(ctx, wide_reg[n], cmp_anc);

    /* Step 8: value += a */
    qc_dynamic_cq_add(ctx, wide_reg, n + 1, a);

    /* Both ancillae clean */
    qc_qubit_free(ctx, cmp_anc);
    qc_qubit_free(ctx, high_bit);

    return QC_OK;
}

/* ====================================================================== */
/* Controlled Beauregard CQ Modular Addition                               */
/* ====================================================================== */

static qc_error_t toffoli_cmod_add_cq_internal(circuit_ctx_t *ctx,
                                                 const uint32_t *value,
                                                 uint32_t value_bits,
                                                 int64_t addend,
                                                 int64_t modulus,
                                                 uint32_t ext_ctrl) {
    if (!ctx || !value) return QC_ERR_NULL;
    if (value_bits == 0 || value_bits > 64) return QC_ERR_WIDTH;
    if (modulus <= 0) return QC_ERR_DIVISOR;

    uint32_t n = value_bits;
    int64_t a = addend % modulus;
    if (a < 0) a += modulus;
    if (a == 0) return QC_OK;

    uint32_t high_bit;
    if (qc_qubit_alloc(ctx, &high_bit) != QC_OK) return QC_ERR_ALLOC;

    uint32_t cmp_anc;
    if (qc_qubit_alloc(ctx, &cmp_anc) != QC_OK) {
        qc_qubit_free(ctx, high_bit);
        return QC_ERR_ALLOC;
    }

    uint32_t wide_reg[65];
    for (uint32_t i = 0; i < n; i++)
        wide_reg[i] = value[i];
    wide_reg[n] = high_bit;

    /* Step 1: controlled value += a */
    qc_dynamic_ccq_add(ctx, wide_reg, n + 1, a, ext_ctrl);

    /* Step 2: controlled value -= N */
    qc_dynamic_ccq_add(ctx, wide_reg, n + 1, -modulus, ext_ctrl);

    /* Step 3: CCX(cmp_anc, high_bit, ext_ctrl) */
    qc_emit_ccx_or_decomp(ctx, cmp_anc, wide_reg[n], ext_ctrl);

    /* Step 4: doubly-controlled add N */
    uint32_t and_anc;
    if (qc_qubit_alloc(ctx, &and_anc) != QC_OK) {
        qc_qubit_free(ctx, cmp_anc);
        qc_qubit_free(ctx, high_bit);
        return QC_ERR_ALLOC;
    }
    qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl);
    qc_dynamic_ccq_add(ctx, wide_reg, n + 1, modulus, and_anc);
    qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl);
    qc_qubit_free(ctx, and_anc);

    /* Step 5: controlled value -= a */
    qc_dynamic_ccq_add(ctx, wide_reg, n + 1, -a, ext_ctrl);

    /* Step 6: CX(cmp_anc, ext_ctrl) */
    qc_circuit_cx(ctx, ext_ctrl, cmp_anc);

    /* Step 7: CCX(cmp_anc, high_bit, ext_ctrl) */
    qc_emit_ccx_or_decomp(ctx, cmp_anc, wide_reg[n], ext_ctrl);

    /* Step 8: controlled value += a */
    qc_dynamic_ccq_add(ctx, wide_reg, n + 1, a, ext_ctrl);

    qc_qubit_free(ctx, cmp_anc);
    qc_qubit_free(ctx, high_bit);

    return QC_OK;
}

/* ====================================================================== */
/* Beauregard QQ Modular Addition                                          */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_mod_add_qq(circuit_ctx_t *ctx,
                                          const uint32_t *value,
                                          uint32_t value_bits,
                                          const uint32_t *other,
                                          uint32_t other_bits,
                                          int64_t modulus) {
    if (!ctx || !value || !other) return QC_ERR_NULL;
    if (value_bits == 0 || value_bits > 64) return QC_ERR_WIDTH;
    if (modulus <= 0) return QC_ERR_DIVISOR;

    uint32_t n = value_bits;

    uint32_t high_bit;
    if (qc_qubit_alloc(ctx, &high_bit) != QC_OK) return QC_ERR_ALLOC;

    uint32_t cmp_anc;
    if (qc_qubit_alloc(ctx, &cmp_anc) != QC_OK) {
        qc_qubit_free(ctx, high_bit);
        return QC_ERR_ALLOC;
    }

    uint32_t wide_reg[65];
    for (uint32_t i = 0; i < n; i++)
        wide_reg[i] = value[i];
    wide_reg[n] = high_bit;

    /* Step 1: value += other (QQ add, (n+1)-bit target) */
    /* Pad source if needed */
    uint32_t pad_count = 0;
    uint32_t pad_start = 0;
    uint32_t src[65];
    for (uint32_t i = 0; i < other_bits && i < n + 1; i++) {
        src[i] = other[i];
    }
    if (other_bits < n + 1) {
        pad_count = n + 1 - other_bits;
        if (qc_qubit_alloc_n(ctx, pad_count, &pad_start) != QC_OK) {
            qc_qubit_free(ctx, cmp_anc);
            qc_qubit_free(ctx, high_bit);
            return QC_ERR_ALLOC;
        }
        for (uint32_t i = other_bits; i < n + 1; i++) {
            src[i] = pad_start + (i - other_bits);
        }
    }
    qc_dynamic_qq_add(ctx, src, wide_reg, n + 1);

    /* Step 2: value -= N */
    qc_dynamic_cq_add(ctx, wide_reg, n + 1, -modulus);

    /* Step 3: copy sign */
    qc_circuit_cx(ctx, wide_reg[n], cmp_anc);

    /* Step 4: conditional value += N */
    qc_dynamic_ccq_add(ctx, wide_reg, n + 1, modulus, cmp_anc);

    /* Step 5: value -= other (inverse QQ add) */
    qc_dynamic_qq_sub(ctx, src, wide_reg, n + 1);

    /* Step 6: X(cmp_anc) */
    qc_circuit_x(ctx, cmp_anc);

    /* Step 7: CNOT(cmp_anc, sign) */
    qc_circuit_cx(ctx, wide_reg[n], cmp_anc);

    /* Step 8: value += other */
    qc_dynamic_qq_add(ctx, src, wide_reg, n + 1);

    if (pad_count > 0) {
        qc_qubit_free_n(ctx, pad_start, pad_count);
    }

    qc_qubit_free(ctx, cmp_anc);
    qc_qubit_free(ctx, high_bit);

    return QC_OK;
}

/* ====================================================================== */
/* Modular CQ Multiplication                                               */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_mod_mul_cq(circuit_ctx_t *ctx,
                                          const uint32_t *value,
                                          uint32_t value_bits,
                                          const uint32_t *result,
                                          uint32_t result_bits,
                                          int64_t multiplier,
                                          int64_t modulus) {
    if (!ctx || !value || !result) return QC_ERR_NULL;
    if (value_bits == 0 || result_bits == 0) return QC_ERR_WIDTH;
    if (modulus <= 0) return QC_ERR_DIVISOR;

    uint32_t n = value_bits;

    int64_t c = multiplier % modulus;
    if (c < 0) c += modulus;
    if (c == 0) return QC_OK;

    /* Special case: multiply by 1 = copy */
    if (c == 1) {
        for (uint32_t i = 0; i < n && i < result_bits; i++) {
            qc_circuit_cx(ctx, value[i], result[i]);
        }
        return QC_OK;
    }

    /* For each bit j of value: controlled mod add of (c * 2^j mod N) */
    int64_t shifted = c;
    for (uint32_t j = 0; j < n; j++) {
        if (shifted != 0) {
            toffoli_cmod_add_cq_internal(ctx, result, result_bits,
                                          shifted, modulus, value[j]);
        }
        shifted = (shifted * 2) % modulus;
    }

    return QC_OK;
}
