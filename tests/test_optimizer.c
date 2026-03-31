/**
 * @file test_optimizer.c
 * @brief Tests for optimizer.c and circuit_optimizer.c (Module 1.6).
 *
 * Tests layer optimization and post-construction optimization passes
 * using the refactored circuit_ctx_t* API.
 *
 * Max qubits in tests: 17 (per project constraint).
 */

#define _USE_MATH_DEFINES
#include "../src/internal.h"
#include <assert.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Minimal stubs for dependencies not yet implemented (Modules 1.3-1.5)   */
/* ====================================================================== */

/*
 * These stubs provide just enough functionality for the optimizer tests
 * to run. They will be replaced by real implementations from Modules
 * 1.3 (circuit core), 1.4 (qubit allocator), and 1.5 (execution).
 */

/* --- Allocator stubs --- */

qc_allocator_t *qc_allocator_create(uint32_t initial_capacity) {
    qc_allocator_t *alloc = calloc(1, sizeof(qc_allocator_t));
    if (!alloc) return NULL;
    alloc->capacity = initial_capacity;
    alloc->next_qubit = 0;
    return alloc;
}

void qc_allocator_destroy(qc_allocator_t *alloc) {
    if (!alloc) return;
    free(alloc->freed_blocks);
    free(alloc->indices);
    free(alloc);
}

uint32_t qc_allocator_alloc(qc_allocator_t *alloc, uint32_t count, bool is_ancilla) {
    (void)is_ancilla;
    if (!alloc) return (uint32_t)-1;
    uint32_t start = alloc->next_qubit;
    alloc->next_qubit += count;
    alloc->stats.total_allocations += count;
    alloc->stats.current_in_use += count;
    if (alloc->next_qubit > alloc->stats.peak_allocated)
        alloc->stats.peak_allocated = alloc->next_qubit;
    return start;
}

int qc_allocator_free(qc_allocator_t *alloc, uint32_t start, uint32_t count) {
    (void)start;
    if (!alloc) return -1;
    alloc->stats.total_deallocations += count;
    alloc->stats.current_in_use -= count;
    return 0;
}

bool qc_allocator_is_allocated(const qc_allocator_t *alloc, uint32_t qubit) {
    if (!alloc) return false;
    return qubit < alloc->next_qubit;
}

qc_alloc_stats_internal_t qc_allocator_get_stats(const qc_allocator_t *alloc) {
    if (!alloc) {
        qc_alloc_stats_internal_t empty = {0};
        return empty;
    }
    return alloc->stats;
}

/* --- Circuit lifecycle stubs --- */

circuit_ctx_t *qc_circuit_create(uint32_t initial_qubits) {
    circuit_ctx_t *ctx = calloc(1, sizeof(circuit_ctx_t));
    if (!ctx) return NULL;

    uint32_t qcap = (initial_qubits > 0) ? initial_qubits : QC_QUBIT_BLOCK;

    ctx->allocator = qc_allocator_create(qcap);
    if (!ctx->allocator) { free(ctx); return NULL; }

    ctx->allocated_layer = QC_LAYER_BLOCK;
    ctx->used_gates_per_layer = calloc(QC_LAYER_BLOCK, sizeof(uint32_t));
    ctx->allocated_gates_per_layer = malloc(QC_LAYER_BLOCK * sizeof(uint32_t));
    ctx->sequence = malloc(QC_LAYER_BLOCK * sizeof(qc_gate_internal_t *));
    ctx->gate_index_of_layer_and_qubits = malloc(QC_LAYER_BLOCK * sizeof(int *));

    if (!ctx->used_gates_per_layer || !ctx->allocated_gates_per_layer ||
        !ctx->sequence || !ctx->gate_index_of_layer_and_qubits) {
        /* Cleanup on failure (simplified) */
        free(ctx->used_gates_per_layer);
        free(ctx->allocated_gates_per_layer);
        free(ctx->sequence);
        free(ctx->gate_index_of_layer_and_qubits);
        qc_allocator_destroy(ctx->allocator);
        free(ctx);
        return NULL;
    }

    for (uint32_t i = 0; i < QC_LAYER_BLOCK; ++i) {
        ctx->allocated_gates_per_layer[i] = QC_GATES_PER_LAYER_BLOCK;
        ctx->sequence[i] = malloc(QC_GATES_PER_LAYER_BLOCK * sizeof(qc_gate_internal_t));
        ctx->gate_index_of_layer_and_qubits[i] = malloc(qcap * sizeof(int));
        if (ctx->gate_index_of_layer_and_qubits[i])
            memset(ctx->gate_index_of_layer_and_qubits[i], 0xFF, qcap * sizeof(int));
    }

    ctx->allocated_qubits = qcap;
    ctx->used_occupation_indices_per_qubit = calloc(qcap, sizeof(uint32_t));
    ctx->allocated_occupation_indices_per_qubit = malloc(qcap * sizeof(uint32_t));
    ctx->occupied_layers_of_qubit = malloc(qcap * sizeof(size_t *));

    if (ctx->occupied_layers_of_qubit && ctx->allocated_occupation_indices_per_qubit) {
        for (uint32_t i = 0; i < qcap; ++i) {
            ctx->occupied_layers_of_qubit[i] = malloc(QC_QUBIT_INDEX_BLOCK * sizeof(size_t));
            ctx->allocated_occupation_indices_per_qubit[i] = QC_QUBIT_INDEX_BLOCK;
        }
    }

    ctx->arithmetic_mode = 1; /* Toffoli default */
    ctx->simulate = 1;        /* Store gates for testing */

    return ctx;
}

void qc_circuit_destroy(circuit_ctx_t *ctx) {
    if (!ctx) return;

    qc_allocator_destroy(ctx->allocator);

    for (uint32_t i = 0; i < ctx->allocated_layer; ++i) {
        /* Free large_control for MCX gates */
        if (i < ctx->used_layer) {
            for (uint32_t g = 0; g < ctx->used_gates_per_layer[i]; ++g) {
                if (ctx->sequence[i][g].NumControls > QC_MAX_INLINE_CONTROLS &&
                    ctx->sequence[i][g].large_control != NULL) {
                    free(ctx->sequence[i][g].large_control);
                }
            }
        }
        free(ctx->sequence[i]);
        free(ctx->gate_index_of_layer_and_qubits[i]);
    }
    free(ctx->sequence);
    free(ctx->gate_index_of_layer_and_qubits);

    for (uint32_t i = 0; i < ctx->allocated_qubits; ++i)
        free(ctx->occupied_layers_of_qubit[i]);
    free(ctx->occupied_layers_of_qubit);

    free(ctx->used_gates_per_layer);
    free(ctx->allocated_gates_per_layer);
    free(ctx->used_occupation_indices_per_qubit);
    free(ctx->allocated_occupation_indices_per_qubit);

    free(ctx);
}

/* --- Allocation helper stubs --- */

void qc_allocate_more_qubits(circuit_ctx_t *ctx, const qc_gate_internal_t *g) {
    if (!ctx || !g) return;
    uint32_t max = qc_max_qubit(g);
    if (max >= ctx->used_qubits)
        ctx->used_qubits = max;
    max++;
    if (max <= ctx->allocated_qubits)
        return;

    uint32_t new_cap = max + QC_QUBIT_BLOCK;

    uint32_t *new_used_occ = realloc(ctx->used_occupation_indices_per_qubit,
                                      new_cap * sizeof(uint32_t));
    if (!new_used_occ) return;
    ctx->used_occupation_indices_per_qubit = new_used_occ;
    memset(ctx->used_occupation_indices_per_qubit + ctx->allocated_qubits, 0,
           (new_cap - ctx->allocated_qubits) * sizeof(uint32_t));

    size_t **new_occ = realloc(ctx->occupied_layers_of_qubit, new_cap * sizeof(size_t *));
    if (!new_occ) return;
    ctx->occupied_layers_of_qubit = new_occ;

    uint32_t *new_alloc_occ = realloc(ctx->allocated_occupation_indices_per_qubit,
                                       new_cap * sizeof(uint32_t));
    if (!new_alloc_occ) return;
    ctx->allocated_occupation_indices_per_qubit = new_alloc_occ;

    for (uint32_t i = ctx->allocated_qubits; i < new_cap; ++i) {
        ctx->occupied_layers_of_qubit[i] = malloc(QC_QUBIT_INDEX_BLOCK * sizeof(size_t));
        ctx->allocated_occupation_indices_per_qubit[i] = QC_QUBIT_INDEX_BLOCK;
    }

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

void qc_allocate_more_layer(circuit_ctx_t *ctx, size_t min_possible_layer) {
    if (!ctx || min_possible_layer < ctx->allocated_layer)
        return;

    uint32_t new_cap = ctx->allocated_layer + QC_LAYER_BLOCK;

    uint32_t *new_used = realloc(ctx->used_gates_per_layer, new_cap * sizeof(uint32_t));
    if (!new_used) return;
    ctx->used_gates_per_layer = new_used;
    memset(&ctx->used_gates_per_layer[ctx->allocated_layer], 0,
           QC_LAYER_BLOCK * sizeof(uint32_t));

    uint32_t *new_alloc = realloc(ctx->allocated_gates_per_layer, new_cap * sizeof(uint32_t));
    if (!new_alloc) return;
    ctx->allocated_gates_per_layer = new_alloc;
    for (uint32_t i = ctx->allocated_layer; i < new_cap; ++i)
        ctx->allocated_gates_per_layer[i] = QC_GATES_PER_LAYER_BLOCK;

    qc_gate_internal_t **new_seq = realloc(ctx->sequence,
                                           new_cap * sizeof(qc_gate_internal_t *));
    if (!new_seq) return;
    ctx->sequence = new_seq;
    for (uint32_t i = ctx->allocated_layer; i < new_cap; ++i)
        ctx->sequence[i] = malloc(QC_GATES_PER_LAYER_BLOCK * sizeof(qc_gate_internal_t));

    int **new_gi = realloc(ctx->gate_index_of_layer_and_qubits, new_cap * sizeof(int *));
    if (!new_gi) return;
    ctx->gate_index_of_layer_and_qubits = new_gi;
    for (uint32_t i = ctx->allocated_layer; i < new_cap; ++i) {
        ctx->gate_index_of_layer_and_qubits[i] = malloc(ctx->allocated_qubits * sizeof(int));
        if (ctx->gate_index_of_layer_and_qubits[i])
            memset(ctx->gate_index_of_layer_and_qubits[i], 0xFF,
                   ctx->allocated_qubits * sizeof(int));
    }

    ctx->allocated_layer = new_cap;
}

void qc_allocate_more_gates_per_layer(circuit_ctx_t *ctx, size_t layer, size_t pos) {
    if (!ctx || pos < ctx->allocated_gates_per_layer[layer])
        return;

    uint32_t new_size = ctx->allocated_gates_per_layer[layer] + QC_GATES_PER_LAYER_BLOCK;
    qc_gate_internal_t *new_seq = realloc(ctx->sequence[layer],
                                          new_size * sizeof(qc_gate_internal_t));
    if (!new_seq) return;
    ctx->sequence[layer] = new_seq;
    ctx->allocated_gates_per_layer[layer] = new_size;
}

void qc_allocate_more_indices_per_qubit(circuit_ctx_t *ctx, int loc) {
    if (!ctx) return;
    if (ctx->used_occupation_indices_per_qubit[loc] ==
        ctx->allocated_occupation_indices_per_qubit[loc]) {
        uint32_t new_cap = ctx->allocated_occupation_indices_per_qubit[loc] +
                           QC_QUBIT_INDEX_BLOCK;
        size_t *new_occ = realloc(ctx->occupied_layers_of_qubit[loc],
                                  new_cap * sizeof(size_t));
        if (!new_occ) return;
        ctx->occupied_layers_of_qubit[loc] = new_occ;
        ctx->allocated_occupation_indices_per_qubit[loc] = new_cap;
    }
}

/* --- Gate helper stubs --- */

bool qc_gates_are_inverse(const qc_gate_internal_t *g1, const qc_gate_internal_t *g2) {
    if (!g1 || !g2) return false;
    if (g1->Target != g2->Target) return false;
    if (g1->NumControls != g2->NumControls) return false;

    /* Check controls match */
    for (uint32_t i = 0; i < g1->NumControls; i++) {
        if (qc_get_control(g1, (int)i) != qc_get_control(g2, (int)i))
            return false;
    }

    /* Self-inverse gates: X, Y, Z, H, CX, CCX, SWAP */
    if (g1->Gate == g2->Gate) {
        switch (g1->Gate) {
        case QC_IGATE_X:
        case QC_IGATE_Y:
        case QC_IGATE_Z:
        case QC_IGATE_H:
            return true;
        default:
            break;
        }
    }

    /* T and Tdg are inverses */
    if ((g1->Gate == QC_IGATE_T && g2->Gate == QC_IGATE_TDG) ||
        (g1->Gate == QC_IGATE_TDG && g2->Gate == QC_IGATE_T))
        return true;

    /* P(theta) and P(-theta) are inverses */
    if (g1->Gate == QC_IGATE_P && g2->Gate == QC_IGATE_P) {
        double sum = g1->GateValue + g2->GateValue;
        if (fabs(sum) < 1e-10 || fabs(fabs(sum) - 2.0 * M_PI) < 1e-10)
            return true;
    }

    return false;
}

bool qc_gates_commute(const qc_gate_internal_t *g1, const qc_gate_internal_t *g2) {
    if (!g1 || !g2) return false;
    /* Simple check: gates on different qubits commute */
    bool share_qubit = false;
    if (g1->Target == g2->Target) share_qubit = true;
    for (uint32_t i = 0; !share_qubit && i < g1->NumControls; i++) {
        uint32_t c = qc_get_control(g1, (int)i);
        if (c == g2->Target) share_qubit = true;
        for (uint32_t j = 0; !share_qubit && j < g2->NumControls; j++) {
            if (c == qc_get_control(g2, (int)j)) share_qubit = true;
        }
    }
    for (uint32_t i = 0; !share_qubit && i < g2->NumControls; i++) {
        if (qc_get_control(g2, (int)i) == g1->Target) share_qubit = true;
    }
    return !share_qubit;
}

/* ====================================================================== */
/* Helper to build gates for testing                                       */
/* ====================================================================== */

static qc_gate_internal_t make_x_gate(uint32_t target) {
    qc_gate_internal_t g = {0};
    g.Gate = QC_IGATE_X;
    g.Target = target;
    g.NumControls = 0;
    g.large_control = NULL;
    return g;
}

static qc_gate_internal_t make_h_gate(uint32_t target) {
    qc_gate_internal_t g = {0};
    g.Gate = QC_IGATE_H;
    g.Target = target;
    g.NumControls = 0;
    g.large_control = NULL;
    return g;
}

static qc_gate_internal_t make_cx_gate(uint32_t control, uint32_t target) {
    qc_gate_internal_t g = {0};
    g.Gate = QC_IGATE_X;
    g.Target = target;
    g.NumControls = 1;
    g.Control[0] = control;
    g.large_control = NULL;
    return g;
}

static qc_gate_internal_t make_t_gate(uint32_t target) {
    qc_gate_internal_t g = {0};
    g.Gate = QC_IGATE_T;
    g.Target = target;
    g.NumControls = 0;
    g.large_control = NULL;
    return g;
}

static qc_gate_internal_t make_tdg_gate(uint32_t target) {
    qc_gate_internal_t g = {0};
    g.Gate = QC_IGATE_TDG;
    g.Target = target;
    g.NumControls = 0;
    g.large_control = NULL;
    return g;
}

static qc_gate_internal_t make_p_gate(uint32_t target, double angle) {
    qc_gate_internal_t g = {0};
    g.Gate = QC_IGATE_P;
    g.Target = target;
    g.NumControls = 0;
    g.GateValue = angle;
    g.large_control = NULL;
    return g;
}

/* ====================================================================== */
/* Tests                                                                   */
/* ====================================================================== */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  %-55s ", #name); \
    tests_run++; \
    name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while (0)

/* --- Test: single gate on single qubit --- */
static void test_single_gate(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g = make_x_gate(0);
    qc_add_gate(ctx, &g);

    assert(ctx->used == 1);
    assert(ctx->used_layer == 1);

    qc_circuit_destroy(ctx);
}

/* --- Test: two non-colliding gates go to same layer --- */
static void test_parallel_gates(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_x_gate(0);
    qc_gate_internal_t g2 = make_x_gate(1);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    /* Two gates on different qubits should be in the same layer */
    assert(ctx->used == 2);
    assert(ctx->used_layer == 1);

    qc_circuit_destroy(ctx);
}

/* --- Test: two gates on same qubit go to different layers --- */
static void test_sequential_gates(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_h_gate(0);
    qc_gate_internal_t g2 = make_x_gate(0);  /* Different gate type, same qubit */
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    assert(ctx->used == 2);
    /* Should be in different layers since they share qubit 0 */
    assert(ctx->used_layer == 2);

    qc_circuit_destroy(ctx);
}

/* --- Test: inverse gate cancellation (X-X) --- */
static void test_inverse_cancellation_xx(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_x_gate(0);
    qc_gate_internal_t g2 = make_x_gate(0);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    /* X-X should cancel */
    assert(ctx->used == 0);

    qc_circuit_destroy(ctx);
}

/* --- Test: inverse gate cancellation (H-H) --- */
static void test_inverse_cancellation_hh(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_h_gate(0);
    qc_gate_internal_t g2 = make_h_gate(0);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    assert(ctx->used == 0);

    qc_circuit_destroy(ctx);
}

/* --- Test: inverse gate cancellation (T-Tdg) --- */
static void test_inverse_cancellation_t_tdg(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_t_gate(0);
    qc_gate_internal_t g2 = make_tdg_gate(0);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    assert(ctx->used == 0);

    qc_circuit_destroy(ctx);
}

/* --- Test: inverse cancellation with P(theta) and P(-theta) --- */
static void test_inverse_cancellation_p_angles(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_p_gate(0, M_PI / 4.0);
    qc_gate_internal_t g2 = make_p_gate(0, -M_PI / 4.0);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    assert(ctx->used == 0);

    qc_circuit_destroy(ctx);
}

/* --- Test: no cancellation for different qubits --- */
static void test_no_cancel_different_qubits(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_x_gate(0);
    qc_gate_internal_t g2 = make_x_gate(1);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    /* Different qubits: no cancellation */
    assert(ctx->used == 2);

    qc_circuit_destroy(ctx);
}

/* --- Test: CNOT gate layer assignment --- */
static void test_cnot_layer_assignment(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    /* CX(0,1) then X(2): should be same layer (no qubit overlap) */
    qc_gate_internal_t g1 = make_cx_gate(0, 1);
    qc_gate_internal_t g2 = make_x_gate(2);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    assert(ctx->used == 2);
    assert(ctx->used_layer == 1);

    /* CX(0,1) then X(1): must be different layers (share qubit 1) */
    circuit_ctx_t *ctx2 = qc_circuit_create(8);
    qc_gate_internal_t g3 = make_cx_gate(0, 1);
    qc_gate_internal_t g4 = make_h_gate(1);
    qc_add_gate(ctx2, &g3);
    qc_add_gate(ctx2, &g4);

    assert(ctx2->used == 2);
    assert(ctx2->used_layer == 2);

    qc_circuit_destroy(ctx);
    qc_circuit_destroy(ctx2);
}

/* --- Test: multiple qubits with mixed parallelism --- */
static void test_mixed_parallelism(void) {
    circuit_ctx_t *ctx = qc_circuit_create(16);
    assert(ctx != NULL);

    /* Layer 0: X(0), X(1), X(2) -- all parallel */
    qc_gate_internal_t gx0 = make_x_gate(0);
    qc_gate_internal_t gx1 = make_x_gate(1);
    qc_gate_internal_t gx2 = make_x_gate(2);
    qc_add_gate(ctx, &gx0);
    qc_add_gate(ctx, &gx1);
    qc_add_gate(ctx, &gx2);
    assert(ctx->used_layer == 1);
    assert(ctx->used == 3);

    /* Layer 1: H(0) -- conflicts with X(0) */
    qc_gate_internal_t gh0 = make_h_gate(0);
    qc_add_gate(ctx, &gh0);
    assert(ctx->used_layer == 2);
    assert(ctx->used == 4);

    qc_circuit_destroy(ctx);
}

/* --- Test: post-construction optimize (copy-replay) --- */
static void test_circuit_optimize(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    /* Build a circuit: H(0), X(1), H(0) -- the two H(0) should NOT cancel
     * because there is X(1) between (different qubits, same layer though).
     * Actually H(0) and H(0) are consecutive on qubit 0, so add_gate
     * will try to merge them. Let's use H(0), then X(0), then something. */
    qc_gate_internal_t g1 = make_h_gate(0);
    qc_gate_internal_t g2 = make_x_gate(1);
    qc_gate_internal_t g3 = make_t_gate(0);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);
    qc_add_gate(ctx, &g3);

    uint64_t orig_count = (uint64_t)ctx->used;
    assert(orig_count == 3);

    /* Optimize should produce a copy with the same gate count
     * (no inverse pairs to cancel) */
    circuit_ctx_t *opt = qc_circuit_optimize(ctx);
    assert(opt != NULL);
    assert(opt->used == ctx->used);

    qc_circuit_destroy(opt);
    qc_circuit_destroy(ctx);
}

/* --- Test: optimize cancels inverse pairs during replay --- */
static void test_optimize_cancels_inverses(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    /* Build: H(0), X(1), X(1) -- X(1)-X(1) cancels during add_gate */
    qc_gate_internal_t g1 = make_h_gate(0);
    qc_gate_internal_t g2 = make_x_gate(1);
    qc_gate_internal_t g3 = make_x_gate(1);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);
    qc_add_gate(ctx, &g3);

    /* X(1)-X(1) should have been cancelled during construction */
    assert(ctx->used == 1);

    /* Optimizing should preserve that */
    circuit_ctx_t *opt = qc_circuit_optimize(ctx);
    assert(opt != NULL);
    assert(opt->used == 1);

    qc_circuit_destroy(opt);
    qc_circuit_destroy(ctx);
}

/* --- Test: optimize_pass with specific pass enum --- */
static void test_optimize_pass(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_x_gate(0);
    qc_gate_internal_t g2 = make_h_gate(1);
    qc_add_gate(ctx, &g1);
    qc_add_gate(ctx, &g2);

    circuit_ctx_t *opt = qc_circuit_optimize_pass(ctx, QC_OPT_CANCEL_INVERSE);
    assert(opt != NULL);
    assert(opt->used == 2);

    circuit_ctx_t *opt2 = qc_circuit_optimize_pass(ctx, QC_OPT_MERGE);
    assert(opt2 != NULL);
    assert(opt2->used == 2);

    qc_circuit_destroy(opt2);
    qc_circuit_destroy(opt);
    qc_circuit_destroy(ctx);
}

/* --- Test: can_optimize on empty circuit --- */
static void test_can_optimize_empty(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);
    assert(qc_circuit_can_optimize(ctx) == false);
    qc_circuit_destroy(ctx);
}

/* --- Test: can_optimize on non-empty circuit --- */
static void test_can_optimize_nonempty(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    qc_gate_internal_t g1 = make_x_gate(0);
    qc_add_gate(ctx, &g1);

    assert(qc_circuit_can_optimize(ctx) == true);
    qc_circuit_destroy(ctx);
}

/* --- Test: NULL ctx handling --- */
static void test_null_handling(void) {
    assert(qc_circuit_optimize(NULL) == NULL);
    assert(qc_circuit_optimize_pass(NULL, QC_OPT_MERGE) == NULL);
    assert(qc_circuit_can_optimize(NULL) == false);

    /* Optimizer functions should handle NULL gracefully */
    qc_gate_internal_t g = make_x_gate(0);
    qc_add_gate(NULL, &g);  /* Should not crash */
    qc_add_gate(NULL, NULL); /* Should not crash */
}

/* --- Test: layer_floor affects gate placement --- */
static void test_layer_floor(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    ctx->layer_floor = 5;
    qc_gate_internal_t g1 = make_x_gate(0);
    qc_add_gate(ctx, &g1);

    /* Gate should be placed at layer >= 5 */
    assert(ctx->used == 1);
    assert(ctx->used_layer >= 6);  /* used_layer = max_layer + 1 */

    qc_circuit_destroy(ctx);
}

/* --- Test: many gates on many qubits (stress, within 17-qubit limit) --- */
static void test_many_gates_stress(void) {
    circuit_ctx_t *ctx = qc_circuit_create(17);
    assert(ctx != NULL);

    /* Add 100 X gates on 17 qubits in round-robin */
    for (int i = 0; i < 100; i++) {
        qc_gate_internal_t g = make_x_gate((uint32_t)(i % 17));
        qc_add_gate(ctx, &g);
    }

    /* Even iterations cancel (X-X), odd remain.
     * qubit 0-15: 6 gates each (100/17 = 5 remainder 15, so 0-14 get 6, 15-16 get 5)
     * Even count = cancelled, odd count = 1 remaining
     * Qubits 0-14: 6 each -> all cancel -> 0 remaining
     * Qubits 15-16: 5 each -> 4 cancel, 1 remaining -> 1 each
     * But wait: 100 / 17 = 5 rem 15, so qubits 0-14 get 6, qubits 15-16 get 5.
     * Actually: round-robin means qubit i gets ceil/floor of 100/17.
     * 100 = 5*17 + 15, so qubits 0-14 get 6 each, qubits 15-16 get 5 each.
     * 6 is even -> all cancel -> 0 gates
     * 5 is odd -> 4 cancel, 1 remains
     * Total remaining: 2 gates (one on qubit 15, one on qubit 16) */
    assert(ctx->used == 2);

    qc_circuit_destroy(ctx);
}

/* --- Test: optimize preserves circuit configuration --- */
static void test_optimize_preserves_config(void) {
    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(ctx != NULL);

    ctx->arithmetic_mode = 0;  /* QFT */
    ctx->qubit_saving = 1;
    ctx->toffoli_decompose = 1;
    ctx->cla_override = 1;

    qc_gate_internal_t g = make_x_gate(0);
    qc_add_gate(ctx, &g);

    circuit_ctx_t *opt = qc_circuit_optimize(ctx);
    assert(opt != NULL);
    assert(opt->arithmetic_mode == 0);
    assert(opt->qubit_saving == 1);
    assert(opt->toffoli_decompose == 1);
    assert(opt->cla_override == 1);

    qc_circuit_destroy(opt);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== test_optimizer (Module 1.6) ===\n");

    TEST(test_single_gate);
    TEST(test_parallel_gates);
    TEST(test_sequential_gates);
    TEST(test_inverse_cancellation_xx);
    TEST(test_inverse_cancellation_hh);
    TEST(test_inverse_cancellation_t_tdg);
    TEST(test_inverse_cancellation_p_angles);
    TEST(test_no_cancel_different_qubits);
    TEST(test_cnot_layer_assignment);
    TEST(test_mixed_parallelism);
    TEST(test_circuit_optimize);
    TEST(test_optimize_cancels_inverses);
    TEST(test_optimize_pass);
    TEST(test_can_optimize_empty);
    TEST(test_can_optimize_nonempty);
    TEST(test_null_handling);
    TEST(test_layer_floor);
    TEST(test_many_gates_stress);
    TEST(test_optimize_preserves_config);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
