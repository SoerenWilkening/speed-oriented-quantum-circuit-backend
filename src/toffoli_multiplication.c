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
 * Also provides inline CDKM adder helpers (qc_dynamic_qq_add, etc.) used
 * by multiplication, division, and modular reduction modules.
 *
 * Four public variants:
 *   qc_toffoli_qq_mul   -- quantum * quantum
 *   qc_toffoli_cq_mul   -- quantum * classical
 *   (controlled variants are internal, called by division/mod_reduce)
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
/* CCX emission helper (respects toffoli_decompose flag)                    */
/* ====================================================================== */

/**
 * @brief Emit a CCX gate, or its 15-gate Clifford+T decomposition.
 *
 * When ctx->toffoli_decompose is set, emits:
 *   H T Tdg CX CX T Tdg CX CX T T H T CX T Tdg
 * (standard Clifford+T decomposition of Toffoli).
 * Otherwise emits a single CCX gate.
 */
void qc_emit_ccx_or_decomp(circuit_ctx_t *ctx, uint32_t target,
                            uint32_t ctrl1, uint32_t ctrl2) {
    if (ctx->toffoli_decompose) {
        /* Standard 15-gate Clifford+T decomposition of Toffoli (CCX).
         * T-count = 7 (4 T + 3 Tdg). */
        qc_circuit_h(ctx, target);
        qc_circuit_t_gate(ctx, target);
        qc_circuit_cx(ctx, ctrl2, target);
        qc_circuit_tdg(ctx, target);
        qc_circuit_cx(ctx, ctrl1, target);
        qc_circuit_t_gate(ctx, target);
        qc_circuit_cx(ctx, ctrl2, target);
        qc_circuit_tdg(ctx, target);
        qc_circuit_h(ctx, target);
        qc_circuit_t_gate(ctx, ctrl2);
        qc_circuit_cx(ctx, ctrl1, ctrl2);
        qc_circuit_t_gate(ctx, ctrl1);
        qc_circuit_tdg(ctx, ctrl2);
        qc_circuit_cx(ctx, ctrl1, ctrl2);
    } else {
        qc_circuit_ccx(ctx, ctrl1, ctrl2, target);
    }
}

/* ====================================================================== */
/* Dynamic CDKM adder: MAJ / UMA primitives                                */
/* ====================================================================== */

/**
 * @brief MAJ gate triple: propagates carry.
 *   CX(c, b); CX(c, a); CCX(a, b, c)
 * After: a retains original, b = b XOR c_in, c = carry_out.
 */
static void emit_maj(circuit_ctx_t *ctx, uint32_t a, uint32_t b, uint32_t c) {
    qc_circuit_cx(ctx, c, b);
    qc_circuit_cx(ctx, c, a);
    qc_emit_ccx_or_decomp(ctx, c, a, b);
}

/**
 * @brief UMA gate triple: extracts sum and restores carry.
 *   CCX(a, b, c); CX(c, a); CX(a, b)
 */
static void emit_uma(circuit_ctx_t *ctx, uint32_t a, uint32_t b, uint32_t c) {
    qc_emit_ccx_or_decomp(ctx, c, a, b);
    qc_circuit_cx(ctx, c, a);
    qc_circuit_cx(ctx, a, b);
}

/* ====================================================================== */
/* Dynamic CDKM QQ addition: b += a                                        */
/* ====================================================================== */

/**
 * @brief CDKM ripple-carry adder: b += a.
 *
 * Allocates one carry ancilla internally.
 * For width == 1: emits a single CX.
 *
 * @param ctx   Circuit context.
 * @param a     Source register (preserved), length = width.
 * @param b     Target register (modified: b += a), length = width.
 * @param width Bit width (>= 1).
 */
void qc_dynamic_qq_add(circuit_ctx_t *ctx, const uint32_t *a,
                        const uint32_t *b, uint32_t width) {
    if (width == 0 || !ctx || !a || !b) return;
    if (width == 1) {
        qc_circuit_cx(ctx, a[0], b[0]);
        return;
    }

    uint32_t carry;
    if (qc_qubit_alloc(ctx, &carry) != QC_OK) return;

    /* Forward MAJ sweep */
    emit_maj(ctx, carry, b[0], a[0]);
    for (uint32_t i = 1; i < width; i++) {
        emit_maj(ctx, a[i - 1], b[i], a[i]);
    }

    /* Reverse UMA sweep */
    for (int i = (int)width - 1; i >= 1; i--) {
        emit_uma(ctx, a[i - 1], b[i], a[i]);
    }
    emit_uma(ctx, carry, b[0], a[0]);

    qc_qubit_free(ctx, carry);
}

/**
 * @brief CDKM ripple-carry subtraction: b -= a (inverse of addition).
 *
 * Runs the CDKM adder in reverse gate order.
 */
void qc_dynamic_qq_sub(circuit_ctx_t *ctx, const uint32_t *a,
                        const uint32_t *b, uint32_t width) {
    if (width == 0 || !ctx || !a || !b) return;
    if (width == 1) {
        qc_circuit_cx(ctx, a[0], b[0]);
        return;
    }

    uint32_t carry;
    if (qc_qubit_alloc(ctx, &carry) != QC_OK) return;

    /* Inverse UMA sweep (forward) */
    /* UMA inverse: CX(a,b); CX(c,a); CCX(a,b,c) */
    /* = exactly the MAJ sequence with (a,b,c) -> reverse */
    /* Actually the inverse of MAJ-then-UMA is UMA^-1 then MAJ^-1 */
    /* Simpler: emit the same gates in reverse order. */

    /* Reverse of:
     *   MAJ(carry, b[0], a[0])
     *   MAJ(a[0], b[1], a[1]) ... MAJ(a[n-2], b[n-1], a[n-1])
     *   UMA(a[n-2], b[n-1], a[n-1]) ... UMA(a[0], b[1], a[1])
     *   UMA(carry, b[0], a[0])
     *
     * Inverse = reverse order with each gate self-inverse:
     *   inverse-UMA(carry, b[0], a[0])
     *   inverse-UMA(a[0], b[1], a[1]) ... inverse-UMA(a[n-2], b[n-1], a[n-1])
     *   inverse-MAJ(a[n-2], b[n-1], a[n-1]) ... inverse-MAJ(a[0], b[1], a[1])
     *   inverse-MAJ(carry, b[0], a[0])
     *
     * Since MAJ = CX;CX;CCX and UMA = CCX;CX;CX, and each gate is self-inverse:
     * inverse-MAJ = CCX;CX;CX (= UMA!) and inverse-UMA = CX;CX;CCX (= MAJ!)
     *
     * So subtraction = UMA sweep forward, then MAJ sweep backward. */

    /* Forward UMA sweep (acts as inverse-MAJ) */
    emit_uma(ctx, carry, b[0], a[0]);
    for (uint32_t i = 1; i < width; i++) {
        emit_uma(ctx, a[i - 1], b[i], a[i]);
    }

    /* Reverse MAJ sweep (acts as inverse-UMA) */
    for (int i = (int)width - 1; i >= 1; i--) {
        emit_maj(ctx, a[i - 1], b[i], a[i]);
    }
    emit_maj(ctx, carry, b[0], a[0]);

    qc_qubit_free(ctx, carry);
}

/* ====================================================================== */
/* Dynamic CQ addition: target += classical value                          */
/* ====================================================================== */

void qc_dynamic_cq_add(circuit_ctx_t *ctx, const uint32_t *target,
                        uint32_t width, int64_t value) {
    if (width == 0 || !ctx || !target) return;

    int *bin = qc_two_complement(value, (int)width);
    if (!bin) return;

    if (width == 1) {
        /* 1-bit: X if LSB is set */
        if (bin[0] & 1) {
            qc_circuit_x(ctx, target[0]);
        }
        free(bin);
        return;
    }

    /* Allocate temp register + build it classically via X gates */
    uint32_t temp_start;
    if (qc_qubit_alloc_n(ctx, width, &temp_start) != QC_OK) {
        free(bin);
        return;
    }
    uint32_t temp[64];
    for (uint32_t i = 0; i < width; i++) {
        temp[i] = temp_start + i;
    }

    /* Init temp to classical value (MSB-first bin, but we need LSB-first qubits) */
    for (uint32_t i = 0; i < width; i++) {
        if (bin[width - 1 - i]) {
            qc_circuit_x(ctx, temp[i]);
        }
    }

    /* QQ add: target += temp */
    qc_dynamic_qq_add(ctx, temp, target, width);

    /* Uncompute temp */
    for (uint32_t i = 0; i < width; i++) {
        if (bin[width - 1 - i]) {
            qc_circuit_x(ctx, temp[i]);
        }
    }

    qc_qubit_free_n(ctx, temp_start, width);
    free(bin);
}

/* ====================================================================== */
/* Controlled CDKM addition helpers                                        */
/* ====================================================================== */

/** @brief Controlled MAJ using AND-ancilla decomposition. */
static void emit_cmaj(circuit_ctx_t *ctx, uint32_t a, uint32_t b,
                       uint32_t c, uint32_t ext_ctrl, uint32_t and_anc) {
    qc_emit_ccx_or_decomp(ctx, b, c, ext_ctrl);
    qc_emit_ccx_or_decomp(ctx, a, c, ext_ctrl);
    /* MCX(c, [a,b,ext]) decomposed via AND-ancilla */
    qc_emit_ccx_or_decomp(ctx, and_anc, a, b);
    qc_emit_ccx_or_decomp(ctx, c, and_anc, ext_ctrl);
    qc_emit_ccx_or_decomp(ctx, and_anc, a, b);
}

/** @brief Controlled UMA using AND-ancilla decomposition. */
static void emit_cuma(circuit_ctx_t *ctx, uint32_t a, uint32_t b,
                       uint32_t c, uint32_t ext_ctrl, uint32_t and_anc) {
    qc_emit_ccx_or_decomp(ctx, and_anc, a, b);
    qc_emit_ccx_or_decomp(ctx, c, and_anc, ext_ctrl);
    qc_emit_ccx_or_decomp(ctx, and_anc, a, b);
    qc_emit_ccx_or_decomp(ctx, a, c, ext_ctrl);
    qc_emit_ccx_or_decomp(ctx, b, a, ext_ctrl);
}

/**
 * @brief Dynamic controlled CDKM addition: b += a, controlled by ext_ctrl.
 *
 * Allocates carry + AND-ancilla internally.
 */
void qc_dynamic_cqq_add(circuit_ctx_t *ctx, const uint32_t *a,
                         const uint32_t *b, uint32_t width,
                         uint32_t control) {
    if (width == 0 || !ctx || !a || !b) return;
    if (width == 1) {
        qc_emit_ccx_or_decomp(ctx, b[0], a[0], control);
        return;
    }

    uint32_t carry, and_anc;
    if (qc_qubit_alloc(ctx, &carry) != QC_OK) return;
    if (qc_qubit_alloc(ctx, &and_anc) != QC_OK) {
        qc_qubit_free(ctx, carry);
        return;
    }

    /* Forward cMAJ sweep */
    emit_cmaj(ctx, carry, b[0], a[0], control, and_anc);
    for (uint32_t i = 1; i < width; i++) {
        emit_cmaj(ctx, a[i - 1], b[i], a[i], control, and_anc);
    }

    /* Reverse cUMA sweep */
    for (int i = (int)width - 1; i >= 1; i--) {
        emit_cuma(ctx, a[i - 1], b[i], a[i], control, and_anc);
    }
    emit_cuma(ctx, carry, b[0], a[0], control, and_anc);

    qc_qubit_free(ctx, and_anc);
    qc_qubit_free(ctx, carry);
}

/**
 * @brief Dynamic controlled CDKM subtraction: b -= a, controlled.
 */
void qc_dynamic_cqq_sub(circuit_ctx_t *ctx, const uint32_t *a,
                         const uint32_t *b, uint32_t width,
                         uint32_t control) {
    if (width == 0 || !ctx || !a || !b) return;
    if (width == 1) {
        qc_emit_ccx_or_decomp(ctx, b[0], a[0], control);
        return;
    }

    uint32_t carry, and_anc;
    if (qc_qubit_alloc(ctx, &carry) != QC_OK) return;
    if (qc_qubit_alloc(ctx, &and_anc) != QC_OK) {
        qc_qubit_free(ctx, carry);
        return;
    }

    /* Inverse: cUMA forward then cMAJ backward */
    emit_cuma(ctx, carry, b[0], a[0], control, and_anc);
    for (uint32_t i = 1; i < width; i++) {
        emit_cuma(ctx, a[i - 1], b[i], a[i], control, and_anc);
    }

    for (int i = (int)width - 1; i >= 1; i--) {
        emit_cmaj(ctx, a[i - 1], b[i], a[i], control, and_anc);
    }
    emit_cmaj(ctx, carry, b[0], a[0], control, and_anc);

    qc_qubit_free(ctx, and_anc);
    qc_qubit_free(ctx, carry);
}

/**
 * @brief Dynamic controlled CQ addition: target += value, controlled.
 */
void qc_dynamic_ccq_add(circuit_ctx_t *ctx, const uint32_t *target,
                         uint32_t width, int64_t value, uint32_t control) {
    if (width == 0 || !ctx || !target) return;

    if (width == 1) {
        if (value & 1) {
            qc_circuit_cx(ctx, control, target[0]);
        }
        return;
    }

    int *bin = qc_two_complement(value, (int)width);
    if (!bin) return;

    uint32_t temp_start;
    if (qc_qubit_alloc_n(ctx, width, &temp_start) != QC_OK) {
        free(bin);
        return;
    }
    uint32_t temp[64];
    for (uint32_t i = 0; i < width; i++) {
        temp[i] = temp_start + i;
    }

    /* Controlled init temp via CX(target=temp[i], control=control) */
    for (uint32_t i = 0; i < width; i++) {
        if (bin[width - 1 - i]) {
            qc_circuit_cx(ctx, control, temp[i]);
        }
    }

    /* Controlled QQ add */
    qc_dynamic_cqq_add(ctx, temp, target, width, control);

    /* Controlled cleanup */
    for (uint32_t i = 0; i < width; i++) {
        if (bin[width - 1 - i]) {
            qc_circuit_cx(ctx, control, temp[i]);
        }
    }

    qc_qubit_free_n(ctx, temp_start, width);
    free(bin);
}

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
