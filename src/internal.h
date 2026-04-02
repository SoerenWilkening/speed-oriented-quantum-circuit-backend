/**
 * @file internal.h
 * @brief Internal definitions for the circuit-c-backend library.
 *
 * This header is NOT part of the public API. It defines the internal layout
 * of circuit_ctx_t and declares internal helper functions shared across
 * translation units within the library.
 *
 * External consumers must only use quantum_circuit.h (the public header).
 */

#ifndef QC_INTERNAL_H
#define QC_INTERNAL_H

#include "quantum_circuit.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Internal constants                                                      */
/* ====================================================================== */

#define QC_QUBIT_BLOCK          128
#define QC_LAYER_BLOCK          128
#define QC_GATES_PER_LAYER_BLOCK 32
#define QC_QUBIT_INDEX_BLOCK    128
#define QC_MAX_QUBITS           8000
#define QC_MAX_INLINE_CONTROLS  2
#define QC_ALLOCATOR_MAX_QUBITS 8192

/* ====================================================================== */
/* Internal gate representation                                            */
/* ====================================================================== */

/**
 * @brief Standard gate enum used internally (monolith-compatible).
 */
typedef enum {
    QC_IGATE_X       = 0,
    QC_IGATE_Y       = 1,
    QC_IGATE_Z       = 2,
    QC_IGATE_R       = 3,
    QC_IGATE_H       = 4,
    QC_IGATE_RX      = 5,
    QC_IGATE_RY      = 6,
    QC_IGATE_RZ      = 7,
    QC_IGATE_P       = 8,
    QC_IGATE_M       = 9,
    QC_IGATE_T       = 10,
    QC_IGATE_TDG     = 11
} qc_igate_t;

/**
 * @brief Internal gate structure (matches monolith gate_t layout).
 */
typedef struct {
    uint32_t Control[QC_MAX_INLINE_CONTROLS];
    uint32_t *large_control;   /**< Heap-allocated controls for n > 2 */
    uint32_t NumControls;
    qc_igate_t Gate;
    double GateValue;
    uint32_t Target;
    uint32_t NumBasisGates;
} qc_gate_internal_t;

/* ====================================================================== */
/* Qubit allocator (embedded in context)                                   */
/* ====================================================================== */

typedef struct {
    uint32_t start;
    uint32_t count;
} qc_qubit_block_t;

typedef struct {
    uint32_t peak_allocated;
    uint32_t total_allocations;
    uint32_t total_deallocations;
    uint32_t current_in_use;
    uint32_t ancilla_allocations;
} qc_alloc_stats_internal_t;

typedef struct {
    uint32_t *indices;
    uint32_t capacity;
    uint32_t next_qubit;
    qc_qubit_block_t *freed_blocks;
    uint32_t freed_block_count;
    uint32_t freed_block_capacity;
    qc_alloc_stats_internal_t stats;
} qc_allocator_t;

/* ====================================================================== */
/* Circuit context (internal layout of the opaque circuit_ctx_t)           */
/* ====================================================================== */

struct circuit_ctx {
    /* Gate storage: sequence[layer][gate_index] */
    qc_gate_internal_t **sequence;
    uint32_t used_layer;
    uint32_t allocated_layer;
    uint32_t *allocated_gates_per_layer;
    uint32_t *used_gates_per_layer;

    /* Spatial index: gate_index_of_layer_and_qubits[layer][qubit] = gate idx or -1 */
    int **gate_index_of_layer_and_qubits;

    /* Qubit occupation tracking */
    size_t **occupied_layers_of_qubit;
    uint32_t *allocated_occupation_indices_per_qubit;
    uint32_t *used_occupation_indices_per_qubit;

    /* Qubit bookkeeping */
    uint32_t allocated_qubits;
    uint32_t used_qubits;
    size_t   used;           /**< Total gate count (stored gates) */

    /* Configuration */
    int toff_decomp;
    int arithmetic_mode;     /**< 0 = QFT, 1 = Toffoli */
    int cla_override;
    int qubit_saving;
    int toffoli_decompose;
    int tradeoff_auto_threshold;
    int tradeoff_min_depth;
    int simulate;            /**< 0 = count-only, 1 = store gates */
    size_t gate_count;       /**< Running count (both modes) */
    uint32_t layer_floor;    /**< Minimum layer for gate placement */

    /* Qubit allocator */
    qc_allocator_t *allocator;
};

/* ====================================================================== */
/* Internal helper: get control qubit (handles large_control)              */
/* ====================================================================== */

static inline uint32_t qc_get_control(const qc_gate_internal_t *g, int i) {
    if (g->NumControls > QC_MAX_INLINE_CONTROLS && g->large_control != NULL) {
        return g->large_control[i];
    }
    return g->Control[i];
}

/* ====================================================================== */
/* Internal helper: max qubit touched by a gate                            */
/* ====================================================================== */

static inline uint32_t qc_max_qubit(const qc_gate_internal_t *g) {
    uint32_t m = g->Target;
    for (uint32_t i = 0; i < g->NumControls; i++) {
        uint32_t c = qc_get_control(g, (int)i);
        if (c > m) m = c;
    }
    return m;
}

/* ====================================================================== */
/* Internal helper: min qubit touched by a gate (Module 1.7)               */
/* ====================================================================== */

static inline uint32_t qc_min_qubit(const qc_gate_internal_t *g) {
    uint32_t m = g->Target;
    for (uint32_t i = 0; i < g->NumControls; i++) {
        uint32_t c = qc_get_control(g, (int)i);
        if (c < m) m = c;
    }
    return m;
}

/* ====================================================================== */
/* Internal function declarations -- circuit_allocations                   */
/* ====================================================================== */

/** @brief Expand qubit-related arrays if gate touches a higher qubit. */
void qc_allocate_more_qubits(circuit_ctx_t *ctx, const qc_gate_internal_t *g);

/** @brief Expand layer arrays if min_possible_layer exceeds allocation. */
void qc_allocate_more_layer(circuit_ctx_t *ctx, size_t min_possible_layer);

/** @brief Expand gate slots within a specific layer. */
void qc_allocate_more_gates_per_layer(circuit_ctx_t *ctx, size_t layer, size_t pos);

/** @brief Expand occupation index slots for a qubit. */
void qc_allocate_more_indices_per_qubit(circuit_ctx_t *ctx, int loc);

/* ====================================================================== */
/* Internal function declarations -- gate helpers                          */
/* ====================================================================== */

/** @brief Check if two gates are inverses of each other. */
bool qc_gates_are_inverse(const qc_gate_internal_t *g1, const qc_gate_internal_t *g2);

/** @brief Check if two gates commute. */
bool qc_gates_commute(const qc_gate_internal_t *g1, const qc_gate_internal_t *g2);

/* ====================================================================== */
/* Internal function declarations -- optimizer (layer assignment)          */
/* ====================================================================== */

/**
 * @brief Add a gate to the circuit with automatic layer assignment and merging.
 *
 * Main entry point for gate insertion. Handles layer assignment, inverse
 * cancellation, and collision detection.
 */
void qc_add_gate(circuit_ctx_t *ctx, qc_gate_internal_t *g);

/** @brief Find the largest occupied layer below a comparison layer. */
size_t qc_smallest_layer_below_comp(circuit_ctx_t *ctx, uint32_t qubit, size_t compar);

/** @brief Determine the minimum possible layer for a gate. */
size_t qc_minimum_layer(circuit_ctx_t *ctx, qc_gate_internal_t *g, size_t compared_layer);

/** @brief Merge (cancel) an inverse gate pair. */
int qc_merge_gates(circuit_ctx_t *ctx, qc_gate_internal_t *g,
                   size_t min_possible_layer, int gate_index);

/** @brief Find colliding gates at a layer boundary. */
void qc_colliding_gates(circuit_ctx_t *ctx, qc_gate_internal_t *g,
                        size_t min_possible_layer, int *gate_index,
                        qc_gate_internal_t **coll);

/** @brief Record a gate's layer occupancy in the spatial index. */
void qc_apply_layer(circuit_ctx_t *ctx, qc_gate_internal_t *g, size_t min_possible_layer);

/** @brief Append a gate to a specific layer. */
void qc_append_gate(circuit_ctx_t *ctx, qc_gate_internal_t *g, size_t min_possible_layer);

/* ====================================================================== */
/* Internal function declarations -- context lifecycle                     */
/* ====================================================================== */

/** @brief Create allocator (used by circuit_create). */
qc_allocator_t *qc_allocator_create(uint32_t initial_capacity);

/** @brief Destroy allocator. */
void qc_allocator_destroy(qc_allocator_t *alloc);

/** @brief Allocate contiguous qubits. Returns start index or (uint32_t)-1. */
uint32_t qc_allocator_alloc(qc_allocator_t *alloc, uint32_t count, bool is_ancilla);

/** @brief Free contiguous qubits. Returns 0 on success, -1 on error. */
int qc_allocator_free(qc_allocator_t *alloc, uint32_t start, uint32_t count);

/** @brief Check if a qubit is currently allocated. */
bool qc_allocator_is_allocated(const qc_allocator_t *alloc, uint32_t qubit);

/** @brief Get allocator stats. */
qc_alloc_stats_internal_t qc_allocator_get_stats(const qc_allocator_t *alloc);

/* ====================================================================== */
/* Internal sequence structure                                             */
/* ====================================================================== */

/**
 * @brief Gate sequence (pre-built instruction, mirrors monolith sequence_t).
 *
 * A sequence represents a pre-built sub-circuit (e.g., QFT, addition) that
 * can be applied to specific qubits via qc_run_instruction().
 *
 * Note: qc_sequence_t is forward-declared in the public header
 * (quantum_circuit.h) as `typedef struct qc_sequence qc_sequence_t;`.
 * This definition provides the struct body.
 */
struct qc_sequence {
    qc_gate_internal_t **seq;    /**< [layer][gate_index] */
    uint32_t num_layer;          /**< Allocated layer count */
    uint32_t used_layer;         /**< Used layer count */
    uint32_t *gates_per_layer;   /**< [layer] gate count */
    uint32_t total_gate_count;   /**< Pre-computed total gate count */
    uint32_t total_qubits;       /**< Total virtual qubits (register + ancillae), 0 = unknown */
};

/* ====================================================================== */
/* Internal function declarations -- execution (Module 1.5)                */
/* ====================================================================== */

/**
 * @brief Compute total_gate_count as sum of gates_per_layer.
 *
 * Call after sequence is fully built (all layers populated).
 */
void qc_sequence_compute_total_gate_count(qc_sequence_t *seq);

/**
 * @brief Apply a pre-built sequence to specific qubits in the circuit.
 *
 * Maps sequence qubit indices through qubit_array[], handles inversion
 * (reversed layer order + negated gate values for rotation gates).
 * When ctx->simulate is 0, counts gates in ctx->gate_count without storage.
 *
 * @param ctx         Circuit context (receives the gates).
 * @param seq         Pre-built gate sequence to apply (NULL is no-op).
 * @param qubit_array Qubit mapping array.
 * @param invert      0 = normal, 1 = apply in reverse with inverted rotations.
 */
void qc_run_instruction(circuit_ctx_t *ctx, qc_sequence_t *seq,
                        const uint32_t qubit_array[], int invert);

/**
 * @brief Reverse gates in a layer range [start_layer, end_layer) in LIFO order.
 *
 * Used for automatic uncomputation of intermediate quantum values.
 * Appends the inverted gates to the end of the circuit.
 *
 * @param ctx         Circuit context.
 * @param start_layer First layer (inclusive).
 * @param end_layer   Last layer (exclusive).
 */
void qc_reverse_circuit_range(circuit_ctx_t *ctx, int start_layer, int end_layer);

/* ====================================================================== */
/* Internal function declarations -- toffoli_helpers (Module 1.9)          */
/* ====================================================================== */

/* Backward-compatibility macros: old names map to new public names */
#define qc_toffoli_seq_alloc qc_sequence_alloc
#define qc_toffoli_seq_free  qc_sequence_free

/* ====================================================================== */
/* Internal function declarations -- integer (Module 1.12)                 */
/* ====================================================================== */

/** @brief Compute n-bit two's complement binary representation (MSB-first). */
int *qc_two_complement(int64_t x, int n);

/* ====================================================================== */
/* Internal function declarations -- toffoli_multiplication (Module 1.11)  */
/* ====================================================================== */

/** @brief Emit CCX or its Clifford+T decomposition. */
void qc_emit_ccx_or_decomp(circuit_ctx_t *ctx, uint32_t target,
                            uint32_t ctrl1, uint32_t ctrl2);

/** @brief Dynamic CDKM QQ addition: b += a, emitted directly into ctx. */
void qc_dynamic_qq_add(circuit_ctx_t *ctx, const uint32_t *a,
                        const uint32_t *b, uint32_t width);

/** @brief Dynamic CDKM QQ subtraction: b -= a. */
void qc_dynamic_qq_sub(circuit_ctx_t *ctx, const uint32_t *a,
                        const uint32_t *b, uint32_t width);

/** @brief Dynamic controlled CDKM QQ addition: b += a, controlled. */
void qc_dynamic_cqq_add(circuit_ctx_t *ctx, const uint32_t *a,
                         const uint32_t *b, uint32_t width,
                         uint32_t control);

/** @brief Dynamic controlled CDKM QQ subtraction: b -= a, controlled. */
void qc_dynamic_cqq_sub(circuit_ctx_t *ctx, const uint32_t *a,
                         const uint32_t *b, uint32_t width,
                         uint32_t control);

/** @brief Dynamic CQ addition: target += value. */
void qc_dynamic_cq_add(circuit_ctx_t *ctx, const uint32_t *target,
                        uint32_t width, int64_t value);

/** @brief Dynamic controlled CQ addition: target += value, controlled. */
void qc_dynamic_ccq_add(circuit_ctx_t *ctx, const uint32_t *target,
                         uint32_t width, int64_t value, uint32_t control);

/* ====================================================================== */
/* Internal function declarations -- arithmetic dispatch (Module 1.13)     */
/* ====================================================================== */

/** @brief Dispatched QQ addition: routes by ctx->arithmetic_mode. */
qc_error_t qc_dispatch_qq_add(circuit_ctx_t *ctx, const uint32_t *a,
                                const uint32_t *b, uint32_t width);

/** @brief Dispatched CQ addition: routes by ctx->arithmetic_mode. */
qc_error_t qc_dispatch_cq_add(circuit_ctx_t *ctx, const uint32_t *target,
                                uint32_t width, int64_t value);

/** @brief Dispatched controlled QQ addition. */
qc_error_t qc_dispatch_cqq_add(circuit_ctx_t *ctx, const uint32_t *a,
                                 const uint32_t *b, uint32_t width,
                                 uint32_t control);

/** @brief Dispatched controlled CQ addition. */
qc_error_t qc_dispatch_ccq_add(circuit_ctx_t *ctx, const uint32_t *target,
                                 uint32_t width, int64_t value,
                                 uint32_t control);

/** @brief Dispatched QQ subtraction. */
qc_error_t qc_dispatch_qq_sub(circuit_ctx_t *ctx, const uint32_t *a,
                                const uint32_t *b, uint32_t width);

/** @brief Dispatched CQ subtraction. */
qc_error_t qc_dispatch_cq_sub(circuit_ctx_t *ctx, const uint32_t *target,
                                uint32_t width, int64_t value);

/** @brief Dispatched QQ multiplication. */
qc_error_t qc_dispatch_qq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                const uint32_t *a, const uint32_t *b,
                                uint32_t width);

/** @brief Dispatched CQ multiplication. */
qc_error_t qc_dispatch_cq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                const uint32_t *target, uint32_t width,
                                int64_t value);

/** @brief Dispatched CQ divmod (Toffoli only). */
qc_error_t qc_dispatch_divmod_cq(circuit_ctx_t *ctx,
                                   const uint32_t *dividend,
                                   uint32_t dividend_bits, int64_t divisor,
                                   const uint32_t *quotient,
                                   const uint32_t *remainder);

/** @brief Dispatched QQ divmod (Toffoli only). */
qc_error_t qc_dispatch_divmod_qq(circuit_ctx_t *ctx,
                                   const uint32_t *dividend,
                                   uint32_t dividend_bits,
                                   const uint32_t *divisor,
                                   uint32_t divisor_bits,
                                   const uint32_t *quotient,
                                   const uint32_t *remainder);

/** @brief Dispatched modular reduction (Toffoli only). */
qc_error_t qc_dispatch_mod_reduce(circuit_ctx_t *ctx, const uint32_t *value,
                                    uint32_t value_bits, int64_t modulus);

/** @brief Dispatched modular CQ addition (Toffoli only). */
qc_error_t qc_dispatch_mod_add_cq(circuit_ctx_t *ctx, const uint32_t *value,
                                    uint32_t value_bits, int64_t addend,
                                    int64_t modulus);

/** @brief Dispatched modular QQ addition (Toffoli only). */
qc_error_t qc_dispatch_mod_add_qq(circuit_ctx_t *ctx, const uint32_t *value,
                                    uint32_t value_bits,
                                    const uint32_t *other,
                                    uint32_t other_bits, int64_t modulus);

/** @brief Dispatched modular CQ multiplication (Toffoli only). */
qc_error_t qc_dispatch_mod_mul_cq(circuit_ctx_t *ctx, const uint32_t *value,
                                    uint32_t value_bits,
                                    const uint32_t *result,
                                    uint32_t result_bits, int64_t multiplier,
                                    int64_t modulus);

#endif /* QC_INTERNAL_H */
