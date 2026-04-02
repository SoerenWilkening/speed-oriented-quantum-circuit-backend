/**
 * @file toffoli_division.c
 * @brief Dynamic Toffoli-based restoring division for all widths.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/ToffoliDivision.c
 * Module: 1.11 (Phase 1)
 * Issue: refactor-tpz
 *
 * Implements restoring division producing quotient and remainder using
 * dynamically emitted CDKM adders.  All gates emitted directly into
 * circuit_ctx_t* ctx -- no pre-built sequences.
 *
 * Two public variants:
 *   qc_toffoli_divmod_cq   -- dividend / classical divisor
 *   qc_toffoli_divmod_qq   -- dividend / quantum divisor
 *   qc_toffoli_cdivmod_cq  -- controlled dividend / classical divisor
 *   qc_toffoli_cdivmod_qq  -- controlled dividend / quantum divisor
 *
 * Qubit arrays: LSB-first (index 0 = LSB, index n-1 = MSB).
 *
 * Algorithm (CQ): Restoring division with Bennett's trick for comparison.
 *   For each bit position k from (n-1) down to 0:
 *     1. Compute widened subtraction (remainder - trial) to get sign bit
 *     2. Copy sign bit to comparison ancilla (Bennett's trick)
 *     3. Uncompute the widened subtraction
 *     4. Conditionally subtract trial from remainder
 *     5. Set quotient bit k
 *     6. Uncompute comparison ancilla
 */

#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* CQ Division: dividend / classical divisor                               */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_divmod_cq(circuit_ctx_t *ctx,
                                         const uint32_t *dividend,
                                         uint32_t dividend_bits,
                                         int64_t divisor,
                                         const uint32_t *quotient,
                                         const uint32_t *remainder) {
    if (!ctx) return QC_ERR_NULL;
    if (!dividend || !quotient || !remainder) return QC_ERR_NULL;
    if (dividend_bits == 0 || dividend_bits > 64) return QC_ERR_WIDTH;

    uint32_t n = dividend_bits;

    /* Division by zero: set all quotient bits to 1, copy dividend to remainder */
    if (divisor == 0) {
        for (uint32_t i = 0; i < n; i++) {
            qc_circuit_x(ctx, quotient[i]);
        }
        for (uint32_t i = 0; i < n; i++) {
            qc_circuit_cx(ctx, dividend[i], remainder[i]);
        }
        return QC_ERR_DIVISOR;
    }

    /* Copy dividend to remainder via CX */
    for (uint32_t i = 0; i < n; i++) {
        qc_circuit_cx(ctx, dividend[i], remainder[i]);
    }

    /* Restoring division: for each bit position from MSB to LSB */
    for (int k = (int)n - 1; k >= 0; k--) {
        int64_t trial = divisor << k;

        /* Skip if trial exceeds the range of n bits */
        if (trial >= (1LL << (int64_t)n)) {
            continue;
        }

        uint32_t wide = n + 1;

        /* Allocate temp for widened comparison */
        uint32_t temp_start;
        if (qc_qubit_alloc_n(ctx, wide, &temp_start) != QC_OK)
            return QC_ERR_ALLOC;

        uint32_t cmp_anc;
        if (qc_qubit_alloc(ctx, &cmp_anc) != QC_OK) {
            qc_qubit_free_n(ctx, temp_start, wide);
            return QC_ERR_ALLOC;
        }

        uint32_t temp_arr[65];
        for (uint32_t i = 0; i < wide; i++) {
            temp_arr[i] = temp_start + i;
        }

        /* COMPUTE: copy remainder to temp, then widened subtract */
        for (uint32_t i = 0; i < n; i++) {
            qc_circuit_cx(ctx, remainder[i], temp_arr[i]);
        }

        /* Subtract trial: temp -= trial (in n+1 bits) */
        qc_dynamic_cq_add(ctx, temp_arr, wide, -trial);

        /* COPY: sign bit to cmp_anc (Bennett's trick) */
        qc_circuit_cx(ctx, temp_arr[n], cmp_anc);

        /* UNCOMPUTE: add trial back, uncopy */
        qc_dynamic_cq_add(ctx, temp_arr, wide, trial);
        for (uint32_t i = 0; i < n; i++) {
            qc_circuit_cx(ctx, remainder[i], temp_arr[i]);
        }

        qc_qubit_free_n(ctx, temp_start, wide);

        /* USE: cmp_anc = 1 if remainder < trial, 0 if remainder >= trial.
         * Flip to get positive-logic control. */
        qc_circuit_x(ctx, cmp_anc);

        /* Conditional subtract: remainder -= trial */
        qc_dynamic_ccq_add(ctx, remainder, n, -trial, cmp_anc);

        /* Set quotient bit */
        qc_circuit_cx(ctx, cmp_anc, quotient[k]);

        /* Unflip */
        qc_circuit_x(ctx, cmp_anc);

        /* RESET: uncompute cmp_anc.
         * cmp_anc = sign_bit = NOT(quotient[k]).
         * CNOT(cmp_anc, quotient[k]) -> cmp_anc = 1; then X -> 0. */
        qc_circuit_cx(ctx, quotient[k], cmp_anc);
        qc_circuit_x(ctx, cmp_anc);

        qc_qubit_free(ctx, cmp_anc);
    }

    return QC_OK;
}

/* ====================================================================== */
/* QQ Division: dividend / quantum divisor                                 */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_divmod_qq(circuit_ctx_t *ctx,
                                         const uint32_t *dividend,
                                         uint32_t dividend_bits,
                                         const uint32_t *divisor,
                                         uint32_t divisor_bits,
                                         const uint32_t *quotient,
                                         const uint32_t *remainder) {
    if (!ctx) return QC_ERR_NULL;
    if (!dividend || !divisor || !quotient || !remainder) return QC_ERR_NULL;
    if (dividend_bits == 0 || dividend_bits > 64) return QC_ERR_WIDTH;
    if (divisor_bits == 0 || divisor_bits > 64) return QC_ERR_WIDTH;

    uint32_t n = dividend_bits;

    /* Copy dividend to remainder via CX */
    for (uint32_t i = 0; i < n; i++) {
        qc_circuit_cx(ctx, dividend[i], remainder[i]);
    }

    /* Repeated subtraction: for each iteration 0..2^n-1 */
    uint32_t max_iterations = 1u << n;

    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        uint32_t wide = n + 1;

        /* Allocate temp for widened comparison */
        uint32_t temp_start;
        if (qc_qubit_alloc_n(ctx, wide, &temp_start) != QC_OK)
            return QC_ERR_ALLOC;

        uint32_t cmp_anc;
        if (qc_qubit_alloc(ctx, &cmp_anc) != QC_OK) {
            qc_qubit_free_n(ctx, temp_start, wide);
            return QC_ERR_ALLOC;
        }

        uint32_t temp_arr[65];
        for (uint32_t i = 0; i < wide; i++) {
            temp_arr[i] = temp_start + i;
        }

        /* Copy remainder to temp */
        for (uint32_t i = 0; i < n; i++) {
            qc_circuit_cx(ctx, remainder[i], temp_arr[i]);
        }

        /* Widened subtract: need a pad qubit for divisor's MSB+1 position */
        uint32_t div_pad;
        if (qc_qubit_alloc(ctx, &div_pad) != QC_OK) {
            for (uint32_t i = 0; i < n; i++) {
                qc_circuit_cx(ctx, remainder[i], temp_arr[i]);
            }
            qc_qubit_free(ctx, cmp_anc);
            qc_qubit_free_n(ctx, temp_start, wide);
            return QC_ERR_ALLOC;
        }

        /* Build widened divisor: divisor[0..n-1] + div_pad */
        uint32_t wide_div[65];
        for (uint32_t i = 0; i < n; i++) {
            wide_div[i] = divisor[i];
        }
        wide_div[n] = div_pad;

        /* QQ subtract: temp -= widened_divisor (n+1 bits) */
        qc_dynamic_qq_sub(ctx, wide_div, temp_arr, wide);

        /* Copy sign bit to cmp_anc */
        qc_circuit_cx(ctx, temp_arr[n], cmp_anc);

        /* Uncompute: add divisor back, uncopy */
        qc_dynamic_qq_add(ctx, wide_div, temp_arr, wide);
        qc_qubit_free(ctx, div_pad);

        for (uint32_t i = 0; i < n; i++) {
            qc_circuit_cx(ctx, remainder[i], temp_arr[i]);
        }

        qc_qubit_free_n(ctx, temp_start, wide);

        /* USE: flip cmp_anc for positive-logic */
        qc_circuit_x(ctx, cmp_anc);

        /* Conditional subtract: remainder -= divisor */
        qc_dynamic_cqq_sub(ctx, divisor, remainder, n, cmp_anc);

        /* Increment quotient by 1, controlled */
        qc_dynamic_ccq_add(ctx, quotient, n, 1, cmp_anc);

        /* Unflip */
        qc_circuit_x(ctx, cmp_anc);

        /* Note: cmp_anc cannot be cleanly uncomputed in general for QQ division.
         * This is a known limitation -- each iteration leaks one comparison ancilla.
         * For small widths this is acceptable. */
        (void)cmp_anc; /* Persistent ancilla -- not freed */
    }

    return QC_OK;
}

/* ====================================================================== */
/* Controlled CQ Division: dividend / classical divisor, ext_ctrl         */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_cdivmod_cq(circuit_ctx_t *ctx,
                                          const uint32_t *dividend,
                                          uint32_t dividend_bits,
                                          int64_t divisor,
                                          const uint32_t *quotient,
                                          const uint32_t *remainder,
                                          uint32_t ext_ctrl) {
    if (!ctx) return QC_ERR_NULL;
    if (!dividend || !quotient || !remainder) return QC_ERR_NULL;
    if (dividend_bits == 0 || dividend_bits > 64) return QC_ERR_WIDTH;

    uint32_t n = dividend_bits;

    /* Division by zero: controlled sentinel */
    if (divisor == 0) {
        for (uint32_t i = 0; i < n; i++) {
            qc_circuit_cx(ctx, ext_ctrl, quotient[i]);
        }
        for (uint32_t i = 0; i < n; i++) {
            qc_emit_ccx_or_decomp(ctx, remainder[i], dividend[i], ext_ctrl);
        }
        return QC_ERR_DIVISOR;
    }

    /* Controlled copy dividend to remainder via CCX */
    for (uint32_t i = 0; i < n; i++) {
        qc_emit_ccx_or_decomp(ctx, remainder[i], dividend[i], ext_ctrl);
    }

    /* Restoring division: for each bit position from MSB to LSB */
    for (int k = (int)n - 1; k >= 0; k--) {
        int64_t trial = divisor << k;

        /* Skip if trial exceeds the range of n bits */
        if (trial >= (1LL << (int64_t)n)) {
            continue;
        }

        uint32_t wide = n + 1;

        /* Allocate temp for widened comparison */
        uint32_t temp_start;
        if (qc_qubit_alloc_n(ctx, wide, &temp_start) != QC_OK)
            return QC_ERR_ALLOC;

        uint32_t cmp_anc;
        if (qc_qubit_alloc(ctx, &cmp_anc) != QC_OK) {
            qc_qubit_free_n(ctx, temp_start, wide);
            return QC_ERR_ALLOC;
        }

        uint32_t temp_arr[65];
        for (uint32_t i = 0; i < wide; i++) {
            temp_arr[i] = temp_start + i;
        }

        /* COMPUTE: controlled copy remainder -> temp */
        for (uint32_t i = 0; i < n; i++) {
            qc_emit_ccx_or_decomp(ctx, temp_arr[i], remainder[i], ext_ctrl);
        }

        /* Controlled subtract trial: temp -= trial (in n+1 bits) */
        qc_dynamic_ccq_add(ctx, temp_arr, wide, -trial, ext_ctrl);

        /* COPY: sign bit to cmp_anc (unconditional -- already conditioned
         * on ext_ctrl because the comparison was controlled) */
        qc_circuit_cx(ctx, temp_arr[n], cmp_anc);

        /* UNCOMPUTE: controlled add trial back, uncopy */
        qc_dynamic_ccq_add(ctx, temp_arr, wide, trial, ext_ctrl);
        for (uint32_t i = 0; i < n; i++) {
            qc_emit_ccx_or_decomp(ctx, temp_arr[i], remainder[i], ext_ctrl);
        }

        qc_qubit_free_n(ctx, temp_start, wide);

        /* USE: flip cmp_anc for positive-logic */
        qc_circuit_x(ctx, cmp_anc);

        /* Doubly-controlled subtract: remainder -= trial,
         * controlled by cmp_anc AND ext_ctrl.
         * Use AND-ancilla pattern. */
        uint32_t and_anc;
        if (qc_qubit_alloc(ctx, &and_anc) != QC_OK) {
            qc_qubit_free(ctx, cmp_anc);
            return QC_ERR_ALLOC;
        }

        qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl);
        qc_dynamic_ccq_add(ctx, remainder, n, -trial, and_anc);
        qc_circuit_cx(ctx, and_anc, quotient[k]);
        qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl); /* uncompute AND */

        qc_qubit_free(ctx, and_anc);

        /* Unflip */
        qc_circuit_x(ctx, cmp_anc);

        /* RESET: uncompute cmp_anc -- must be controlled by ext_ctrl
         * to handle ext_ctrl=0 correctly (otherwise cmp_anc leaks at 1). */
        qc_emit_ccx_or_decomp(ctx, cmp_anc, quotient[k], ext_ctrl);
        qc_circuit_cx(ctx, ext_ctrl, cmp_anc);

        qc_qubit_free(ctx, cmp_anc);
    }

    return QC_OK;
}

/* ====================================================================== */
/* Controlled QQ Division: dividend / quantum divisor, ext_ctrl            */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_cdivmod_qq(circuit_ctx_t *ctx,
                                          const uint32_t *dividend,
                                          uint32_t dividend_bits,
                                          const uint32_t *divisor,
                                          uint32_t divisor_bits,
                                          const uint32_t *quotient,
                                          const uint32_t *remainder,
                                          uint32_t ext_ctrl) {
    if (!ctx) return QC_ERR_NULL;
    if (!dividend || !divisor || !quotient || !remainder) return QC_ERR_NULL;
    if (dividend_bits == 0 || dividend_bits > 64) return QC_ERR_WIDTH;
    if (divisor_bits == 0 || divisor_bits > 64) return QC_ERR_WIDTH;

    uint32_t n = dividend_bits;

    /* Controlled copy dividend to remainder via CCX */
    for (uint32_t i = 0; i < n; i++) {
        qc_emit_ccx_or_decomp(ctx, remainder[i], dividend[i], ext_ctrl);
    }

    /* Repeated subtraction: for each iteration 0..2^n-1 */
    uint32_t max_iterations = 1u << n;

    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        uint32_t wide = n + 1;

        /* Allocate temp for widened comparison */
        uint32_t temp_start;
        if (qc_qubit_alloc_n(ctx, wide, &temp_start) != QC_OK)
            return QC_ERR_ALLOC;

        uint32_t cmp_anc;
        if (qc_qubit_alloc(ctx, &cmp_anc) != QC_OK) {
            qc_qubit_free_n(ctx, temp_start, wide);
            return QC_ERR_ALLOC;
        }

        uint32_t temp_arr[65];
        for (uint32_t i = 0; i < wide; i++) {
            temp_arr[i] = temp_start + i;
        }

        /* Controlled copy remainder to temp */
        for (uint32_t i = 0; i < n; i++) {
            qc_emit_ccx_or_decomp(ctx, temp_arr[i], remainder[i], ext_ctrl);
        }

        /* Widened comparison: pad divisor for MSB+1 position */
        uint32_t div_pad;
        if (qc_qubit_alloc(ctx, &div_pad) != QC_OK) {
            for (uint32_t i = 0; i < n; i++) {
                qc_emit_ccx_or_decomp(ctx, temp_arr[i], remainder[i], ext_ctrl);
            }
            qc_qubit_free(ctx, cmp_anc);
            qc_qubit_free_n(ctx, temp_start, wide);
            return QC_ERR_ALLOC;
        }

        /* Build widened divisor: divisor[0..n-1] + div_pad */
        uint32_t wide_div[65];
        for (uint32_t i = 0; i < n; i++) {
            wide_div[i] = divisor[i];
        }
        wide_div[n] = div_pad;

        /* Controlled QQ subtract: temp -= widened_divisor */
        qc_dynamic_cqq_sub(ctx, wide_div, temp_arr, wide, ext_ctrl);

        /* Copy sign bit to cmp_anc */
        qc_circuit_cx(ctx, temp_arr[n], cmp_anc);

        /* Uncompute: controlled add divisor back, uncopy */
        qc_dynamic_cqq_add(ctx, wide_div, temp_arr, wide, ext_ctrl);
        qc_qubit_free(ctx, div_pad);

        for (uint32_t i = 0; i < n; i++) {
            qc_emit_ccx_or_decomp(ctx, temp_arr[i], remainder[i], ext_ctrl);
        }

        qc_qubit_free_n(ctx, temp_start, wide);

        /* USE: flip cmp_anc for positive-logic */
        qc_circuit_x(ctx, cmp_anc);

        /* Doubly-controlled subtract and increment:
         * AND-ancilla for cmp_anc AND ext_ctrl */
        uint32_t and_anc;
        if (qc_qubit_alloc(ctx, &and_anc) != QC_OK) {
            (void)cmp_anc;
            return QC_ERR_ALLOC;
        }

        qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl);
        qc_dynamic_cqq_sub(ctx, divisor, remainder, n, and_anc);
        qc_dynamic_ccq_add(ctx, quotient, n, 1, and_anc);
        qc_emit_ccx_or_decomp(ctx, and_anc, cmp_anc, ext_ctrl); /* uncompute AND */

        qc_qubit_free(ctx, and_anc);
        qc_circuit_x(ctx, cmp_anc);

        /* cmp_anc leaks for QQ (same as uncontrolled) */
        (void)cmp_anc;
    }

    return QC_OK;
}
