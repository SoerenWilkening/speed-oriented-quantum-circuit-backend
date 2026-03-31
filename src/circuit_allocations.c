/**
 * @file circuit_allocations.c
 * @brief Memory management for circuit gate/layer/qubit arrays.
 *
 * Refactored from monolith circuit_allocations.c. All functions take
 * circuit_ctx_t* ctx instead of using global state.
 *
 * Also implements the qubit allocator lifecycle (create/destroy/alloc/free)
 * used for qubit management through the public API.
 */

#include "internal.h"

/* ====================================================================== */
/* Circuit array growth — qubit dimension                                  */
/* ====================================================================== */

void qc_allocate_more_qubits(circuit_ctx_t *ctx, const qc_gate_internal_t *g) {
    uint32_t max = qc_max_qubit(g);
    if (max >= ctx->used_qubits)
        ctx->used_qubits = max;
    max++;
    if (max <= ctx->allocated_qubits)
        return;

    uint32_t new_cap = max + QC_QUBIT_BLOCK;

    /* used_occupation_indices_per_qubit */
    uint32_t *new_used_occ = realloc(ctx->used_occupation_indices_per_qubit,
                                     new_cap * sizeof(uint32_t));
    if (!new_used_occ) return;
    ctx->used_occupation_indices_per_qubit = new_used_occ;
    memset(ctx->used_occupation_indices_per_qubit + ctx->allocated_qubits, 0,
           (new_cap - ctx->allocated_qubits) * sizeof(uint32_t));

    /* occupied_layers_of_qubit */
    size_t **new_occ_layers = realloc(ctx->occupied_layers_of_qubit,
                                      new_cap * sizeof(size_t *));
    if (!new_occ_layers) return;
    ctx->occupied_layers_of_qubit = new_occ_layers;

    /* allocated_occupation_indices_per_qubit */
    uint32_t *new_alloc_occ = realloc(ctx->allocated_occupation_indices_per_qubit,
                                      new_cap * sizeof(uint32_t));
    if (!new_alloc_occ) return;
    ctx->allocated_occupation_indices_per_qubit = new_alloc_occ;

    for (uint32_t i = ctx->allocated_qubits; i < new_cap; ++i) {
        ctx->occupied_layers_of_qubit[i] =
            malloc(QC_QUBIT_INDEX_BLOCK * sizeof(size_t));
        if (!ctx->occupied_layers_of_qubit[i]) {
            for (uint32_t j = ctx->allocated_qubits; j < i; ++j)
                free(ctx->occupied_layers_of_qubit[j]);
            return;
        }
        ctx->allocated_occupation_indices_per_qubit[i] = QC_QUBIT_INDEX_BLOCK;
    }

    /* gate_index_of_layer_and_qubits — grow qubit dimension for each layer */
    for (uint32_t lay = 0; lay < ctx->allocated_layer; ++lay) {
        int *new_gi = realloc(ctx->gate_index_of_layer_and_qubits[lay],
                              new_cap * sizeof(int));
        if (!new_gi) return;
        ctx->gate_index_of_layer_and_qubits[lay] = new_gi;
        memset(&ctx->gate_index_of_layer_and_qubits[lay][ctx->allocated_qubits],
               0xFF, (new_cap - ctx->allocated_qubits) * sizeof(int));
    }

    ctx->allocated_qubits = new_cap;
}

/* ====================================================================== */
/* Circuit array growth — layer dimension                                  */
/* ====================================================================== */

void qc_allocate_more_layer(circuit_ctx_t *ctx, size_t min_possible_layer) {
    if (min_possible_layer < ctx->allocated_layer)
        return;

    uint32_t new_cap = ctx->allocated_layer + QC_LAYER_BLOCK;

    /* used_gates_per_layer */
    uint32_t *new_used = realloc(ctx->used_gates_per_layer,
                                 new_cap * sizeof(uint32_t));
    if (!new_used) return;
    ctx->used_gates_per_layer = new_used;
    memset(&ctx->used_gates_per_layer[ctx->allocated_layer], 0,
           QC_LAYER_BLOCK * sizeof(uint32_t));

    /* allocated_gates_per_layer */
    uint32_t *new_alloc = realloc(ctx->allocated_gates_per_layer,
                                  new_cap * sizeof(uint32_t));
    if (!new_alloc) return;
    ctx->allocated_gates_per_layer = new_alloc;
    for (uint32_t i = ctx->allocated_layer; i < new_cap; ++i)
        ctx->allocated_gates_per_layer[i] = QC_GATES_PER_LAYER_BLOCK;

    /* sequence — new layer gate arrays */
    qc_gate_internal_t **new_seq = realloc(ctx->sequence,
                                           new_cap * sizeof(qc_gate_internal_t *));
    if (!new_seq) return;
    ctx->sequence = new_seq;
    for (uint32_t i = ctx->allocated_layer; i < new_cap; ++i) {
        ctx->sequence[i] = malloc(QC_GATES_PER_LAYER_BLOCK * sizeof(qc_gate_internal_t));
        if (!ctx->sequence[i]) {
            for (uint32_t j = ctx->allocated_layer; j < i; ++j)
                free(ctx->sequence[j]);
            return;
        }
    }

    /* gate_index_of_layer_and_qubits — new layer rows */
    int **new_gi = realloc(ctx->gate_index_of_layer_and_qubits,
                           new_cap * sizeof(int *));
    if (!new_gi) return;
    ctx->gate_index_of_layer_and_qubits = new_gi;
    for (uint32_t i = ctx->allocated_layer; i < new_cap; ++i) {
        ctx->gate_index_of_layer_and_qubits[i] =
            malloc(ctx->allocated_qubits * sizeof(int));
        if (!ctx->gate_index_of_layer_and_qubits[i]) {
            for (uint32_t j = ctx->allocated_layer; j < i; ++j)
                free(ctx->gate_index_of_layer_and_qubits[j]);
            return;
        }
        memset(ctx->gate_index_of_layer_and_qubits[i], 0xFF,
               ctx->allocated_qubits * sizeof(int));
    }

    ctx->allocated_layer = new_cap;
}

/* ====================================================================== */
/* Circuit array growth — gates per layer                                  */
/* ====================================================================== */

void qc_allocate_more_gates_per_layer(circuit_ctx_t *ctx, size_t layer, size_t pos) {
    if (pos < ctx->allocated_gates_per_layer[layer])
        return;

    uint32_t new_size = ctx->allocated_gates_per_layer[layer] + QC_GATES_PER_LAYER_BLOCK;
    qc_gate_internal_t *new_seq = realloc(ctx->sequence[layer],
                                          new_size * sizeof(qc_gate_internal_t));
    if (!new_seq) return;
    ctx->sequence[layer] = new_seq;
    ctx->allocated_gates_per_layer[layer] = new_size;
}

/* ====================================================================== */
/* Circuit array growth — occupation indices per qubit                     */
/* ====================================================================== */

void qc_allocate_more_indices_per_qubit(circuit_ctx_t *ctx, int loc) {
    uint32_t uloc = (uint32_t)loc;
    if (ctx->used_occupation_indices_per_qubit[uloc] ==
        ctx->allocated_occupation_indices_per_qubit[uloc]) {
        uint32_t new_cap = ctx->allocated_occupation_indices_per_qubit[uloc] +
                           QC_QUBIT_INDEX_BLOCK;
        size_t *new_arr = realloc(ctx->occupied_layers_of_qubit[uloc],
                                  new_cap * sizeof(size_t));
        if (!new_arr) return;
        ctx->occupied_layers_of_qubit[uloc] = new_arr;
        ctx->allocated_occupation_indices_per_qubit[uloc] = new_cap;
    }
}

/* Qubit allocator lifecycle (qc_allocator_create/destroy/alloc/free) and
 * public qubit API (qc_qubit_alloc/free/is_allocated) moved to
 * qubit_allocator.c (Module 1.4) -- their authoritative home.
 *
 * Statistics wrappers (qc_circuit_num_qubits, qc_circuit_alloc_stats)
 * are in circuit_stats.c (Module 1.7). */
