/**
 * @file toffoli_multiplication.c
 * @brief Dynamic Toffoli-based multiplication for all widths.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/ToffoliMultiplication.c
 * Module: 1.11 (Phase 1)
 * Issue: refactor-tpz
 *
 * Implements shift-and-add (schoolbook) multiplication using dynamically
 * emitted CDKM ripple-carry adders as subroutines.  All gates are emitted
 * directly into circuit_ctx_t* ctx -- no pre-built sequences, no global
 * state.
 *
 * CDKM adder helpers (qc_dynamic_qq_add, qc_dynamic_cqq_add, etc.) live
 * in toffoli_cdkm_adder.c.
 *
 * Two public variants:
 *   qc_toffoli_qq_mul   -- quantum * quantum
 *   qc_toffoli_cq_mul   -- quantum * classical
 *
 * Qubit arrays: LSB-first (index 0 = LSB, index n-1 = MSB).
 *
 * References:
 *   Schoolbook multiplication: sum_{j=0}^{n-1} a * b[j] * 2^j
 *   CDKM adder: Cuccaro et al., arXiv:quant-ph/0410184
 *   AND-ancilla decomposition: Beauregard (2003), Haner et al. (2018)
 */

#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* QQ Multiplication: result = a * b (schoolbook, shift-and-add)           */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_qq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                      uint32_t result_bits, const uint32_t *a,
                                      uint32_t a_bits, const uint32_t *b,
                                      uint32_t b_bits) {
    if (!ctx) return QC_ERR_NULL;
    if (!result || !a || !b) return QC_ERR_NULL;
    if (result_bits == 0 || a_bits == 0 || b_bits == 0) return QC_ERR_WIDTH;

    uint32_t n = result_bits;

    for (uint32_t j = 0; j < n; j++) {
        uint32_t add_width = n - j;
        if (add_width > a_bits)
            add_width = a_bits;
        if (add_width < 1) break;
        if (j >= b_bits) break; /* No more multiplier bits */

        if (add_width == 1) {
            /* 1-bit controlled add: CCX(result[j], a[0], b[j]) */
            qc_emit_ccx_or_decomp(ctx, result[j], a[0], b[j]);
        } else {
            /* Controlled addition of a[0..add_width-1] into
             * result[j..j+add_width-1], controlled by b[j]. */
            qc_dynamic_cqq_add(ctx, a, &result[j], add_width, b[j]);
        }
    }

    return QC_OK;
}

/* ====================================================================== */
/* CQ Multiplication: result = a * classical_value                         */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_cq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                      uint32_t result_bits, const uint32_t *target,
                                      uint32_t target_bits, int64_t value) {
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

        /* add_width is min(remaining result bits, target_bits) to avoid
         * reading past the end of the target array */
        uint32_t add_width = n - j;
        if (add_width > target_bits)
            add_width = target_bits;
        if (add_width < 1) break;

        if (add_width == 1) {
            /* 1-bit uncontrolled addition: CX */
            qc_circuit_cx(ctx, target[0], result[j]);
        } else {
            /* Uncontrolled addition of target[0..add_width-1]
             * into result[j..j+add_width-1] */
            qc_dynamic_qq_add(ctx, target, &result[j], add_width);
        }
    }

    free(bin);
    return QC_OK;
}
