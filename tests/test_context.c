/**
 * @file test_context.c
 * @brief Tests for Module 1.3: circuit context lifecycle, gate insertion, allocations.
 *
 * Tests:
 * 1. Create/destroy lifecycle (no leaks)
 * 2. Basic gate insertion (X, H, CX, CCX)
 * 3. Gate count and depth tracking
 * 4. Circuit reset
 * 5. Qubit allocation via public API
 * 6. Multi-controlled gates (MCX, MCZ)
 * 7. Generic gate insertion via qc_circuit_add_gate
 * 8. Configuration setters/getters
 * 9. Layer assignment (parallel gates on different qubits)
 * 10. Version query
 *
 * Max 17 qubits per the task constraint.
 */

#include <quantum_circuit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "  FAIL: %s — expected %lld, got %lld (line %d)\n", \
                msg, (long long)(b), (long long)(a), __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS() do { tests_passed++; } while(0)

/* ====================================================================== */
/* Tests                                                                   */
/* ====================================================================== */

static void test_create_destroy(void) {
    printf("  test_create_destroy...\n");
    circuit_ctx_t *ctx = qc_circuit_create(0);
    ASSERT(ctx != NULL, "qc_circuit_create should not return NULL");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 0, "new circuit gate count");
    ASSERT_EQ(qc_circuit_depth(ctx), 0, "new circuit depth");
    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_destroy_null(void) {
    printf("  test_destroy_null...\n");
    qc_circuit_destroy(NULL);  /* Should not crash */
    TEST_PASS();
}

static void test_single_qubit_gates(void) {
    printf("  test_single_qubit_gates...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");

    ASSERT_EQ(qc_circuit_x(ctx, 0), QC_OK, "x gate");
    ASSERT_EQ(qc_circuit_y(ctx, 1), QC_OK, "y gate");
    ASSERT_EQ(qc_circuit_z(ctx, 2), QC_OK, "z gate");
    ASSERT_EQ(qc_circuit_h(ctx, 3), QC_OK, "h gate");
    ASSERT_EQ(qc_circuit_t_gate(ctx, 4), QC_OK, "t gate");
    ASSERT_EQ(qc_circuit_tdg(ctx, 5), QC_OK, "tdg gate");
    ASSERT_EQ(qc_circuit_p(ctx, 6, 3.14), QC_OK, "p gate");
    ASSERT_EQ(qc_circuit_rx(ctx, 7, 1.57), QC_OK, "rx gate");
    ASSERT_EQ(qc_circuit_ry(ctx, 8, 1.57), QC_OK, "ry gate");
    ASSERT_EQ(qc_circuit_rz(ctx, 9, 1.57), QC_OK, "rz gate");

    /* All gates on different qubits -> should all be in layer 0 (depth=1) */
    ASSERT_EQ(qc_circuit_gate_count(ctx), 10, "10 gates total");
    ASSERT_EQ(qc_circuit_depth(ctx), 1, "all parallel -> depth 1");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_two_qubit_gates(void) {
    printf("  test_two_qubit_gates...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    ASSERT_EQ(qc_circuit_cx(ctx, 0, 1), QC_OK, "cx");
    ASSERT_EQ(qc_circuit_cy(ctx, 2, 3), QC_OK, "cy");
    ASSERT_EQ(qc_circuit_cz(ctx, 4, 5), QC_OK, "cz");
    ASSERT_EQ(qc_circuit_ch(ctx, 6, 7), QC_OK, "ch");
    ASSERT_EQ(qc_circuit_cp(ctx, 8, 9, 1.0), QC_OK, "cp");
    ASSERT_EQ(qc_circuit_cry(ctx, 10, 11, 0.5), QC_OK, "cry");

    ASSERT_EQ(qc_circuit_gate_count(ctx), 6, "6 two-qubit gates");
    /* Each gate touches 2 distinct qubits, no overlap -> depth 1 */
    ASSERT_EQ(qc_circuit_depth(ctx), 1, "all parallel -> depth 1");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_ccx_gate(void) {
    printf("  test_ccx_gate...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    ASSERT_EQ(qc_circuit_ccx(ctx, 0, 1, 2), QC_OK, "ccx");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "1 ccx gate");
    ASSERT_EQ(qc_circuit_depth(ctx), 1, "depth 1");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_mcx_gate(void) {
    printf("  test_mcx_gate...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t controls[] = {0, 1, 2, 3};
    ASSERT_EQ(qc_circuit_mcx(ctx, controls, 4, 4), QC_OK, "mcx 4 controls");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "1 mcx gate");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_mcz_gate(void) {
    printf("  test_mcz_gate...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t controls[] = {0, 1, 2};
    ASSERT_EQ(qc_circuit_mcz(ctx, controls, 3, 3), QC_OK, "mcz 3 controls");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "1 mcz gate");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_depth_sequential(void) {
    printf("  test_depth_sequential...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* H then X on the same qubit -> non-inverses, must be sequential */
    qc_circuit_h(ctx, 0);
    qc_circuit_x(ctx, 0);
    ASSERT_EQ(qc_circuit_gate_count(ctx), 2, "2 gates");
    ASSERT_EQ(qc_circuit_depth(ctx), 2, "sequential -> depth 2");

    /* Third gate (Z) on same qubit */
    qc_circuit_z(ctx, 0);
    ASSERT_EQ(qc_circuit_depth(ctx), 3, "depth 3");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_depth_parallel(void) {
    printf("  test_depth_parallel...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* Gates on different qubits should be parallel */
    for (uint32_t i = 0; i < 16; ++i) {
        qc_circuit_x(ctx, i);
    }
    ASSERT_EQ(qc_circuit_gate_count(ctx), 16, "16 gates");
    ASSERT_EQ(qc_circuit_depth(ctx), 1, "all parallel -> depth 1");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_circuit_reset(void) {
    printf("  test_circuit_reset...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    qc_circuit_x(ctx, 0);
    qc_circuit_h(ctx, 1);
    qc_circuit_cx(ctx, 0, 1);
    ASSERT(qc_circuit_gate_count(ctx) > 0, "gates before reset");

    ASSERT_EQ(qc_circuit_reset(ctx), QC_OK, "reset");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 0, "gate count after reset");
    ASSERT_EQ(qc_circuit_depth(ctx), 0, "depth after reset");

    /* Should be reusable after reset */
    qc_circuit_x(ctx, 0);
    ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "gate after re-use");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_null_ctx_errors(void) {
    printf("  test_null_ctx_errors...\n");
    ASSERT_EQ(qc_circuit_x(NULL, 0), QC_ERR_NULL, "x null");
    ASSERT_EQ(qc_circuit_cx(NULL, 0, 1), QC_ERR_NULL, "cx null");
    ASSERT_EQ(qc_circuit_reset(NULL), QC_ERR_NULL, "reset null");
    ASSERT_EQ(qc_circuit_gate_count(NULL), 0, "gate_count null");
    ASSERT_EQ(qc_circuit_depth(NULL), 0, "depth null");
    TEST_PASS();
}

static void test_qubit_alloc_free(void) {
    printf("  test_qubit_alloc_free...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t q;
    ASSERT_EQ(qc_qubit_alloc(ctx, &q), QC_OK, "alloc qubit");
    ASSERT_EQ(q, 0, "first qubit is 0");
    ASSERT(qc_qubit_is_allocated(ctx, q), "qubit 0 allocated");

    uint32_t q2;
    ASSERT_EQ(qc_qubit_alloc(ctx, &q2), QC_OK, "alloc second qubit");
    ASSERT_EQ(q2, 1, "second qubit is 1");

    ASSERT_EQ(qc_qubit_free(ctx, q), QC_OK, "free qubit 0");
    ASSERT(!qc_qubit_is_allocated(ctx, q), "qubit 0 freed");

    /* Allocating again should reuse qubit 0 */
    uint32_t q3;
    ASSERT_EQ(qc_qubit_alloc(ctx, &q3), QC_OK, "alloc third qubit");
    ASSERT_EQ(q3, 0, "reused qubit 0");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_qubit_alloc_n(void) {
    printf("  test_qubit_alloc_n...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t start;
    ASSERT_EQ(qc_qubit_alloc_n(ctx, 4, &start), QC_OK, "alloc 4 qubits");
    ASSERT_EQ(start, 0, "block starts at 0");

    for (uint32_t i = 0; i < 4; ++i)
        ASSERT(qc_qubit_is_allocated(ctx, start + i), "block qubit allocated");

    ASSERT_EQ(qc_qubit_free_n(ctx, start, 4), QC_OK, "free block");
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT(!qc_qubit_is_allocated(ctx, start + i), "block qubit freed");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_double_free(void) {
    printf("  test_double_free...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t q;
    qc_qubit_alloc(ctx, &q);
    qc_qubit_free(ctx, q);
    ASSERT_EQ(qc_qubit_free(ctx, q), QC_ERR_DOUBLE_FREE, "double free");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_generic_add_gate(void) {
    printf("  test_generic_add_gate...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* Single-qubit via generic API */
    ASSERT_EQ(qc_circuit_add_gate(ctx, QC_GATE_X, 0, NULL, 0, 0.0), QC_OK, "generic X");
    ASSERT_EQ(qc_circuit_add_gate(ctx, QC_GATE_H, 1, NULL, 0, 0.0), QC_OK, "generic H");
    ASSERT_EQ(qc_circuit_add_gate(ctx, QC_GATE_P, 2, NULL, 0, 1.57), QC_OK, "generic P");

    /* Controlled via generic API */
    uint32_t ctrl = 3;
    ASSERT_EQ(qc_circuit_add_gate(ctx, QC_GATE_CX, 4, &ctrl, 1, 0.0), QC_OK, "generic CX");

    uint32_t ctrls[2] = {5, 6};
    ASSERT_EQ(qc_circuit_add_gate(ctx, QC_GATE_CCX, 7, ctrls, 2, 0.0), QC_OK, "generic CCX");

    ASSERT_EQ(qc_circuit_gate_count(ctx), 5, "5 generic gates");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_swap_gate(void) {
    printf("  test_swap_gate...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t ctrl = 0;
    ASSERT_EQ(qc_circuit_add_gate(ctx, QC_GATE_SWAP, 1, &ctrl, 1, 0.0), QC_OK, "swap");
    /* SWAP = 3 CX gates */
    ASSERT_EQ(qc_circuit_gate_count(ctx), 3, "swap = 3 CX");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_config_setters(void) {
    printf("  test_config_setters...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    ASSERT_EQ(qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT), QC_OK, "set arith QFT");
    ASSERT_EQ(qc_circuit_get_arith_mode(ctx), QC_ARITH_QFT, "get arith QFT");

    ASSERT_EQ(qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI), QC_OK, "set arith Toffoli");
    ASSERT_EQ(qc_circuit_get_arith_mode(ctx), QC_ARITH_TOFFOLI, "get arith Toffoli");

    ASSERT_EQ(qc_circuit_set_toffoli_decompose(ctx, true), QC_OK, "set decompose");
    ASSERT_EQ(qc_circuit_set_qubit_saving(ctx, true), QC_OK, "set qubit saving");
    ASSERT_EQ(qc_circuit_set_cla_override(ctx, 1), QC_OK, "set cla override");
    ASSERT_EQ(qc_circuit_set_simulate(ctx, false), QC_OK, "set simulate false");
    ASSERT_EQ(qc_circuit_set_layer_floor(ctx, 5), QC_OK, "set layer floor");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_count_only_mode(void) {
    printf("  test_count_only_mode...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    qc_circuit_set_simulate(ctx, false);
    qc_circuit_x(ctx, 0);
    qc_circuit_h(ctx, 1);
    qc_circuit_cx(ctx, 0, 1);

    /* Gates should be counted but not stored */
    ASSERT_EQ(qc_circuit_gate_count(ctx), 3, "count-only tracks gate count");
    ASSERT_EQ(qc_circuit_depth(ctx), 0, "count-only: no layers stored");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_version(void) {
    printf("  test_version...\n");
    const char *ver = qc_version_string();
    ASSERT(ver != NULL, "version string not null");
    ASSERT(strcmp(ver, "1.0.0") == 0, "version string is 1.0.0");
    ASSERT_EQ(qc_version_number(), 10000, "version number is 10000");
    TEST_PASS();
}

static void test_alloc_stats(void) {
    printf("  test_alloc_stats...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t q1, q2;
    qc_qubit_alloc(ctx, &q1);
    qc_qubit_alloc(ctx, &q2);
    qc_qubit_free(ctx, q1);

    qc_alloc_stats_t stats = qc_circuit_alloc_stats(ctx);
    ASSERT_EQ(stats.total_allocations, 2, "2 allocs");
    ASSERT_EQ(stats.total_deallocations, 1, "1 dealloc");
    ASSERT_EQ(stats.current_in_use, 1, "1 in use");
    ASSERT_EQ(stats.peak_allocated, 2, "peak 2");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_many_gates_grow(void) {
    printf("  test_many_gates_grow...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* Add 200 gates on different qubits to trigger array growth */
    for (uint32_t i = 0; i < 200; ++i) {
        qc_circuit_x(ctx, i);
    }
    ASSERT_EQ(qc_circuit_gate_count(ctx), 200, "200 gates");
    /* All on different qubits -> depth 1 */
    ASSERT_EQ(qc_circuit_depth(ctx), 1, "200 parallel gates -> depth 1");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

static void test_layer_floor(void) {
    printf("  test_layer_floor...\n");
    circuit_ctx_t *ctx = qc_circuit_create(16);

    qc_circuit_set_layer_floor(ctx, 5);
    qc_circuit_x(ctx, 0);
    /* Gate should be at layer >= 5 */
    ASSERT(qc_circuit_depth(ctx) >= 6, "layer floor respected");

    qc_circuit_destroy(ctx);
    TEST_PASS();
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("Running Module 1.3 tests...\n");

    test_create_destroy();
    test_destroy_null();
    test_single_qubit_gates();
    test_two_qubit_gates();
    test_ccx_gate();
    test_mcx_gate();
    test_mcz_gate();
    test_depth_sequential();
    test_depth_parallel();
    test_circuit_reset();
    test_null_ctx_errors();
    test_qubit_alloc_free();
    test_qubit_alloc_n();
    test_double_free();
    test_generic_add_gate();
    test_swap_gate();
    test_config_setters();
    test_count_only_mode();
    test_version();
    test_alloc_stats();
    test_many_gates_grow();
    test_layer_floor();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
