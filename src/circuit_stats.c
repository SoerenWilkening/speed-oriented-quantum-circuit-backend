/**
 * @file circuit_stats.c
 * @brief Circuit statistics collection — refactored for ctx.
 *
 * Refactored from monolith Quantum_Assembly/c_backend/src/circuit_stats.c.
 * All functions take circuit_ctx_t* instead of circuit_t*.
 *
 * Public API implemented:
 *   - qc_circuit_gate_count()
 *   - qc_circuit_depth()
 *   - qc_circuit_width()
 *   - qc_circuit_num_qubits()
 *   - qc_circuit_gate_counts()
 *   - qc_circuit_gate_counts_range()
 *   - qc_circuit_alloc_stats()
 *
 * @see quantum_circuit.h for public declarations.
 * @see internal.h for circuit_ctx struct definition.
 */

#include "internal.h"

/* ====================================================================== */
/* Scalar statistics                                                       */
/* ====================================================================== */

QC_API uint64_t qc_circuit_gate_count(const circuit_ctx_t *ctx) {
    if (ctx == NULL) return 0;
    return (uint64_t)ctx->gate_count;
}

QC_API uint32_t qc_circuit_depth(const circuit_ctx_t *ctx) {
    if (ctx == NULL) return 0;
    return ctx->used_layer;
}

QC_API uint32_t qc_circuit_width(const circuit_ctx_t *ctx) {
    if (ctx == NULL) return 0;
    return ctx->used_qubits + 1;
}

QC_API uint32_t qc_circuit_num_qubits(const circuit_ctx_t *ctx) {
    if (ctx == NULL) return 0;
    if (ctx->allocator == NULL) return 0;
    return ctx->allocator->stats.current_in_use;
}

/* ====================================================================== */
/* Allocation statistics                                                   */
/* ====================================================================== */

QC_API qc_alloc_stats_t qc_circuit_alloc_stats(const circuit_ctx_t *ctx) {
    qc_alloc_stats_t stats = {0, 0, 0, 0};
    if (ctx == NULL || ctx->allocator == NULL) return stats;

    stats.peak_allocated      = ctx->allocator->stats.peak_allocated;
    stats.total_allocations   = ctx->allocator->stats.total_allocations;
    stats.total_deallocations = ctx->allocator->stats.total_deallocations;
    stats.current_in_use      = ctx->allocator->stats.current_in_use;
    return stats;
}

/* ====================================================================== */
/* Internal: accumulate gate counts over a layer range                     */
/* ====================================================================== */

/**
 * @brief Count gates by type across layers [start, end).
 *
 * Shared implementation for both full-circuit and range queries.
 * Gate classification follows the monolith conventions:
 *   - X with 0 controls -> x_gates
 *   - X with 1 control  -> cx_gates (CNOT)
 *   - X with 2 controls -> ccx_gates (Toffoli)
 *   - X with 3+ controls -> other_gates (MCX)
 *   - P, Rx, Ry, Rz     -> p_gates (all parameterized rotations)
 *   - T                  -> t_gates
 *   - Tdg                -> tdg_gates
 *   - All others         -> other_gates
 */
static qc_gate_counts_t count_gates_in_range(const circuit_ctx_t *ctx,
                                              uint32_t start, uint32_t end) {
    qc_gate_counts_t counts;
    memset(&counts, 0, sizeof(counts));

    if (ctx == NULL || start >= end) return counts;

    if (end > ctx->used_layer) end = ctx->used_layer;

    for (uint32_t layer = start; layer < end; layer++) {
        for (uint32_t gi = 0; gi < ctx->used_gates_per_layer[layer]; gi++) {
            const qc_gate_internal_t *g = &ctx->sequence[layer][gi];

            switch (g->Gate) {
            case QC_IGATE_X:
                if (g->NumControls == 0)
                    counts.x_gates++;
                else if (g->NumControls == 1)
                    counts.cx_gates++;
                else if (g->NumControls == 2)
                    counts.ccx_gates++;
                else
                    counts.other_gates++;
                break;
            case QC_IGATE_Y:
                counts.y_gates++;
                break;
            case QC_IGATE_Z:
                counts.z_gates++;
                break;
            case QC_IGATE_H:
                counts.h_gates++;
                break;
            case QC_IGATE_P:
            case QC_IGATE_RX:
            case QC_IGATE_RY:
            case QC_IGATE_RZ:
                counts.p_gates++;
                break;
            case QC_IGATE_T:
                counts.t_gates++;
                break;
            case QC_IGATE_TDG:
                counts.tdg_gates++;
                break;
            default:
                counts.other_gates++;
                break;
            }
        }
    }

    /* T-count: actual T/Tdg if present, else estimate 7 T per CCX */
    if (counts.t_gates > 0 || counts.tdg_gates > 0) {
        counts.t_count = counts.t_gates + counts.tdg_gates;
    } else {
        counts.t_count = 7 * counts.ccx_gates;
    }

    return counts;
}

/* ====================================================================== */
/* Public API — gate counts                                                */
/* ====================================================================== */

QC_API qc_gate_counts_t qc_circuit_gate_counts(const circuit_ctx_t *ctx) {
    if (ctx == NULL) {
        qc_gate_counts_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return count_gates_in_range(ctx, 0, ctx->used_layer);
}

QC_API qc_gate_counts_t qc_circuit_gate_counts_range(const circuit_ctx_t *ctx,
                                                       uint32_t start_layer,
                                                       uint32_t end_layer) {
    return count_gates_in_range(ctx, start_layer, end_layer);
}

/* ====================================================================== */
/* Gate extraction                                                         */
/* ====================================================================== */

QC_API qc_error_t qc_circuit_extract_gates(const circuit_ctx_t *ctx,
                                             uint32_t start_layer,
                                             uint32_t end_layer,
                                             qc_exported_gate_t **out_gates,
                                             uint32_t *out_count) {
    if (ctx == NULL || out_gates == NULL || out_count == NULL)
        return QC_ERR_NULL;

    *out_gates = NULL;
    *out_count = 0;

    if (end_layer > ctx->used_layer) end_layer = ctx->used_layer;
    if (start_layer >= end_layer) return QC_OK;

    /* Count total gates in range */
    uint32_t total = 0;
    for (uint32_t layer = start_layer; layer < end_layer; layer++) {
        total += ctx->used_gates_per_layer[layer];
    }
    if (total == 0) return QC_OK;

    qc_exported_gate_t *gates = (qc_exported_gate_t *)malloc(
        total * sizeof(qc_exported_gate_t));
    if (gates == NULL) return QC_ERR_ALLOC;

    uint32_t idx = 0;
    for (uint32_t layer = start_layer; layer < end_layer; layer++) {
        for (uint32_t gi = 0; gi < ctx->used_gates_per_layer[layer]; gi++) {
            const qc_gate_internal_t *g = &ctx->sequence[layer][gi];
            qc_exported_gate_t *eg = &gates[idx++];
            eg->gate_type = (uint32_t)g->Gate;
            eg->target = g->Target;
            eg->angle = g->GateValue;
            eg->num_controls = g->NumControls;
            for (uint32_t ci = 0; ci < g->NumControls && ci < 8; ci++) {
                eg->controls[ci] = qc_get_control(g, (int)ci);
            }
        }
    }

    *out_gates = gates;
    *out_count = idx;
    return QC_OK;
}

QC_API uint32_t qc_circuit_used_layer(const circuit_ctx_t *ctx) {
    if (ctx == NULL) return 0;
    return ctx->used_layer;
}
