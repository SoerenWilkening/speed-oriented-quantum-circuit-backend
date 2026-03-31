/**
 * @file test_qft_arithmetic.c
 * @brief Tests for QFT-based addition and multiplication (Module 1.8).
 *
 * Issue: refactor-h41
 *
 * Tests dynamic QFT addition and multiplication for widths 1-16.
 * Max 17 qubits in simulations (width 8 for QQ ops = 16 qubits).
 *
 * Tests verify:
 *   - CQ addition generates gates for all widths 1-16
 *   - QQ addition generates gates for all widths 1-8
 *   - Controlled variants (cCQ, cQQ) generate gates
 *   - CQ multiplication generates gates for all widths 1-8
 *   - QQ multiplication generates gates for all widths 1-5
 *   - Gate counts are non-zero and consistent across calls
 *   - Width validation rejects 0 and >64
 *   - NULL pointer handling
 */

#include "quantum_circuit.h"
#include "internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====================================================================== */
/* Test infrastructure                                                     */
/* ====================================================================== */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        tests_run++;                                                        \
        if (!(cond)) {                                                      \
            fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__);       \
            tests_failed++;                                                 \
        } else {                                                            \
            tests_passed++;                                                 \
        }                                                                   \
    } while (0)

#define TEST_SECTION(name)                                                  \
    do { printf("--- %s ---\n", name); } while (0)

/* ====================================================================== */
/* Helper: allocate contiguous qubits and fill an array                    */
/* ====================================================================== */

static int alloc_reg(circuit_ctx_t *ctx, uint32_t *reg, uint32_t width) {
    uint32_t start;
    qc_error_t err = qc_qubit_alloc_n(ctx, width, &start);
    if (err != QC_OK)
        return -1;
    for (uint32_t i = 0; i < width; ++i)
        reg[i] = start + i;
    return 0;
}

/* ====================================================================== */
/* Test: CQ addition for widths 1-16                                       */
/* ====================================================================== */

static void test_cq_add_widths(void) {
    TEST_SECTION("CQ addition widths 1-16");

    for (uint32_t w = 1; w <= 16; ++w) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        TEST_ASSERT(ctx != NULL, "create ctx");
        qc_circuit_set_simulate(ctx, true);

        uint32_t target[64];
        int rc = alloc_reg(ctx, target, w);
        TEST_ASSERT(rc == 0, "alloc target");

        qc_error_t err = qc_arith_cq_add(ctx, target, w, 3);
        TEST_ASSERT(err == QC_OK, "cq_add returns OK");

        uint64_t gc = qc_circuit_gate_count(ctx);
        TEST_ASSERT(gc > 0, "gate count > 0");

        if (w <= 4) {
            printf("  CQ_add w=%u: %lu gates\n", w, (unsigned long)gc);
        }

        qc_circuit_destroy(ctx);
    }
}

/* ====================================================================== */
/* Test: QQ addition for widths 1-8                                        */
/* ====================================================================== */

static void test_qq_add_widths(void) {
    TEST_SECTION("QQ addition widths 1-8");

    for (uint32_t w = 1; w <= 8; ++w) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        TEST_ASSERT(ctx != NULL, "create ctx");
        qc_circuit_set_simulate(ctx, true);

        uint32_t a[64], b[64];
        int rc = alloc_reg(ctx, a, w);
        TEST_ASSERT(rc == 0, "alloc a");
        rc = alloc_reg(ctx, b, w);
        TEST_ASSERT(rc == 0, "alloc b");

        qc_error_t err = qc_arith_qq_add(ctx, a, b, w);
        TEST_ASSERT(err == QC_OK, "qq_add returns OK");

        uint64_t gc = qc_circuit_gate_count(ctx);
        TEST_ASSERT(gc > 0, "gate count > 0");

        if (w <= 4) {
            printf("  QQ_add w=%u: %lu gates\n", w, (unsigned long)gc);
        }

        qc_circuit_destroy(ctx);
    }
}

/* ====================================================================== */
/* Test: Controlled CQ addition                                            */
/* ====================================================================== */

static void test_ccq_add(void) {
    TEST_SECTION("Controlled CQ addition widths 1-8");

    for (uint32_t w = 1; w <= 8; ++w) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        TEST_ASSERT(ctx != NULL, "create ctx");
        qc_circuit_set_simulate(ctx, true);

        uint32_t target[64];
        int rc = alloc_reg(ctx, target, w);
        TEST_ASSERT(rc == 0, "alloc target");

        uint32_t ctrl;
        qc_error_t err = qc_qubit_alloc(ctx, &ctrl);
        TEST_ASSERT(err == QC_OK, "alloc ctrl");

        err = qc_arith_ccq_add(ctx, target, w, 5, ctrl);
        TEST_ASSERT(err == QC_OK, "ccq_add returns OK");

        uint64_t gc = qc_circuit_gate_count(ctx);
        TEST_ASSERT(gc > 0, "gate count > 0");

        if (w <= 4) {
            printf("  cCQ_add w=%u: %lu gates\n", w, (unsigned long)gc);
        }

        qc_circuit_destroy(ctx);
    }
}

/* ====================================================================== */
/* Test: Controlled QQ addition                                            */
/* ====================================================================== */

static void test_cqq_add(void) {
    TEST_SECTION("Controlled QQ addition widths 1-8");

    for (uint32_t w = 1; w <= 8; ++w) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        TEST_ASSERT(ctx != NULL, "create ctx");
        qc_circuit_set_simulate(ctx, true);

        uint32_t a[64], b[64];
        int rc = alloc_reg(ctx, a, w);
        TEST_ASSERT(rc == 0, "alloc a");
        rc = alloc_reg(ctx, b, w);
        TEST_ASSERT(rc == 0, "alloc b");

        uint32_t ctrl;
        qc_error_t err = qc_qubit_alloc(ctx, &ctrl);
        TEST_ASSERT(err == QC_OK, "alloc ctrl");

        err = qc_arith_cqq_add(ctx, a, b, w, ctrl);
        TEST_ASSERT(err == QC_OK, "cqq_add returns OK");

        uint64_t gc = qc_circuit_gate_count(ctx);
        TEST_ASSERT(gc > 0, "gate count > 0");

        if (w <= 4) {
            printf("  cQQ_add w=%u: %lu gates\n", w, (unsigned long)gc);
        }

        qc_circuit_destroy(ctx);
    }
}

/* ====================================================================== */
/* Test: CQ multiplication for widths 1-8                                  */
/* ====================================================================== */

static void test_cq_mul_widths(void) {
    TEST_SECTION("CQ multiplication widths 1-8");

    for (uint32_t w = 1; w <= 8; ++w) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        TEST_ASSERT(ctx != NULL, "create ctx");
        qc_circuit_set_simulate(ctx, true);

        uint32_t result[64], target[64];
        int rc = alloc_reg(ctx, result, w);
        TEST_ASSERT(rc == 0, "alloc result");
        rc = alloc_reg(ctx, target, w);
        TEST_ASSERT(rc == 0, "alloc target");

        qc_error_t err = qc_arith_cq_mul(ctx, result, target, w, 3);
        TEST_ASSERT(err == QC_OK, "cq_mul returns OK");

        uint64_t gc = qc_circuit_gate_count(ctx);
        TEST_ASSERT(gc > 0, "gate count > 0");

        if (w <= 4) {
            printf("  CQ_mul w=%u: %lu gates\n", w, (unsigned long)gc);
        }

        qc_circuit_destroy(ctx);
    }
}

/* ====================================================================== */
/* Test: QQ multiplication for widths 1-5                                  */
/* ====================================================================== */

static void test_qq_mul_widths(void) {
    TEST_SECTION("QQ multiplication widths 1-5");

    for (uint32_t w = 1; w <= 5; ++w) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        TEST_ASSERT(ctx != NULL, "create ctx");
        qc_circuit_set_simulate(ctx, true);

        uint32_t result[64], a[64], b[64];
        int rc = alloc_reg(ctx, result, w);
        TEST_ASSERT(rc == 0, "alloc result");
        rc = alloc_reg(ctx, a, w);
        TEST_ASSERT(rc == 0, "alloc a");
        rc = alloc_reg(ctx, b, w);
        TEST_ASSERT(rc == 0, "alloc b");

        qc_error_t err = qc_arith_qq_mul(ctx, result, a, b, w);
        TEST_ASSERT(err == QC_OK, "qq_mul returns OK");

        uint64_t gc = qc_circuit_gate_count(ctx);
        TEST_ASSERT(gc > 0, "gate count > 0");

        printf("  QQ_mul w=%u: %lu gates\n", w, (unsigned long)gc);

        qc_circuit_destroy(ctx);
    }
}

/* ====================================================================== */
/* Test: Consistency — same width produces same gate count                  */
/* ====================================================================== */

static void test_consistency(void) {
    TEST_SECTION("Consistency: repeated calls produce same gate count");

    uint32_t w = 4;

    /* Run CQ addition twice, compare gate counts */
    uint64_t gc1, gc2;

    {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        qc_circuit_set_simulate(ctx, true);
        uint32_t target[64];
        alloc_reg(ctx, target, w);
        qc_arith_cq_add(ctx, target, w, 7);
        gc1 = qc_circuit_gate_count(ctx);
        qc_circuit_destroy(ctx);
    }
    {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        qc_circuit_set_simulate(ctx, true);
        uint32_t target[64];
        alloc_reg(ctx, target, w);
        qc_arith_cq_add(ctx, target, w, 7);
        gc2 = qc_circuit_gate_count(ctx);
        qc_circuit_destroy(ctx);
    }

    TEST_ASSERT(gc1 == gc2, "CQ add gate count consistent");
    printf("  CQ_add w=%u: gc1=%lu gc2=%lu\n", w,
           (unsigned long)gc1, (unsigned long)gc2);

    /* Run QQ addition twice */
    {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        qc_circuit_set_simulate(ctx, true);
        uint32_t a[64], b[64];
        alloc_reg(ctx, a, w);
        alloc_reg(ctx, b, w);
        qc_arith_qq_add(ctx, a, b, w);
        gc1 = qc_circuit_gate_count(ctx);
        qc_circuit_destroy(ctx);
    }
    {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        qc_circuit_set_simulate(ctx, true);
        uint32_t a[64], b[64];
        alloc_reg(ctx, a, w);
        alloc_reg(ctx, b, w);
        qc_arith_qq_add(ctx, a, b, w);
        gc2 = qc_circuit_gate_count(ctx);
        qc_circuit_destroy(ctx);
    }

    TEST_ASSERT(gc1 == gc2, "QQ add gate count consistent");
    printf("  QQ_add w=%u: gc1=%lu gc2=%lu\n", w,
           (unsigned long)gc1, (unsigned long)gc2);
}

/* ====================================================================== */
/* Test: Width validation                                                  */
/* ====================================================================== */

static void test_width_validation(void) {
    TEST_SECTION("Width validation");

    circuit_ctx_t *ctx = qc_circuit_create(64);
    TEST_ASSERT(ctx != NULL, "create ctx");

    uint32_t target[64] = {0, 1, 2, 3};
    uint32_t b[64] = {4, 5, 6, 7};

    /* Width 0 should fail */
    qc_error_t err = qc_arith_cq_add(ctx, target, 0, 3);
    TEST_ASSERT(err == QC_ERR_WIDTH, "CQ add width=0 rejected");

    err = qc_arith_qq_add(ctx, target, b, 0);
    TEST_ASSERT(err == QC_ERR_WIDTH, "QQ add width=0 rejected");

    /* Width 65 should fail */
    err = qc_arith_cq_add(ctx, target, 65, 3);
    TEST_ASSERT(err == QC_ERR_WIDTH, "CQ add width=65 rejected");

    err = qc_arith_qq_add(ctx, target, b, 65);
    TEST_ASSERT(err == QC_ERR_WIDTH, "QQ add width=65 rejected");

    /* Multiplication width validation */
    uint32_t res[64] = {8, 9, 10, 11};
    err = qc_arith_cq_mul(ctx, res, target, 0, 3);
    TEST_ASSERT(err == QC_ERR_WIDTH, "CQ mul width=0 rejected");

    err = qc_arith_qq_mul(ctx, res, target, b, 0);
    TEST_ASSERT(err == QC_ERR_WIDTH, "QQ mul width=0 rejected");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: NULL pointer handling                                             */
/* ====================================================================== */

static void test_null_handling(void) {
    TEST_SECTION("NULL pointer handling");

    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t target[4] = {0, 1, 2, 3};
    uint32_t b[4] = {4, 5, 6, 7};
    uint32_t res[4] = {8, 9, 10, 11};

    /* NULL ctx */
    TEST_ASSERT(qc_arith_cq_add(NULL, target, 4, 3) == QC_ERR_NULL,
                "CQ add NULL ctx");
    TEST_ASSERT(qc_arith_qq_add(NULL, target, b, 4) == QC_ERR_NULL,
                "QQ add NULL ctx");
    TEST_ASSERT(qc_arith_cq_mul(NULL, res, target, 4, 3) == QC_ERR_NULL,
                "CQ mul NULL ctx");
    TEST_ASSERT(qc_arith_qq_mul(NULL, res, target, b, 4) == QC_ERR_NULL,
                "QQ mul NULL ctx");

    /* NULL register */
    TEST_ASSERT(qc_arith_cq_add(ctx, NULL, 4, 3) == QC_ERR_NULL,
                "CQ add NULL target");
    TEST_ASSERT(qc_arith_qq_add(ctx, NULL, b, 4) == QC_ERR_NULL,
                "QQ add NULL a");
    TEST_ASSERT(qc_arith_qq_add(ctx, target, NULL, 4) == QC_ERR_NULL,
                "QQ add NULL b");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: CQ addition with negative value                                   */
/* ====================================================================== */

static void test_cq_add_negative(void) {
    TEST_SECTION("CQ addition with negative value");

    for (uint32_t w = 2; w <= 8; ++w) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        qc_circuit_set_simulate(ctx, true);

        uint32_t target[64];
        alloc_reg(ctx, target, w);

        qc_error_t err = qc_arith_cq_add(ctx, target, w, -1);
        TEST_ASSERT(err == QC_OK, "cq_add negative returns OK");

        uint64_t gc = qc_circuit_gate_count(ctx);
        TEST_ASSERT(gc > 0, "gate count > 0 for negative");

        qc_circuit_destroy(ctx);
    }
}

/* ====================================================================== */
/* Test: Count-only mode (simulate=false)                                  */
/* ====================================================================== */

static void test_count_only_mode(void) {
    TEST_SECTION("Count-only mode (simulate=false)");

    uint32_t w = 4;

    /* simulate=true: store gates */
    circuit_ctx_t *ctx1 = qc_circuit_create(64);
    qc_circuit_set_simulate(ctx1, true);
    uint32_t t1[64];
    alloc_reg(ctx1, t1, w);
    qc_arith_cq_add(ctx1, t1, w, 5);
    uint64_t gc_sim = qc_circuit_gate_count(ctx1);

    /* simulate=false: count only */
    circuit_ctx_t *ctx2 = qc_circuit_create(64);
    qc_circuit_set_simulate(ctx2, false);
    uint32_t t2[64];
    alloc_reg(ctx2, t2, w);
    qc_arith_cq_add(ctx2, t2, w, 5);
    uint64_t gc_cnt = qc_circuit_gate_count(ctx2);

    /* Both should report the same gate count */
    TEST_ASSERT(gc_sim == gc_cnt,
                "simulate and count-only modes agree on gate count");

    printf("  Count-only: sim=%lu count=%lu\n",
           (unsigned long)gc_sim, (unsigned long)gc_cnt);

    qc_circuit_destroy(ctx1);
    qc_circuit_destroy(ctx2);
}

/* ====================================================================== */
/* Test: CQ addition with two's complement edge cases                      */
/* ====================================================================== */

static void test_twos_complement_edge(void) {
    TEST_SECTION("Two's complement edge cases");

    /* Adding 0 should still produce gates (QFT + phase(0) + IQFT) */
    circuit_ctx_t *ctx = qc_circuit_create(64);
    qc_circuit_set_simulate(ctx, true);
    uint32_t target[64];
    alloc_reg(ctx, target, 4);
    qc_error_t err = qc_arith_cq_add(ctx, target, 4, 0);
    TEST_ASSERT(err == QC_OK, "cq_add value=0 OK");
    uint64_t gc = qc_circuit_gate_count(ctx);
    TEST_ASSERT(gc > 0, "gate count > 0 for value=0");
    qc_circuit_destroy(ctx);

    /* Adding max value for width */
    ctx = qc_circuit_create(64);
    qc_circuit_set_simulate(ctx, true);
    alloc_reg(ctx, target, 4);
    err = qc_arith_cq_add(ctx, target, 4, 15);
    TEST_ASSERT(err == QC_OK, "cq_add value=15 OK");
    gc = qc_circuit_gate_count(ctx);
    TEST_ASSERT(gc > 0, "gate count > 0 for max value");
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== QFT Arithmetic Tests (Module 1.8, refactor-h41) ===\n\n");

    test_cq_add_widths();
    test_qq_add_widths();
    test_ccq_add();
    test_cqq_add();
    test_cq_mul_widths();
    test_qq_mul_widths();
    test_consistency();
    test_width_validation();
    test_null_handling();
    test_cq_add_negative();
    test_count_only_mode();
    test_twos_complement_edge();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return (tests_failed > 0) ? 1 : 0;
}
