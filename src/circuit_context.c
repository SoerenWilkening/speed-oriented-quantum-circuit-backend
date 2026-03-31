/**
 * @file circuit_context.c
 * @brief Circuit context lifecycle: create, destroy, reset, and configuration.
 *
 * Implements qc_circuit_create(), qc_circuit_destroy(), qc_circuit_reset(),
 * version queries, and configuration setters/getters.
 *
 * Refactored from monolith init_circuit()/free_circuit() in circuit_allocations.c.
 * All global state replaced by circuit_ctx_t* parameter.
 */

#include "internal.h"

/* ====================================================================== */
/* Public API — circuit lifecycle                                          */
/* ====================================================================== */

QC_API circuit_ctx_t *qc_circuit_create(uint32_t initial_qubits) {
    if (initial_qubits == 0)
        initial_qubits = QC_QUBIT_BLOCK;

    circuit_ctx_t *ctx = calloc(1, sizeof(circuit_ctx_t));
    if (!ctx) return NULL;

    /* Default configuration (matches monolith init_circuit defaults) */
    ctx->toff_decomp        = 0;  /* DONTDECOMPOSETOFFOLI */
    ctx->arithmetic_mode    = 1;  /* ARITH_TOFFOLI */
    ctx->cla_override       = 0;
    ctx->qubit_saving       = 0;
    ctx->toffoli_decompose  = 0;
    ctx->tradeoff_auto_threshold = 4;
    ctx->tradeoff_min_depth = 0;
    ctx->simulate           = 1;  /* Store gates by default */
    ctx->layer_floor        = 0;
    ctx->gate_count         = 0;
    ctx->used               = 0;

    /* Allocate layer-dimension arrays */
    ctx->allocated_layer = QC_LAYER_BLOCK;
    ctx->used_layer      = 0;

    ctx->used_gates_per_layer = calloc(QC_LAYER_BLOCK, sizeof(uint32_t));
    if (!ctx->used_gates_per_layer) goto fail;

    ctx->allocated_gates_per_layer = malloc(QC_LAYER_BLOCK * sizeof(uint32_t));
    if (!ctx->allocated_gates_per_layer) goto fail;
    for (uint32_t i = 0; i < QC_LAYER_BLOCK; ++i)
        ctx->allocated_gates_per_layer[i] = QC_GATES_PER_LAYER_BLOCK;

    /* Gate sequence: [layer][gate_index] */
    ctx->sequence = malloc(QC_LAYER_BLOCK * sizeof(qc_gate_internal_t *));
    if (!ctx->sequence) goto fail;
    for (uint32_t i = 0; i < QC_LAYER_BLOCK; ++i) {
        ctx->sequence[i] = malloc(QC_GATES_PER_LAYER_BLOCK * sizeof(qc_gate_internal_t));
        if (!ctx->sequence[i]) {
            for (uint32_t j = 0; j < i; ++j) free(ctx->sequence[j]);
            free(ctx->sequence);
            ctx->sequence = NULL;
            goto fail;
        }
    }

    /* Spatial index: [layer][qubit] -> gate index or -1 */
    ctx->gate_index_of_layer_and_qubits = malloc(QC_LAYER_BLOCK * sizeof(int *));
    if (!ctx->gate_index_of_layer_and_qubits) goto fail;
    for (uint32_t i = 0; i < QC_LAYER_BLOCK; ++i) {
        ctx->gate_index_of_layer_and_qubits[i] = malloc(initial_qubits * sizeof(int));
        if (!ctx->gate_index_of_layer_and_qubits[i]) {
            for (uint32_t j = 0; j < i; ++j)
                free(ctx->gate_index_of_layer_and_qubits[j]);
            free(ctx->gate_index_of_layer_and_qubits);
            ctx->gate_index_of_layer_and_qubits = NULL;
            goto fail;
        }
        memset(ctx->gate_index_of_layer_and_qubits[i], 0xFF,
               initial_qubits * sizeof(int));
    }

    /* Qubit-dimension arrays */
    ctx->allocated_qubits = initial_qubits;
    ctx->used_qubits      = 0;

    ctx->used_occupation_indices_per_qubit = calloc(initial_qubits, sizeof(uint32_t));
    if (!ctx->used_occupation_indices_per_qubit) goto fail;

    ctx->allocated_occupation_indices_per_qubit = malloc(initial_qubits * sizeof(uint32_t));
    if (!ctx->allocated_occupation_indices_per_qubit) goto fail;

    ctx->occupied_layers_of_qubit = malloc(initial_qubits * sizeof(size_t *));
    if (!ctx->occupied_layers_of_qubit) goto fail;
    for (uint32_t i = 0; i < initial_qubits; ++i) {
        ctx->occupied_layers_of_qubit[i] =
            malloc(QC_QUBIT_INDEX_BLOCK * sizeof(size_t));
        if (!ctx->occupied_layers_of_qubit[i]) {
            for (uint32_t j = 0; j < i; ++j)
                free(ctx->occupied_layers_of_qubit[j]);
            free(ctx->occupied_layers_of_qubit);
            ctx->occupied_layers_of_qubit = NULL;
            goto fail;
        }
        ctx->allocated_occupation_indices_per_qubit[i] = QC_QUBIT_INDEX_BLOCK;
    }

    /* Qubit allocator */
    ctx->allocator = qc_allocator_create(initial_qubits);
    if (!ctx->allocator) goto fail;

    return ctx;

fail:
    qc_circuit_destroy(ctx);
    return NULL;
}

QC_API void qc_circuit_destroy(circuit_ctx_t *ctx) {
    if (!ctx) return;

    /* Free gate storage */
    if (ctx->sequence) {
        for (uint32_t i = 0; i < ctx->allocated_layer; ++i) {
            if (ctx->sequence[i]) {
                /* Free large_control arrays in used gates */
                if (i < ctx->used_layer) {
                    for (uint32_t g = 0; g < ctx->used_gates_per_layer[i]; ++g) {
                        if (ctx->sequence[i][g].NumControls > QC_MAX_INLINE_CONTROLS &&
                            ctx->sequence[i][g].large_control != NULL) {
                            free(ctx->sequence[i][g].large_control);
                        }
                    }
                }
                free(ctx->sequence[i]);
            }
        }
        free(ctx->sequence);
    }

    if (ctx->gate_index_of_layer_and_qubits) {
        for (uint32_t i = 0; i < ctx->allocated_layer; ++i)
            free(ctx->gate_index_of_layer_and_qubits[i]);
        free(ctx->gate_index_of_layer_and_qubits);
    }

    free(ctx->used_gates_per_layer);
    free(ctx->allocated_gates_per_layer);

    /* Free qubit-dimension arrays */
    if (ctx->occupied_layers_of_qubit) {
        for (uint32_t i = 0; i < ctx->allocated_qubits; ++i)
            free(ctx->occupied_layers_of_qubit[i]);
        free(ctx->occupied_layers_of_qubit);
    }

    free(ctx->used_occupation_indices_per_qubit);
    free(ctx->allocated_occupation_indices_per_qubit);

    /* Destroy allocator */
    if (ctx->allocator)
        qc_allocator_destroy(ctx->allocator);

    free(ctx);
}

QC_API qc_error_t qc_circuit_reset(circuit_ctx_t *ctx) {
    if (!ctx) return QC_ERR_NULL;

    /* Free large_control arrays in used gates, then zero out usage counts */
    for (uint32_t i = 0; i < ctx->used_layer; ++i) {
        for (uint32_t g = 0; g < ctx->used_gates_per_layer[i]; ++g) {
            if (ctx->sequence[i][g].NumControls > QC_MAX_INLINE_CONTROLS &&
                ctx->sequence[i][g].large_control != NULL) {
                free(ctx->sequence[i][g].large_control);
                ctx->sequence[i][g].large_control = NULL;
            }
        }
        ctx->used_gates_per_layer[i] = 0;
    }
    ctx->used_layer = 0;
    ctx->gate_count = 0;
    ctx->used       = 0;

    /* Reset qubit occupancy tracking */
    for (uint32_t i = 0; i < ctx->allocated_qubits; ++i)
        ctx->used_occupation_indices_per_qubit[i] = 0;

    /* Reset spatial index to -1 */
    for (uint32_t i = 0; i < ctx->allocated_layer; ++i)
        memset(ctx->gate_index_of_layer_and_qubits[i], 0xFF,
               ctx->allocated_qubits * sizeof(int));

    ctx->used_qubits = 0;

    /* Reset allocator */
    if (ctx->allocator) {
        qc_allocator_destroy(ctx->allocator);
        ctx->allocator = qc_allocator_create(ctx->allocated_qubits);
    }

    return QC_OK;
}

/* ====================================================================== */
/* Version query                                                           */
/* ====================================================================== */

QC_API const char *qc_version_string(void) {
    return QC_VERSION_STRING;
}

QC_API int qc_version_number(void) {
    return QC_VERSION_NUMBER;
}

/* ====================================================================== */
/* Configuration setters/getters                                           */
/* ====================================================================== */

QC_API qc_error_t qc_circuit_set_arith_mode(circuit_ctx_t *ctx, qc_arith_mode_t mode) {
    if (!ctx) return QC_ERR_NULL;
    ctx->arithmetic_mode = (int)mode;
    return QC_OK;
}

QC_API qc_arith_mode_t qc_circuit_get_arith_mode(const circuit_ctx_t *ctx) {
    if (!ctx) return QC_ARITH_QFT;
    return (qc_arith_mode_t)ctx->arithmetic_mode;
}

QC_API qc_error_t qc_circuit_set_toffoli_decompose(circuit_ctx_t *ctx, bool enable) {
    if (!ctx) return QC_ERR_NULL;
    ctx->toffoli_decompose = enable ? 1 : 0;
    return QC_OK;
}

QC_API qc_error_t qc_circuit_set_qubit_saving(circuit_ctx_t *ctx, bool enable) {
    if (!ctx) return QC_ERR_NULL;
    ctx->qubit_saving = enable ? 1 : 0;
    return QC_OK;
}

QC_API qc_error_t qc_circuit_set_cla_override(circuit_ctx_t *ctx, int override_val) {
    if (!ctx) return QC_ERR_NULL;
    ctx->cla_override = override_val;
    return QC_OK;
}

QC_API qc_error_t qc_circuit_set_simulate(circuit_ctx_t *ctx, bool enable) {
    if (!ctx) return QC_ERR_NULL;
    ctx->simulate = enable ? 1 : 0;
    return QC_OK;
}

QC_API qc_error_t qc_circuit_set_layer_floor(circuit_ctx_t *ctx, uint32_t layer) {
    if (!ctx) return QC_ERR_NULL;
    ctx->layer_floor = layer;
    return QC_OK;
}
