/**
 * @file toffoli_mod_extras.c
 * @brief Extended Toffoli-based modular arithmetic primitives for qint_mod.
 *
 * Issue: refactor-m38p (Step 0 of PLAN_qint_mod_primitives.md)
 *
 * Sibling of toffoli_mod_reduce.c. Created in Step 0 as motivated
 * preparation: subsequent steps (notably Step 4 cmod_add_qq and Step 6
 * mod_mul_qq) would push toffoli_mod_reduce.c past the 500-line cap if
 * appended in place. Splitting now lets each later step pick the
 * correct file without a mid-stream relocation.
 *
 * The bodies in this file are temporary stubs returning
 * QC_ERR_INVALID_OP. Subsequent steps replace each stub with the real
 * Beauregard construction; see PRD_qint_mod_primitives.md §4 and
 * PLAN_qint_mod_primitives.md Steps 1-7.
 *
 * internal.h is included now (not in a later step) because Steps 4
 * onward require qc_dynamic_qq_add / qq_sub / cqq_add / cqq_sub /
 * cq_add / ccq_add helpers from it.
 */

#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Stubs — to be replaced by Steps 1-7 of PLAN_qint_mod_primitives.md     */
/* ====================================================================== */

QC_API qc_error_t qc_toffoli_cmod_add_cq(circuit_ctx_t *ctx,
                                           const uint32_t *value,
                                           uint32_t value_bits,
                                           int64_t addend,
                                           int64_t modulus,
                                           uint32_t ext_ctrl) {
    /* Step 1 (refactor-xf0n): thin public wrapper around the existing
     * controlled Beauregard core. Validation mirrors qc_toffoli_mod_add_cq
     * in toffoli_mod_reduce.c; the internal helper performs the same checks
     * defensively, but we surface the same error codes here so callers see
     * a consistent contract whether they call the controlled or
     * uncontrolled variant.
     */
    if (!ctx) return QC_ERR_NULL;
    if (!value) return QC_ERR_NULL;
    if (value_bits == 0 || value_bits > 64) return QC_ERR_WIDTH;
    if (modulus <= 0) return QC_ERR_DIVISOR;

    return toffoli_cmod_add_cq_internal(ctx, value, value_bits,
                                        addend, modulus, ext_ctrl);
}

QC_API qc_error_t qc_toffoli_cmod_mul_cq(circuit_ctx_t *ctx,
                                           const uint32_t *value,
                                           uint32_t value_bits,
                                           const uint32_t *result,
                                           uint32_t result_bits,
                                           int64_t multiplier,
                                           int64_t modulus,
                                           uint32_t ext_ctrl) {
    /* Step 2 (refactor-haai): Beauregard controlled modular multiplier.
     *
     * Mirrors qc_toffoli_mod_mul_cq in toffoli_mod_reduce.c, but every
     * operation that touches the result register is doubly controlled on
     * (ext_ctrl, value[j]) by AND-ing them into a fresh ancilla via
     * Toffoli, calling toffoli_cmod_add_cq_internal with that ancilla as
     * its single control, then uncomputing the Toffoli to return the
     * ancilla to |0>. The c==1 special case must use ccx, NOT bare cx,
     * to honour the external control even when the multiplier reduces
     * to 1 mod N.
     */
    if (!ctx || !value || !result) return QC_ERR_NULL;
    if (value_bits == 0 || result_bits == 0) return QC_ERR_WIDTH;
    if (value_bits > 64 || result_bits > 64) return QC_ERR_WIDTH;
    if (modulus <= 0) return QC_ERR_DIVISOR;

    uint32_t n = value_bits;

    int64_t c = multiplier % modulus;
    if (c < 0) c += modulus;
    if (c == 0) return QC_OK;

    /* Special case: multiply by 1 = controlled copy. MUST be ccx, not cx,
     * because the external control still has to gate the operation. */
    if (c == 1) {
        for (uint32_t i = 0; i < n && i < result_bits; i++) {
            qc_circuit_ccx(ctx, ext_ctrl, value[i], result[i]);
        }
        return QC_OK;
    }

    /* General case: doubly-controlled cmod_add ladder. */
    int64_t shifted = c;
    for (uint32_t j = 0; j < n; j++) {
        if (shifted != 0) {
            uint32_t and_anc;
            qc_error_t aerr = qc_qubit_alloc(ctx, &and_anc);
            if (aerr != QC_OK) return aerr;

            /* AND ext_ctrl & value[j] -> and_anc */
            qc_circuit_ccx(ctx, ext_ctrl, value[j], and_anc);

            qc_error_t err = toffoli_cmod_add_cq_internal(
                ctx, result, result_bits, shifted, modulus, and_anc);
            if (err != QC_OK) {
                /* Best-effort: uncompute and free before returning. */
                qc_circuit_ccx(ctx, ext_ctrl, value[j], and_anc);
                qc_qubit_free(ctx, and_anc);
                return err;
            }

            /* Uncompute the AND so the ancilla returns to |0>. */
            qc_circuit_ccx(ctx, ext_ctrl, value[j], and_anc);
            qc_qubit_free(ctx, and_anc);
        }
        shifted = (shifted * 2) % modulus;
    }

    return QC_OK;
}

QC_API qc_error_t qc_toffoli_mod_sub_cq(circuit_ctx_t *ctx,
                                          const uint32_t *value,
                                          uint32_t value_bits,
                                          int64_t subtrahend,
                                          int64_t modulus) {
    /* Step 3a (refactor-y2cb): classical-subtrahend adapter.
     *
     * Because the subtrahend is a compile-time constant, modular subtraction
     * reduces exactly to modular addition by the additive inverse:
     *     value -= s (mod N)  ==  value += ((N - (s mod N)) mod N) (mod N)
     *
     * Reducing s classically first means this construction is safe for any
     * positive modulus (no two's-complement underflow, no power-of-two
     * assumption). Negative subtrahends are normalised into [0, N) by the
     * C99 % operator plus a fix-up.
     */
    if (!ctx) return QC_ERR_NULL;
    if (!value) return QC_ERR_NULL;
    if (value_bits == 0 || value_bits > 64) return QC_ERR_WIDTH;
    if (modulus < 2) return QC_ERR_DIVISOR;

    int64_t reduced = subtrahend % modulus;
    if (reduced < 0) reduced += modulus;
    int64_t negated = (modulus - reduced) % modulus;

    return qc_toffoli_mod_add_cq(ctx, value, value_bits, negated, modulus);
}

QC_API qc_error_t qc_toffoli_mod_sub_qq(circuit_ctx_t *ctx,
                                          const uint32_t *value,
                                          uint32_t value_bits,
                                          const uint32_t *other,
                                          uint32_t other_bits,
                                          int64_t modulus) {
    /* Step 3b (refactor-7awn): qc_toffoli_mod_sub_qq as the unitary
     * inverse (dagger) of qc_toffoli_mod_add_qq.
     *
     * The Beauregard 8-step block in qc_toffoli_mod_add_qq is run
     * backwards here, with each gate replaced by its inverse:
     *   - CX, X are self-inverse
     *   - qq_add <-> qq_sub
     *   - cq_add(-N) <-> cq_add(+N)
     *   - ccq_add(+N, cmp_anc) <-> ccq_add(-N, cmp_anc)
     *
     * The cmp_anc and high_bit ancilla allocation/free positions are
     * invariant under daggering: both ancillae must begin and end in
     * |0>, so they bracket the whole block exactly as in mod_add_qq.
     *
     * TWO'S-COMPLEMENT IS FORBIDDEN (PRD §4.2): computing
     * value += (2^n - other) mod 2^n does NOT equal (value - other) mod N
     * for non-power-of-two N. The dagger construction avoids this trap.
     */
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

    /* Pad source if needed (identical to mod_add_qq). */
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

    /* Dagger of mod_add_qq: steps 8..1 inverted. */

    /* Inverse of step 8 (value += other)  ==>  value -= other */
    qc_dynamic_qq_sub(ctx, src, wide_reg, n + 1);

    /* Inverse of step 7 (CX(sign, cmp_anc)) — self-inverse */
    qc_circuit_cx(ctx, wide_reg[n], cmp_anc);

    /* Inverse of step 6 (X(cmp_anc)) — self-inverse */
    qc_circuit_x(ctx, cmp_anc);

    /* Inverse of step 5 (value -= other)  ==>  value += other */
    qc_dynamic_qq_add(ctx, src, wide_reg, n + 1);

    /* Inverse of step 4 (conditional value += N)  ==>  conditional value -= N */
    qc_dynamic_ccq_add(ctx, wide_reg, n + 1, -modulus, cmp_anc);

    /* Inverse of step 3 (CX(sign, cmp_anc)) — self-inverse */
    qc_circuit_cx(ctx, wide_reg[n], cmp_anc);

    /* Inverse of step 2 (value -= N)  ==>  value += N */
    qc_dynamic_cq_add(ctx, wide_reg, n + 1, modulus);

    /* Inverse of step 1 (value += other)  ==>  value -= other */
    qc_dynamic_qq_sub(ctx, src, wide_reg, n + 1);

    if (pad_count > 0) {
        qc_qubit_free_n(ctx, pad_start, pad_count);
    }

    qc_qubit_free(ctx, cmp_anc);
    qc_qubit_free(ctx, high_bit);

    return QC_OK;
}

QC_API qc_error_t qc_toffoli_cmod_add_qq(circuit_ctx_t *ctx,
                                           const uint32_t *value,
                                           uint32_t value_bits,
                                           const uint32_t *other,
                                           uint32_t other_bits,
                                           int64_t modulus,
                                           uint32_t ext_ctrl) {
    (void)ctx; (void)value; (void)value_bits;
    (void)other; (void)other_bits;
    (void)modulus; (void)ext_ctrl;
    return QC_ERR_INVALID_OP;
}

QC_API qc_error_t qc_toffoli_cmod_sub_cq(circuit_ctx_t *ctx,
                                           const uint32_t *value,
                                           uint32_t value_bits,
                                           int64_t subtrahend,
                                           int64_t modulus,
                                           uint32_t ext_ctrl) {
    (void)ctx; (void)value; (void)value_bits;
    (void)subtrahend; (void)modulus; (void)ext_ctrl;
    return QC_ERR_INVALID_OP;
}

QC_API qc_error_t qc_toffoli_cmod_sub_qq(circuit_ctx_t *ctx,
                                           const uint32_t *value,
                                           uint32_t value_bits,
                                           const uint32_t *other,
                                           uint32_t other_bits,
                                           int64_t modulus,
                                           uint32_t ext_ctrl) {
    (void)ctx; (void)value; (void)value_bits;
    (void)other; (void)other_bits;
    (void)modulus; (void)ext_ctrl;
    return QC_ERR_INVALID_OP;
}

QC_API qc_error_t qc_toffoli_mod_mul_qq(circuit_ctx_t *ctx,
                                          const uint32_t *a,
                                          uint32_t a_bits,
                                          const uint32_t *b,
                                          uint32_t b_bits,
                                          const uint32_t *result,
                                          uint32_t result_bits,
                                          int64_t modulus) {
    (void)ctx; (void)a; (void)a_bits;
    (void)b; (void)b_bits;
    (void)result; (void)result_bits; (void)modulus;
    return QC_ERR_INVALID_OP;
}

QC_API qc_error_t qc_toffoli_cmod_mul_qq(circuit_ctx_t *ctx,
                                           const uint32_t *a,
                                           uint32_t a_bits,
                                           const uint32_t *b,
                                           uint32_t b_bits,
                                           const uint32_t *result,
                                           uint32_t result_bits,
                                           int64_t modulus,
                                           uint32_t ext_ctrl) {
    (void)ctx; (void)a; (void)a_bits;
    (void)b; (void)b_bits;
    (void)result; (void)result_bits;
    (void)modulus; (void)ext_ctrl;
    return QC_ERR_INVALID_OP;
}
