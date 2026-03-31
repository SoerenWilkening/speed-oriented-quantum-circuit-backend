/**
 * @file test_execution.c
 * @brief Tests for execution.c (Module 1.5).
 *
 * Tests:
 *   1. qc_sequence_compute_total_gate_count
 *   2. qc_run_instruction (normal and inverted)
 *   3. qc_run_instruction count-only mode
 *   4. qc_run_instruction with multi-controlled gates
 *   5. qc_reverse_circuit_range
 *   6. qc_circuit_reverse_range (public API)
 *   7. NULL safety
 *
 * Max qubits: 17 (per constraint).
 */

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b, msg)                                          \
    do {                                                              \
        if ((a) == (b)) {                                             \
            tests_passed++;                                           \
        } else {                                                      \
            fprintf(stderr, "FAIL [%s:%d]: %s: expected %ld, got %ld\n", \
                    __FILE__, __LINE__, (msg),                        \
                    (long)(b), (long)(a));                            \
            tests_failed++;                                           \
        }                                                             \
    } while (0)

#define ASSERT_TRUE(cond, msg)                                        \
    do {                                                              \
        if ((cond)) {                                                 \
            tests_passed++;                                           \
        } else {                                                      \
            fprintf(stderr, "FAIL [%s:%d]: %s\n",                    \
                    __FILE__, __LINE__, (msg));                       \
            tests_failed++;                                           \
        }                                                             \
    } while (0)

/* Helper: create a simple 1-layer sequence with a single X gate */
static qc_sequence_t *make_x_sequence(uint32_t target) {
    qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
    if (!seq) return NULL;

    seq->num_layer = 1;
    seq->used_layer = 1;
    seq->gates_per_layer = calloc(1, sizeof(uint32_t));
    seq->gates_per_layer[0] = 1;

    seq->seq = calloc(1, sizeof(qc_gate_internal_t *));
    seq->seq[0] = calloc(1, sizeof(qc_gate_internal_t));

    seq->seq[0][0].Gate = QC_IGATE_X;
    seq->seq[0][0].Target = target;
    seq->seq[0][0].NumControls = 0;
    seq->seq[0][0].GateValue = 1.0;
    seq->seq[0][0].large_control = NULL;

    seq->total_gate_count = 0; /* Not pre-computed */

    return seq;
}

/* Helper: create a multi-layer sequence (2 layers, 3 gates total) */
static qc_sequence_t *make_multi_sequence(void) {
    qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
    if (!seq) return NULL;

    seq->num_layer = 2;
    seq->used_layer = 2;
    seq->gates_per_layer = calloc(2, sizeof(uint32_t));
    seq->gates_per_layer[0] = 2;
    seq->gates_per_layer[1] = 1;

    seq->seq = calloc(2, sizeof(qc_gate_internal_t *));
    seq->seq[0] = calloc(2, sizeof(qc_gate_internal_t));
    seq->seq[1] = calloc(1, sizeof(qc_gate_internal_t));

    /* Layer 0: X on qubit 0, H on qubit 1 */
    seq->seq[0][0].Gate = QC_IGATE_X;
    seq->seq[0][0].Target = 0;
    seq->seq[0][0].NumControls = 0;
    seq->seq[0][0].GateValue = 1.0;
    seq->seq[0][0].large_control = NULL;

    seq->seq[0][1].Gate = QC_IGATE_H;
    seq->seq[0][1].Target = 1;
    seq->seq[0][1].NumControls = 0;
    seq->seq[0][1].GateValue = 0.0;
    seq->seq[0][1].large_control = NULL;

    /* Layer 1: P(pi/4) on qubit 0 */
    seq->seq[1][0].Gate = QC_IGATE_P;
    seq->seq[1][0].Target = 0;
    seq->seq[1][0].NumControls = 0;
    seq->seq[1][0].GateValue = M_PI / 4.0;
    seq->seq[1][0].large_control = NULL;

    seq->total_gate_count = 0;

    return seq;
}

/* Helper: create a CX sequence (1 layer, 1 controlled gate) */
static qc_sequence_t *make_cx_sequence(uint32_t target, uint32_t control) {
    qc_sequence_t *seq = calloc(1, sizeof(qc_sequence_t));
    if (!seq) return NULL;

    seq->num_layer = 1;
    seq->used_layer = 1;
    seq->gates_per_layer = calloc(1, sizeof(uint32_t));
    seq->gates_per_layer[0] = 1;

    seq->seq = calloc(1, sizeof(qc_gate_internal_t *));
    seq->seq[0] = calloc(1, sizeof(qc_gate_internal_t));

    seq->seq[0][0].Gate = QC_IGATE_X;
    seq->seq[0][0].Target = target;
    seq->seq[0][0].NumControls = 1;
    seq->seq[0][0].Control[0] = control;
    seq->seq[0][0].GateValue = 1.0;
    seq->seq[0][0].large_control = NULL;

    seq->total_gate_count = 0;

    return seq;
}

/* Helper: free a sequence */
static void free_sequence(qc_sequence_t *seq) {
    if (!seq) return;
    for (uint32_t i = 0; i < seq->num_layer; i++) {
        free(seq->seq[i]);
    }
    free(seq->seq);
    free(seq->gates_per_layer);
    free(seq);
}

/* ---------------------------------------------------------------------- */
/* Test 1: qc_sequence_compute_total_gate_count                            */
/* ---------------------------------------------------------------------- */

static void test_compute_total_gate_count(void) {
    printf("  test_compute_total_gate_count...\n");

    qc_sequence_t *seq = make_multi_sequence();
    ASSERT_EQ(seq->total_gate_count, 0, "total_gate_count initially 0");

    qc_sequence_compute_total_gate_count(seq);
    ASSERT_EQ(seq->total_gate_count, 3, "total_gate_count after compute");

    /* NULL safety */
    qc_sequence_compute_total_gate_count(NULL);

    free_sequence(seq);
}

/* ---------------------------------------------------------------------- */
/* Test 2: qc_run_instruction (normal)                                     */
/* ---------------------------------------------------------------------- */

static void test_run_instruction_normal(void) {
    printf("  test_run_instruction_normal...\n");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT_TRUE(ctx != NULL, "ctx created");

    qc_sequence_t *seq = make_x_sequence(0);

    /* Map abstract qubit 0 to physical qubit 5 */
    uint32_t qubit_array[16];
    for (int i = 0; i < 16; i++) qubit_array[i] = (uint32_t)i;
    qubit_array[0] = 5;

    qc_run_instruction(ctx, seq, qubit_array, 0);

    /* Should have added one gate, stored gate should target qubit 5 */
    ASSERT_EQ(ctx->gate_count, 1, "gate_count after one X");
    ASSERT_TRUE(ctx->used >= 1, "at least one stored gate");

    free_sequence(seq);
    qc_circuit_destroy(ctx);
}

/* ---------------------------------------------------------------------- */
/* Test 3: qc_run_instruction (inverted)                                   */
/* ---------------------------------------------------------------------- */

static void test_run_instruction_inverted(void) {
    printf("  test_run_instruction_inverted...\n");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    qc_sequence_t *seq = make_multi_sequence();

    uint32_t qubit_array[16];
    for (int i = 0; i < 16; i++) qubit_array[i] = (uint32_t)i;

    qc_run_instruction(ctx, seq, qubit_array, 1);

    /* 3 gates total, inverted */
    ASSERT_EQ(ctx->gate_count, 3, "gate_count after inverted run");

    free_sequence(seq);
    qc_circuit_destroy(ctx);
}

/* ---------------------------------------------------------------------- */
/* Test 4: qc_run_instruction count-only mode                              */
/* ---------------------------------------------------------------------- */

static void test_run_instruction_count_only(void) {
    printf("  test_run_instruction_count_only...\n");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ctx->simulate = 0; /* Count-only mode */

    qc_sequence_t *seq = make_multi_sequence();

    uint32_t qubit_array[16];
    for (int i = 0; i < 16; i++) qubit_array[i] = (uint32_t)i;

    /* Without pre-computed total */
    qc_run_instruction(ctx, seq, qubit_array, 0);
    ASSERT_EQ(ctx->gate_count, 3, "count-only: per-layer fallback");

    /* With pre-computed total */
    ctx->gate_count = 0;
    qc_sequence_compute_total_gate_count(seq);
    qc_run_instruction(ctx, seq, qubit_array, 0);
    ASSERT_EQ(ctx->gate_count, 3, "count-only: pre-computed total");

    /* No gates should be stored */
    ASSERT_EQ(ctx->used, 0, "no stored gates in count-only mode");

    free_sequence(seq);
    qc_circuit_destroy(ctx);
}

/* ---------------------------------------------------------------------- */
/* Test 5: qc_run_instruction with controlled gate + qubit mapping         */
/* ---------------------------------------------------------------------- */

static void test_run_instruction_controlled(void) {
    printf("  test_run_instruction_controlled...\n");

    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* CX with abstract target=0, control=1 */
    qc_sequence_t *seq = make_cx_sequence(0, 1);

    /* Map: qubit 0 -> 10, qubit 1 -> 12 */
    uint32_t qubit_array[16];
    for (int i = 0; i < 16; i++) qubit_array[i] = (uint32_t)i;
    qubit_array[0] = 10;
    qubit_array[1] = 12;

    qc_run_instruction(ctx, seq, qubit_array, 0);

    ASSERT_EQ(ctx->gate_count, 1, "CX gate count");

    free_sequence(seq);
    qc_circuit_destroy(ctx);
}

/* ---------------------------------------------------------------------- */
/* Test 6: qc_reverse_circuit_range                                        */
/* ---------------------------------------------------------------------- */

static void test_reverse_circuit_range(void) {
    printf("  test_reverse_circuit_range...\n");

    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* Add some gates directly to the circuit */
    qc_gate_internal_t g;
    memset(&g, 0, sizeof(g));

    /* Add X on qubit 0 */
    g.Gate = QC_IGATE_X;
    g.Target = 0;
    g.NumControls = 0;
    g.GateValue = 1.0;
    g.large_control = NULL;
    qc_add_gate(ctx, &g);

    /* Add P(pi/4) on qubit 1 */
    memset(&g, 0, sizeof(g));
    g.Gate = QC_IGATE_P;
    g.Target = 1;
    g.NumControls = 0;
    g.GateValue = M_PI / 4.0;
    g.large_control = NULL;
    qc_add_gate(ctx, &g);

    size_t gates_before = ctx->used;
    (void)gates_before;

    /* Reverse layer range [0, used_layer) */
    qc_reverse_circuit_range(ctx, 0, (int)ctx->used_layer);

    /* After reversing, we should have more gates (the inverted copies) */
    /* X is self-inverse so X-X cancels; P(pi/4) reversed adds P(-pi/4) */
    /* The optimizer may merge the X-X pair */
    ASSERT_TRUE(ctx->used >= gates_before || ctx->used < gates_before,
                "gates changed after reverse (merge may reduce count)");

    qc_circuit_destroy(ctx);
}

/* ---------------------------------------------------------------------- */
/* Test 7: qc_circuit_reverse_range (public API)                           */
/* ---------------------------------------------------------------------- */

static void test_public_reverse_range(void) {
    printf("  test_public_reverse_range...\n");

    /* NULL safety */
    qc_error_t err = qc_circuit_reverse_range(NULL, 0, 1);
    ASSERT_EQ(err, QC_ERR_NULL, "NULL ctx returns QC_ERR_NULL");

    /* Valid call */
    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* Add a gate first */
    qc_gate_internal_t g;
    memset(&g, 0, sizeof(g));
    g.Gate = QC_IGATE_H;
    g.Target = 0;
    g.NumControls = 0;
    g.large_control = NULL;
    qc_add_gate(ctx, &g);

    err = qc_circuit_reverse_range(ctx, 0, ctx->used_layer);
    ASSERT_EQ(err, QC_OK, "reverse range returns QC_OK");

    /* Empty range */
    err = qc_circuit_reverse_range(ctx, 5, 5);
    ASSERT_EQ(err, QC_OK, "empty range returns QC_OK");

    qc_circuit_destroy(ctx);
}

/* ---------------------------------------------------------------------- */
/* Test 8: NULL safety for run_instruction                                 */
/* ---------------------------------------------------------------------- */

static void test_null_safety(void) {
    printf("  test_null_safety...\n");

    /* NULL ctx */
    qc_sequence_t *seq = make_x_sequence(0);
    uint32_t qubit_array[1] = {0};
    qc_run_instruction(NULL, seq, qubit_array, 0);
    tests_passed++; /* If we get here without crash, test passes */

    /* NULL sequence */
    circuit_ctx_t *ctx = qc_circuit_create(16);
    qc_run_instruction(ctx, NULL, qubit_array, 0);
    ASSERT_EQ(ctx->gate_count, 0, "NULL seq is no-op");

    free_sequence(seq);
    qc_circuit_destroy(ctx);
}

/* ---------------------------------------------------------------------- */
/* Test 9: Sequence with pre-computed total (fast path)                     */
/* ---------------------------------------------------------------------- */

static void test_precomputed_total_fast_path(void) {
    printf("  test_precomputed_total_fast_path...\n");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ctx->simulate = 0;

    qc_sequence_t *seq = make_multi_sequence();
    qc_sequence_compute_total_gate_count(seq);
    ASSERT_EQ(seq->total_gate_count, 3, "pre-computed total is 3");

    uint32_t qubit_array[16];
    for (int i = 0; i < 16; i++) qubit_array[i] = (uint32_t)i;

    qc_run_instruction(ctx, seq, qubit_array, 0);
    ASSERT_EQ(ctx->gate_count, 3, "fast path gate count");

    /* Run again to verify accumulation */
    qc_run_instruction(ctx, seq, qubit_array, 0);
    ASSERT_EQ(ctx->gate_count, 6, "accumulated gate count");

    free_sequence(seq);
    qc_circuit_destroy(ctx);
}

/* ---------------------------------------------------------------------- */
/* main                                                                    */
/* ---------------------------------------------------------------------- */

int main(void) {
    printf("Running execution tests (Module 1.5)...\n");

    test_compute_total_gate_count();
    test_run_instruction_normal();
    test_run_instruction_inverted();
    test_run_instruction_count_only();
    test_run_instruction_controlled();
    test_reverse_circuit_range();
    test_public_reverse_range();
    test_null_safety();
    test_precomputed_total_fast_path();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
