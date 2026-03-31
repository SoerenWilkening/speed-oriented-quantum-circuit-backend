/**
 * @file optimizer.c
 * @brief Layer-based gate optimization (second half of monolith optimizer.c).
 *
 * Implements intelligent gate placement into the circuit's layer structure:
 *   - Layer assignment for minimal circuit depth (binary search on occupied layers)
 *   - Inverse gate cancellation (merge_gates)
 *   - Collision detection between gates sharing qubits
 *   - Layer bookkeeping (apply_layer, append_gate)
 *
 * All functions take circuit_ctx_t* ctx as the first argument, replacing the
 * monolith's global circuit_t* parameter.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/optimizer.c
 * Module: 1.6 (Phase 1)
 * Issue: refactor-5sm
 */

#include "internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------- */
/* smallest_layer_below_comp                                               */
/* ---------------------------------------------------------------------- */

/**
 * Binary search: find the largest occupied layer index < compar for a qubit.
 * occupied_layers_of_qubit is sorted in monotonically increasing order.
 *
 * Returns 0 if no occupied layer exists below compar.
 */
size_t qc_smallest_layer_below_comp(circuit_ctx_t *ctx, uint32_t qubit, size_t compar) {
    if (ctx == NULL) return 0;

    int count = (int)ctx->used_occupation_indices_per_qubit[qubit];
    if (count <= 0)
        return 0;

    size_t *arr = ctx->occupied_layers_of_qubit[qubit];

    /* Binary search: find lower_bound (first element >= compar) */
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] < compar)
            lo = mid + 1;
        else
            hi = mid;
    }

    /* lo is the first index where arr[lo] >= compar.
     * The largest element < compar is at arr[lo - 1]. */
    return (lo > 0) ? arr[lo - 1] : 0;
}

/* ---------------------------------------------------------------------- */
/* minimum_layer                                                           */
/* ---------------------------------------------------------------------- */

/**
 * Determine the minimal possible layer where a gate can be placed,
 * considering all qubits (target + controls) the gate touches.
 */
size_t qc_minimum_layer(circuit_ctx_t *ctx, qc_gate_internal_t *g, size_t compared_layer) {
    if (ctx == NULL || g == NULL) return 0;

    size_t min_possible_layer = ctx->layer_floor;

    /* Check all control qubits */
    for (int j = 0; j < (int)g->NumControls; ++j) {
        uint32_t qubit = qc_get_control(g, j);
        size_t min_layer = qc_smallest_layer_below_comp(ctx, qubit, compared_layer);
        if (min_layer > min_possible_layer)
            min_possible_layer = min_layer;
    }

    /* Check target qubit */
    size_t min_layer = qc_smallest_layer_below_comp(ctx, g->Target, compared_layer);
    if (min_layer > min_possible_layer)
        min_possible_layer = min_layer;

    return min_possible_layer;
}

/* ---------------------------------------------------------------------- */
/* merge_gates                                                             */
/* ---------------------------------------------------------------------- */

/**
 * Remove an inverse gate pair. Called when the gate being added is the
 * inverse of an existing gate at min_possible_layer - 1.
 *
 * Removes the existing gate from its layer, shifts remaining gates,
 * updates spatial indices, and removes empty layers.
 *
 * Returns 0 (false) to indicate no further processing needed.
 */
int qc_merge_gates(circuit_ctx_t *ctx, qc_gate_internal_t *g,
                   size_t min_possible_layer, int gate_index) {
    if (ctx == NULL || g == NULL) return 0;

    size_t layer_idx = min_possible_layer - 1;

    /* Reset target index in spatial map */
    ctx->gate_index_of_layer_and_qubits[layer_idx][g->Target] = -1;

    /* Reset control indices */
    for (int k = 0; k < (int)g->NumControls; ++k) {
        uint32_t ctrl = qc_get_control(g, k);
        ctx->gate_index_of_layer_and_qubits[layer_idx][ctrl] = -1;
        ctx->used_occupation_indices_per_qubit[ctrl]--;
    }

    /* Update target qubit's occupied layers */
    ctx->occupied_layers_of_qubit[g->Target]
        [ctx->used_occupation_indices_per_qubit[g->Target] - 1] = 0;
    ctx->used_occupation_indices_per_qubit[g->Target]--;

    /* Shift remaining gates in the layer to fill the gap */
    for (int k = gate_index;
         k < (int)ctx->used_gates_per_layer[layer_idx] - 1; ++k) {
        ctx->sequence[layer_idx][k] = ctx->sequence[layer_idx][k + 1];

        qc_gate_internal_t *helper = &ctx->sequence[layer_idx][k];
        /* Update stored gate index of shifted gates */
        ctx->gate_index_of_layer_and_qubits[layer_idx][helper->Target]--;
        for (int l = 0; l < (int)helper->NumControls; ++l) {
            uint32_t hctrl = qc_get_control(helper, l);
            ctx->gate_index_of_layer_and_qubits[layer_idx][hctrl]--;
        }
    }

    /* Layer contains fewer gates */
    ctx->used_gates_per_layer[layer_idx]--;
    ctx->used--;

    /* Remove layer entirely if last gate was removed */
    if (ctx->used_gates_per_layer[layer_idx] == 0) {
        for (size_t j = layer_idx; j < ctx->used_layer - 1; ++j)
            *ctx->sequence[j] = *ctx->sequence[j + 1];
        ctx->used_layer--;
    }

    return 0;
}

/* ---------------------------------------------------------------------- */
/* apply_layer                                                             */
/* ---------------------------------------------------------------------- */

/**
 * Record a gate's layer occupancy: update occupied_layers_of_qubit for
 * all qubits the gate touches, and update layer/depth tracking.
 */
void qc_apply_layer(circuit_ctx_t *ctx, qc_gate_internal_t *g, size_t min_possible_layer) {
    if (ctx == NULL || g == NULL) return;

    /* Update control qubits */
    for (int j = 0; j < (int)g->NumControls; ++j) {
        uint32_t loc = qc_get_control(g, j);
        qc_allocate_more_indices_per_qubit(ctx, (int)loc);
        ctx->occupied_layers_of_qubit[loc]
            [ctx->used_occupation_indices_per_qubit[loc]++] = min_possible_layer + 1;
    }

    /* Update target qubit */
    uint32_t loc = g->Target;
    qc_allocate_more_indices_per_qubit(ctx, (int)loc);
    ctx->occupied_layers_of_qubit[loc]
        [ctx->used_occupation_indices_per_qubit[loc]++] = min_possible_layer + 1;

    /* Update gates-per-layer count */
    ctx->used_gates_per_layer[min_possible_layer]++;

    /* Expand depth if needed */
    if (min_possible_layer + 1 > ctx->used_layer)
        ctx->used_layer = (uint32_t)(min_possible_layer + 1);
}

/* ---------------------------------------------------------------------- */
/* append_gate                                                             */
/* ---------------------------------------------------------------------- */

/**
 * Append a gate to a specific layer. Copies the gate into the layer's
 * gate array and updates the spatial index.
 */
void qc_append_gate(circuit_ctx_t *ctx, qc_gate_internal_t *g, size_t min_possible_layer) {
    if (ctx == NULL || g == NULL) return;

    size_t pos = ctx->used_gates_per_layer[min_possible_layer];
    qc_allocate_more_gates_per_layer(ctx, min_possible_layer, pos);

    /* Copy gate into layer */
    memcpy(&ctx->sequence[min_possible_layer][pos], g, sizeof(qc_gate_internal_t));

    /* Update spatial index */
    ctx->gate_index_of_layer_and_qubits[min_possible_layer][g->Target] = (int)pos;
    for (int i = 0; i < (int)g->NumControls; ++i) {
        uint32_t ctrl = qc_get_control(g, i);
        ctx->gate_index_of_layer_and_qubits[min_possible_layer][ctrl] = (int)pos;
    }

    ctx->used++;
}

/* ---------------------------------------------------------------------- */
/* colliding_gates                                                         */
/* ---------------------------------------------------------------------- */

/**
 * Find gates in the layer immediately below min_possible_layer that
 * share qubits with the incoming gate. Stack-allocated output arrays
 * (no malloc per gate -- Phase 61 optimization).
 *
 * Checks target qubit first, then control qubits. Returns at most one
 * colliding gate (the first found).
 */
void qc_colliding_gates(circuit_ctx_t *ctx, qc_gate_internal_t *g,
                        size_t min_possible_layer, int *gate_index,
                        qc_gate_internal_t **coll) {
    coll[0] = NULL;
    coll[1] = NULL;
    coll[2] = NULL;

    if (ctx == NULL || min_possible_layer == 0)
        return;

    size_t prev_layer = min_possible_layer - 1;

    /* Check target qubit first */
    gate_index[0] = ctx->gate_index_of_layer_and_qubits[prev_layer][g->Target];
    if (gate_index[0] >= 0) {
        coll[0] = &ctx->sequence[prev_layer][gate_index[0]];
        return;
    }

    /* Check control qubits */
    for (int i = 0; i < (int)g->NumControls; ++i) {
        uint32_t ctrl = qc_get_control(g, i);
        int step = ctx->gate_index_of_layer_and_qubits[prev_layer][ctrl];
        if (step >= 0) {
            coll[0] = &ctx->sequence[prev_layer][step];
            gate_index[0] = step;
            return;
        }
    }
}

/* ---------------------------------------------------------------------- */
/* add_gate (main entry point)                                             */
/* ---------------------------------------------------------------------- */

/**
 * Add a gate to the circuit with automatic layer assignment, collision
 * detection, and inverse gate cancellation.
 *
 * This is the main entry point for all gate insertion. The algorithm:
 * 1. Expand qubit arrays if needed
 * 2. Find minimum possible layer via binary search on occupied layers
 * 3. Check for colliding gates in the layer below
 * 4. If a colliding gate is the inverse, merge (cancel both)
 * 5. Otherwise, append the gate to the minimum layer
 */
void qc_add_gate(circuit_ctx_t *ctx, qc_gate_internal_t *g) {
    if (ctx == NULL || g == NULL) return;

    /* Always count the gate (both simulate and count-only modes) */
    ctx->gate_count++;

    /* In count-only mode, do not store the gate */
    if (!ctx->simulate) return;

    /* Ensure qubit arrays are large enough */
    qc_allocate_more_qubits(ctx, g);

    /* Track highest qubit index used */
    uint32_t mq = qc_max_qubit(g);
    if (mq > ctx->used_qubits)
        ctx->used_qubits = mq;

    size_t min_possible_layer;
    size_t prev = SIZE_MAX;

    for (int i = 0; i < 1; ++i) {
        min_possible_layer = qc_minimum_layer(ctx, g, prev);
        qc_allocate_more_layer(ctx, min_possible_layer);

        /* Find colliding gates (stack-allocated, no malloc) */
        int gate_index[3];
        qc_gate_internal_t *coll[3];
        qc_colliding_gates(ctx, g, min_possible_layer, gate_index, coll);

        (void)(prev);

        for (int j = 0; j < 1; ++j) {
            qc_gate_internal_t *g2 = coll[j];
            if (g2 != NULL) {
                /* If inverse, cancel the gate pair */
                if (qc_gates_are_inverse(g, g2)) {
                    qc_merge_gates(ctx, g, min_possible_layer, gate_index[j]);
                    return;
                }
            }
        }
    }

    qc_append_gate(ctx, g, min_possible_layer);
    qc_apply_layer(ctx, g, min_possible_layer);
}
