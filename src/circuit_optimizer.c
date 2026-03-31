/**
 * @file circuit_optimizer.c
 * @brief Post-construction circuit optimization passes.
 *
 * Implements optimization passes that operate on a fully-constructed circuit:
 *   - qc_circuit_optimize: run all passes, producing a new optimized circuit
 *   - qc_circuit_optimize_pass: run a specific pass
 *   - qc_circuit_can_optimize: heuristic check
 *
 * The optimization strategy is to copy the circuit via qc_add_gate, which
 * already performs inverse gate cancellation during insertion. This means
 * the "copy" is itself an optimization pass.
 *
 * All public functions take circuit_ctx_t* ctx as the first argument,
 * matching the public API in quantum_circuit.h.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/circuit_optimizer.c
 * Module: 1.6 (Phase 1)
 * Issue: refactor-5sm
 */

#include "internal.h"

#include <string.h>

/* ---------------------------------------------------------------------- */
/* Internal: deep copy a circuit via gate replay                           */
/* ---------------------------------------------------------------------- */

/**
 * Deep-copy a circuit by replaying all gates through qc_add_gate.
 * This applies the built-in inverse cancellation and layer optimization
 * during the copy, producing an optimized result.
 *
 * Returns a new circuit_ctx_t* (caller owns), or NULL on failure.
 */
static circuit_ctx_t *copy_circuit_optimized(circuit_ctx_t *src) {
    if (src == NULL)
        return NULL;

    circuit_ctx_t *dst = qc_circuit_create(src->allocated_qubits);
    if (dst == NULL)
        return NULL;

    /* Copy configuration from source */
    dst->toff_decomp = src->toff_decomp;
    dst->arithmetic_mode = src->arithmetic_mode;
    dst->cla_override = src->cla_override;
    dst->qubit_saving = src->qubit_saving;
    dst->toffoli_decompose = src->toffoli_decompose;
    dst->tradeoff_auto_threshold = src->tradeoff_auto_threshold;
    dst->tradeoff_min_depth = src->tradeoff_min_depth;
    dst->simulate = src->simulate;

    /* Replay gates layer by layer */
    for (uint32_t layer = 0; layer < src->used_layer; layer++) {
        for (uint32_t gi = 0; gi < src->used_gates_per_layer[layer]; gi++) {
            qc_gate_internal_t g_copy = src->sequence[layer][gi];

            /* Deep-copy large_control if present */
            if (g_copy.NumControls > QC_MAX_INLINE_CONTROLS &&
                g_copy.large_control != NULL) {
                uint32_t *lc = malloc(g_copy.NumControls * sizeof(uint32_t));
                if (lc != NULL) {
                    memcpy(lc, g_copy.large_control,
                           g_copy.NumControls * sizeof(uint32_t));
                    g_copy.large_control = lc;
                } else {
                    /* Fallback: skip gate on alloc failure */
                    continue;
                }
            } else {
                g_copy.large_control = NULL;
            }

            qc_add_gate(dst, &g_copy);
        }
    }

    return dst;
}

/* ---------------------------------------------------------------------- */
/* Internal: cancel inverse gate pairs (analysis pass)                     */
/* ---------------------------------------------------------------------- */

/**
 * Scan for consecutive inverse gate pairs on each qubit.
 * Returns count of gates that would be removed.
 *
 * This is primarily a diagnostic/heuristic function. The actual removal
 * happens via copy_circuit_optimized's replay through qc_add_gate.
 */
static int count_cancellable_inverse_pairs(const circuit_ctx_t *ctx) {
    if (ctx == NULL || ctx->used == 0)
        return 0;

    int removable = 0;

    for (uint32_t qubit = 0; qubit <= ctx->used_qubits; qubit++) {
        const qc_gate_internal_t *last_gate = NULL;

        for (uint32_t layer = 0; layer < ctx->used_layer; layer++) {
            if (qubit >= ctx->allocated_qubits)
                break;

            int gate_idx = ctx->gate_index_of_layer_and_qubits[layer][qubit];
            if (gate_idx < 0)
                continue;

            const qc_gate_internal_t *g = &ctx->sequence[layer][gate_idx];

            /* Only check single-qubit gates for simple inverse pairs */
            if (g->NumControls > 0) {
                last_gate = g;
                continue;
            }

            if (last_gate != NULL && last_gate->NumControls == 0) {
                if (qc_gates_are_inverse(last_gate, g)) {
                    removable += 2;
                    last_gate = NULL;  /* Reset after finding a pair */
                    continue;
                }
            }

            last_gate = g;
        }
    }

    return removable;
}

/* ---------------------------------------------------------------------- */
/* Public API: qc_circuit_optimize                                         */
/* ---------------------------------------------------------------------- */

/**
 * Run all optimization passes, producing a new optimized circuit.
 * The source circuit is not modified.
 *
 * Current strategy: replay all gates via qc_add_gate, which applies
 * inverse cancellation during insertion.
 *
 * Returns a new circuit_ctx_t* (caller owns), or NULL on failure.
 */
circuit_ctx_t *qc_circuit_optimize(circuit_ctx_t *ctx) {
    if (ctx == NULL)
        return NULL;

    return copy_circuit_optimized(ctx);
}

/* ---------------------------------------------------------------------- */
/* Public API: qc_circuit_optimize_pass                                    */
/* ---------------------------------------------------------------------- */

/**
 * Run a specific optimization pass, producing a new optimized circuit.
 * The source circuit is not modified.
 *
 * Currently all passes go through the copy-replay strategy which
 * applies qc_add_gate's built-in inverse cancellation.
 *
 * Returns a new circuit_ctx_t* (caller owns), or NULL on failure.
 */
circuit_ctx_t *qc_circuit_optimize_pass(circuit_ctx_t *ctx, qc_opt_pass_t pass) {
    if (ctx == NULL)
        return NULL;

    /* All passes currently use the same copy-replay strategy */
    switch (pass) {
    case QC_OPT_CANCEL_INVERSE:
    case QC_OPT_MERGE:
        return copy_circuit_optimized(ctx);
    default:
        return copy_circuit_optimized(ctx);
    }
}

/* ---------------------------------------------------------------------- */
/* Public API: qc_circuit_can_optimize                                     */
/* ---------------------------------------------------------------------- */

/**
 * Heuristic check: returns true if optimization would change the circuit.
 *
 * Uses count_cancellable_inverse_pairs as a quick check. Falls back to
 * a simple "has gates" check.
 */
bool qc_circuit_can_optimize(const circuit_ctx_t *ctx) {
    if (ctx == NULL || ctx->used == 0)
        return false;

    /* Quick heuristic: check for cancellable inverse pairs */
    int cancellable = count_cancellable_inverse_pairs(ctx);
    if (cancellable > 0)
        return true;

    /* Conservative: if there are gates, optimization might help
     * (future passes may do more than inverse cancellation) */
    return (ctx->used > 0);
}
