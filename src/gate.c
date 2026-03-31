/**
 * @file gate.c
 * @brief Gate construction, analysis, and circuit-level gate addition.
 *
 * Refactored from monolith gate.c. All gate functions that interact with
 * the circuit now take circuit_ctx_t* ctx. Pure gate-struct initializers
 * operate on qc_gate_internal_t without context.
 *
 * Implements:
 * - Gate struct initializers (x, cx, ccx, h, p, t, etc.)
 * - Gate analysis (min/max qubit, inverse check, commutation)
 * - qc_add_gate(): main entry point for adding gates to a context
 * - Public qc_circuit_*() gate functions (API wrappers)
 * - Generic qc_circuit_add_gate() dispatcher
 */

#include "internal.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====================================================================== */
/* Gate struct initializers (no context interaction)                        */
/* ====================================================================== */

static void gate_init(qc_gate_internal_t *g) {
    memset(g, 0, sizeof(qc_gate_internal_t));
    g->large_control = NULL;
}

static void gate_x(qc_gate_internal_t *g, uint32_t target) {
    gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->GateValue = 1;
    g->NumControls = 0;
}

static void gate_y(qc_gate_internal_t *g, uint32_t target) {
    gate_init(g);
    g->Gate = QC_IGATE_Y;
    g->Target = target;
    g->NumControls = 0;
}

static void gate_z(qc_gate_internal_t *g, uint32_t target) {
    gate_init(g);
    g->Gate = QC_IGATE_Z;
    g->Target = target;
    g->NumControls = 0;
}

static void gate_h(qc_gate_internal_t *g, uint32_t target) {
    gate_init(g);
    g->Gate = QC_IGATE_H;
    g->Target = target;
    g->NumControls = 0;
}

static void gate_p(qc_gate_internal_t *g, uint32_t target, double angle) {
    gate_init(g);
    g->Gate = QC_IGATE_P;
    g->GateValue = angle;
    g->Target = target;
    g->NumControls = 0;
}

static void gate_rx(qc_gate_internal_t *g, uint32_t target, double angle) {
    gate_init(g);
    g->Gate = QC_IGATE_RX;
    g->GateValue = angle;
    g->Target = target;
    g->NumControls = 0;
}

static void gate_ry(qc_gate_internal_t *g, uint32_t target, double angle) {
    gate_init(g);
    g->Gate = QC_IGATE_RY;
    g->GateValue = angle;
    g->Target = target;
    g->NumControls = 0;
}

static void gate_rz(qc_gate_internal_t *g, uint32_t target, double angle) {
    gate_init(g);
    g->Gate = QC_IGATE_RZ;
    g->GateValue = angle;
    g->Target = target;
    g->NumControls = 0;
}

static void gate_t(qc_gate_internal_t *g, uint32_t target) {
    gate_init(g);
    g->Gate = QC_IGATE_T;
    g->Target = target;
    g->NumControls = 0;
    g->GateValue = M_PI / 4.0;
}

static void gate_tdg(qc_gate_internal_t *g, uint32_t target) {
    gate_init(g);
    g->Gate = QC_IGATE_TDG;
    g->Target = target;
    g->NumControls = 0;
    g->GateValue = -M_PI / 4.0;
}

static void gate_cx(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->NumControls = 1;
    g->GateValue = 1;
    g->Control[0] = control;
}

static void gate_cy(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    gate_init(g);
    g->Gate = QC_IGATE_Y;
    g->Target = target;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void gate_cz(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    gate_init(g);
    g->Gate = QC_IGATE_Z;
    g->Target = target;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void gate_ch(qc_gate_internal_t *g, uint32_t target, uint32_t control) {
    gate_init(g);
    g->Gate = QC_IGATE_H;
    g->Target = target;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void gate_cp(qc_gate_internal_t *g, uint32_t target, uint32_t control,
                    double angle) {
    gate_init(g);
    g->Gate = QC_IGATE_P;
    g->GateValue = angle;
    g->Target = target;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void gate_cry_init(qc_gate_internal_t *g, uint32_t target, uint32_t control,
                          double angle) {
    gate_init(g);
    g->Gate = QC_IGATE_RY;
    g->GateValue = angle;
    g->Target = target;
    g->NumControls = 1;
    g->Control[0] = control;
}

static void gate_ccx(qc_gate_internal_t *g, uint32_t target,
                     uint32_t ctrl1, uint32_t ctrl2) {
    gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->NumControls = 2;
    g->GateValue = 1;
    g->Control[0] = ctrl1;
    g->Control[1] = ctrl2;
}

static void gate_mcx(qc_gate_internal_t *g, uint32_t target,
                     const uint32_t *controls, uint32_t n_controls) {
    gate_init(g);
    g->Gate = QC_IGATE_X;
    g->Target = target;
    g->NumControls = n_controls;
    g->GateValue = 1;

    if (n_controls <= QC_MAX_INLINE_CONTROLS) {
        if (n_controls >= 1) g->Control[0] = controls[0];
        if (n_controls >= 2) g->Control[1] = controls[1];
    } else {
        g->large_control = malloc(n_controls * sizeof(uint32_t));
        if (g->large_control) {
            memcpy(g->large_control, controls, n_controls * sizeof(uint32_t));
            g->Control[0] = controls[0];
            g->Control[1] = controls[1];
        }
    }
}

static void gate_mcz(qc_gate_internal_t *g, uint32_t target,
                     const uint32_t *controls, uint32_t n_controls) {
    gate_init(g);
    g->Gate = QC_IGATE_Z;
    g->Target = target;
    g->NumControls = n_controls;

    if (n_controls <= QC_MAX_INLINE_CONTROLS) {
        if (n_controls >= 1) g->Control[0] = controls[0];
        if (n_controls >= 2) g->Control[1] = controls[1];
    } else {
        g->large_control = malloc(n_controls * sizeof(uint32_t));
        if (g->large_control) {
            memcpy(g->large_control, controls, n_controls * sizeof(uint32_t));
            g->Control[0] = controls[0];
            g->Control[1] = controls[1];
        }
    }
}

/* ====================================================================== */
/* Gate analysis                                                           */
/* ====================================================================== */

bool qc_gates_are_inverse(const qc_gate_internal_t *g1, const qc_gate_internal_t *g2) {
    if (!g2) return false;
    if (g1->Target != g2->Target) return false;
    if (g1->NumControls != g2->NumControls) return false;

    /* T and Tdg are inverses of each other */
    if ((g1->Gate == QC_IGATE_T && g2->Gate == QC_IGATE_TDG) ||
        (g1->Gate == QC_IGATE_TDG && g2->Gate == QC_IGATE_T)) {
        for (uint32_t i = 0; i < g1->NumControls; ++i)
            if (qc_get_control(g1, (int)i) != qc_get_control(g2, (int)i))
                return false;
        return true;
    }

    if (g1->Gate != g2->Gate) return false;

    if (g1->Gate == QC_IGATE_P || g1->Gate == QC_IGATE_RY ||
        g1->Gate == QC_IGATE_RX || g1->Gate == QC_IGATE_RZ) {
        if (g1->GateValue != -g2->GateValue) return false;
    } else if (g1->GateValue != g2->GateValue) {
        return false;
    }

    for (uint32_t i = 0; i < g1->NumControls; ++i)
        if (qc_get_control(g1, (int)i) != qc_get_control(g2, (int)i))
            return false;

    return true;
}

bool qc_gates_commute(const qc_gate_internal_t *g1, const qc_gate_internal_t *g2) {
    if (!g2) return true;
    if (g1->NumControls > 0 && g2->NumControls > 0 && g1->Target != g2->Target)
        return true;

    switch (g1->Gate) {
    case QC_IGATE_P:
        if (g2->Gate == QC_IGATE_P) return true;
        if (g2->Gate == QC_IGATE_Z) return true;
        if (g2->Gate == QC_IGATE_X && g1->Target != g2->Target) return true;
        break;
    case QC_IGATE_X:
        if (g2->Gate == QC_IGATE_X && g1->Target == g2->Target) return true;
        break;
    case QC_IGATE_H:
        if (g2->Gate == QC_IGATE_H && g1->Target == g2->Target) return true;
        break;
    case QC_IGATE_Z:
        if (g2->Gate == QC_IGATE_P) return true;
        if (g2->Gate == QC_IGATE_Z) return true;
        if (g2->Gate == QC_IGATE_X && g1->Target != g2->Target) return true;
        break;
    default:
        break;
    }
    return false;
}

/* Layer assignment and gate addition (qc_add_gate, qc_apply_layer, etc.)
 * are implemented in optimizer.c (Module 1.5-1.6). */

/* ====================================================================== */
/* Public API — single-qubit gates                                         */
/* ====================================================================== */

QC_API qc_error_t qc_circuit_x(circuit_ctx_t *ctx, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_x(&g, target);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_y(circuit_ctx_t *ctx, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_y(&g, target);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_z(circuit_ctx_t *ctx, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_z(&g, target);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_h(circuit_ctx_t *ctx, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_h(&g, target);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_t_gate(circuit_ctx_t *ctx, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_t(&g, target);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_tdg(circuit_ctx_t *ctx, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_tdg(&g, target);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_p(circuit_ctx_t *ctx, uint32_t target, double angle) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_p(&g, target, angle);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_rx(circuit_ctx_t *ctx, uint32_t target, double angle) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_rx(&g, target, angle);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_ry(circuit_ctx_t *ctx, uint32_t target, double angle) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_ry(&g, target, angle);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_rz(circuit_ctx_t *ctx, uint32_t target, double angle) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_rz(&g, target, angle);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

/* ====================================================================== */
/* Public API — two-qubit controlled gates                                 */
/* ====================================================================== */

QC_API qc_error_t qc_circuit_cx(circuit_ctx_t *ctx, uint32_t control, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_cx(&g, target, control);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_cy(circuit_ctx_t *ctx, uint32_t control, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_cy(&g, target, control);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_cz(circuit_ctx_t *ctx, uint32_t control, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_cz(&g, target, control);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_ch(circuit_ctx_t *ctx, uint32_t control, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_ch(&g, target, control);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_cp(circuit_ctx_t *ctx, uint32_t control, uint32_t target,
                                 double angle) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_cp(&g, target, control, angle);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_cry(circuit_ctx_t *ctx, uint32_t control, uint32_t target,
                                  double angle) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_cry_init(&g, target, control, angle);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

/* ====================================================================== */
/* Public API — multi-qubit gates                                          */
/* ====================================================================== */

QC_API qc_error_t qc_circuit_ccx(circuit_ctx_t *ctx, uint32_t ctrl1, uint32_t ctrl2,
                                  uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    qc_gate_internal_t g;
    gate_ccx(&g, target, ctrl1, ctrl2);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_mcx(circuit_ctx_t *ctx, const uint32_t *controls,
                                  uint32_t n_controls, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    if (!controls || n_controls == 0) return QC_ERR_GATE;
    qc_gate_internal_t g;
    gate_mcx(&g, target, controls, n_controls);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

QC_API qc_error_t qc_circuit_mcz(circuit_ctx_t *ctx, const uint32_t *controls,
                                  uint32_t n_controls, uint32_t target) {
    if (!ctx) return QC_ERR_NULL;
    if (!controls || n_controls == 0) return QC_ERR_GATE;
    qc_gate_internal_t g;
    gate_mcz(&g, target, controls, n_controls);
    qc_add_gate(ctx, &g);
    return QC_OK;
}

/* ====================================================================== */
/* Public API — generic gate insertion                                     */
/* ====================================================================== */

QC_API qc_error_t qc_circuit_add_gate(circuit_ctx_t *ctx, qc_gate_type_t type,
                                       uint32_t target, const uint32_t *controls,
                                       uint32_t n_controls, double angle) {
    if (!ctx) return QC_ERR_NULL;
    if ((int)type < 0 || type >= QC_GATE_COUNT) return QC_ERR_GATE;

    switch (type) {
    case QC_GATE_X:   return qc_circuit_x(ctx, target);
    case QC_GATE_Y:   return qc_circuit_y(ctx, target);
    case QC_GATE_Z:   return qc_circuit_z(ctx, target);
    case QC_GATE_H:   return qc_circuit_h(ctx, target);
    case QC_GATE_S:   return qc_circuit_p(ctx, target, M_PI / 2.0);
    case QC_GATE_SDG: return qc_circuit_p(ctx, target, -M_PI / 2.0);
    case QC_GATE_T:   return qc_circuit_t_gate(ctx, target);
    case QC_GATE_TDG: return qc_circuit_tdg(ctx, target);
    case QC_GATE_P:   return qc_circuit_p(ctx, target, angle);
    case QC_GATE_RX:  return qc_circuit_rx(ctx, target, angle);
    case QC_GATE_RY:  return qc_circuit_ry(ctx, target, angle);
    case QC_GATE_RZ:  return qc_circuit_rz(ctx, target, angle);
    case QC_GATE_CX:
        if (!controls || n_controls < 1) return QC_ERR_GATE;
        return qc_circuit_cx(ctx, controls[0], target);
    case QC_GATE_CY:
        if (!controls || n_controls < 1) return QC_ERR_GATE;
        return qc_circuit_cy(ctx, controls[0], target);
    case QC_GATE_CZ:
        if (!controls || n_controls < 1) return QC_ERR_GATE;
        return qc_circuit_cz(ctx, controls[0], target);
    case QC_GATE_CH:
        if (!controls || n_controls < 1) return QC_ERR_GATE;
        return qc_circuit_ch(ctx, controls[0], target);
    case QC_GATE_CP:
        if (!controls || n_controls < 1) return QC_ERR_GATE;
        return qc_circuit_cp(ctx, controls[0], target, angle);
    case QC_GATE_CRY:
        if (!controls || n_controls < 1) return QC_ERR_GATE;
        return qc_circuit_cry(ctx, controls[0], target, angle);
    case QC_GATE_CCX:
        if (!controls || n_controls < 2) return QC_ERR_GATE;
        return qc_circuit_ccx(ctx, controls[0], controls[1], target);
    case QC_GATE_MCX:
        return qc_circuit_mcx(ctx, controls, n_controls, target);
    case QC_GATE_MCZ:
        return qc_circuit_mcz(ctx, controls, n_controls, target);
    case QC_GATE_SWAP:
        /* SWAP = 3 CX gates */
        if (!controls || n_controls < 1) return QC_ERR_GATE;
        qc_circuit_cx(ctx, target, controls[0]);
        qc_circuit_cx(ctx, controls[0], target);
        qc_circuit_cx(ctx, target, controls[0]);
        return QC_OK;
    case QC_GATE_M: {
        qc_gate_internal_t g;
        gate_init(&g);
        g.Gate = QC_IGATE_M;
        g.Target = target;
        g.NumControls = 0;
        qc_add_gate(ctx, &g);
        return QC_OK;
    }
    default:
        return QC_ERR_GATE;
    }
}

/* qc_circuit_gate_count, qc_circuit_depth, qc_circuit_width moved to
 * circuit_stats.c (Module 1.7) — their authoritative home. */
