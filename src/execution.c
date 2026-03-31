/**
 * @file execution.c
 * @brief Instruction execution engine for the circuit backend.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/execution.c
 * Module: 1.5 (Phase 1)
 * Issue: refactor-02e
 *
 * All functions take circuit_ctx_t* ctx as first argument, replacing the
 * monolith's global circuit_t* parameter. The sequence_t type is replaced
 * by the internal qc_sequence_t.
 *
 * Key functions:
 *   - qc_sequence_compute_total_gate_count: pre-compute total gate count
 *   - qc_run_instruction: apply a pre-built sequence to mapped qubits
 *   - qc_reverse_circuit_range: reverse a range of layers (uncomputation)
 *
 * Thread safety: Each circuit_ctx_t is independent. No global state.
 */

#include "internal.h"

#include <assert.h>
#include <stdint.h>

/* ---------------------------------------------------------------------- */
/* qc_sequence_compute_total_gate_count                                    */
/* ---------------------------------------------------------------------- */

void qc_sequence_compute_total_gate_count(qc_sequence_t *seq) {
    if (seq == NULL)
        return;

    uint32_t total = 0;
    for (int i = 0; i < (int)seq->used_layer; ++i) {
        total += seq->gates_per_layer[i];
    }
    seq->total_gate_count = total;
}

/* ---------------------------------------------------------------------- */
/* qc_run_instruction                                                      */
/* ---------------------------------------------------------------------- */

/**
 * Apply a pre-built gate sequence to the circuit, mapping abstract qubit
 * indices through qubit_array[].
 *
 * When ctx->simulate is 0 (count-only mode), the gates are not stored;
 * instead ctx->gate_count is incremented by the total gate count. Uses
 * pre-computed total_gate_count for O(1) when available, otherwise falls
 * back to per-layer summation.
 *
 * When invert is 1, the sequence is applied in reverse layer order with
 * negated gate values for non-self-inverse gates (rotation inversion).
 */
void qc_run_instruction(circuit_ctx_t *ctx, qc_sequence_t *seq,
                        const uint32_t qubit_array[], int invert) {
    if (ctx == NULL || seq == NULL)
        return;

    /* Fast path: count-only mode (simulate == 0) */
    if (!ctx->simulate) {
        if (seq->total_gate_count > 0) {
            ctx->gate_count += seq->total_gate_count;
        } else {
            for (int layer_index = 0; layer_index < (int)seq->used_layer; ++layer_index) {
                ctx->gate_count += seq->gates_per_layer[layer_index];
            }
        }
        return;
    }

    int direction = (invert) ? -1 : 1;

    for (int layer_index = 0; layer_index < (int)seq->used_layer; ++layer_index) {
        /* Calculate actual layer accounting for inversion direction */
        size_t layer = (size_t)((int)(invert * seq->used_layer) +
                                direction * layer_index - invert);

        for (int gate_index = 0; gate_index < (int)seq->gates_per_layer[layer]; ++gate_index) {
            /* Calculate actual gate index accounting for inversion */
            size_t gate = (size_t)((int)(invert * seq->gates_per_layer[layer]) +
                                   direction * gate_index - invert);

            /* Stack-allocated copy of the gate (no malloc, no leak) */
            qc_gate_internal_t g;
            memcpy(&g, &seq->seq[layer][gate], sizeof(qc_gate_internal_t));

            /* Map target qubit through qubit_array */
            g.Target = qubit_array[g.Target];

            /* Handle n-controlled gates (controls in large_control) */
            if (g.NumControls > QC_MAX_INLINE_CONTROLS &&
                seq->seq[layer][gate].large_control != NULL) {
                /* Allocate new large_control array for mapped qubits */
                g.large_control = malloc(g.NumControls * sizeof(uint32_t));
                if (g.large_control != NULL) {
                    for (int i = 0; i < (int)g.NumControls; ++i) {
                        g.large_control[i] =
                            qubit_array[seq->seq[layer][gate].large_control[i]];
                    }
                    /* Also update Control[0] and Control[1] for compatibility */
                    g.Control[0] = g.large_control[0];
                    g.Control[1] = g.large_control[1];
                }
            } else {
                /* Standard case: up to 2 controls in Control[] array */
                for (int i = 0; i < (int)g.NumControls &&
                                i < QC_MAX_INLINE_CONTROLS; ++i) {
                    g.Control[i] = qubit_array[g.Control[i]];
                }
            }

            /* Invert gate value only for non-self-inverse gates */
            if (invert) {
                switch (g.Gate) {
                case QC_IGATE_X:
                case QC_IGATE_Y:
                case QC_IGATE_Z:
                case QC_IGATE_H:
                case QC_IGATE_M:
                    /* Self-inverse gates: GateValue unchanged */
                    break;
                default:
                    /* Phase/rotation gates: negate for inversion */
                    g.GateValue = -g.GateValue;
                    break;
                }
            }

            qc_add_gate(ctx, &g);
            /*
             * NOTE: gate_count is incremented inside qc_add_gate().
             * Do NOT free g.large_control here. The circuit takes
             * ownership of the allocated large_control via memcpy in
             * qc_append_gate. qc_circuit_destroy handles cleanup.
             */
        }
    }
}

/* ---------------------------------------------------------------------- */
/* qc_reverse_circuit_range                                                */
/* ---------------------------------------------------------------------- */

/**
 * Reverse gates from circuit layers [start_layer, end_layer) in LIFO order.
 * Used for automatic uncomputation of intermediate quantum values.
 *
 * For each gate in the reversed range:
 *   - Self-inverse gates (X, Y, Z, H, M) keep their GateValue
 *   - Rotation gates (P, Rx, Ry, Rz, R, T, Tdg) get negated GateValue
 *   - Multi-controlled gates get a fresh large_control copy
 *
 * The inverted gates are appended to the current end of the circuit.
 */
void qc_reverse_circuit_range(circuit_ctx_t *ctx, int start_layer, int end_layer) {
    assert(ctx != NULL);

    /* Empty range: no-op */
    if (start_layer >= end_layer)
        return;

    /* Iterate backwards from end_layer - 1 down to start_layer (LIFO) */
    for (int layer_index = end_layer - 1; layer_index >= start_layer; --layer_index) {
        /* Iterate backwards through gates in this layer */
        for (int gate_index = (int)ctx->used_gates_per_layer[layer_index] - 1;
             gate_index >= 0; --gate_index) {
            qc_gate_internal_t *original_gate =
                &ctx->sequence[layer_index][gate_index];

            /* Stack-allocated copy with inverted GateValue */
            qc_gate_internal_t g;
            memcpy(&g, original_gate, sizeof(qc_gate_internal_t));

            /* Invert only non-self-inverse gates */
            switch (g.Gate) {
            case QC_IGATE_X:
            case QC_IGATE_Y:
            case QC_IGATE_Z:
            case QC_IGATE_H:
            case QC_IGATE_M:
                /* Self-inverse: GateValue unchanged */
                break;
            default:
                /* Phase/rotation gates: negate for inversion */
                g.GateValue = -g.GateValue;
                break;
            }

            /* Handle n-controlled gates: allocate new large_control */
            if (g.NumControls > QC_MAX_INLINE_CONTROLS &&
                original_gate->large_control != NULL) {
                g.large_control = malloc(g.NumControls * sizeof(uint32_t));
                if (g.large_control != NULL) {
                    for (int i = 0; i < (int)g.NumControls; ++i) {
                        g.large_control[i] = original_gate->large_control[i];
                    }
                    /* Update Control[0] and Control[1] for compatibility */
                    g.Control[0] = g.large_control[0];
                    g.Control[1] = g.large_control[1];
                }
            }
            /* For gates with <= 2 controls, memcpy already copied Control[] */

            /* Append inverted gate to circuit */
            qc_add_gate(ctx, &g);
            /*
             * NOTE: Do NOT free g.large_control here. The circuit takes
             * ownership via memcpy in qc_append_gate.
             */
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Public API wrappers (validated entry points)                            */
/* ---------------------------------------------------------------------- */

/**
 * @brief Public API: Reverse gates in a layer range [start, end).
 *
 * Wraps qc_reverse_circuit_range with NULL check and returns error code.
 */
qc_error_t qc_circuit_reverse_range(circuit_ctx_t *ctx, uint32_t start_layer,
                                     uint32_t end_layer) {
    if (ctx == NULL)
        return QC_ERR_NULL;

    qc_reverse_circuit_range(ctx, (int)start_layer, (int)end_layer);
    return QC_OK;
}
