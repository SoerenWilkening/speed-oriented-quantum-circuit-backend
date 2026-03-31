/**
 * @file test_dispatch.c
 * @brief Tests for arithmetic dispatch (Module 1.13, refactor-r5b).
 *
 * Verifies that the dispatch layer correctly routes arithmetic operations
 * based on ctx->arithmetic_mode for both QFT and Toffoli modes.
 *
 * Tests cover:
 *   - QQ addition dispatch (QFT and Toffoli)
 *   - CQ addition dispatch (QFT and Toffoli)
 *   - Controlled QQ/CQ addition dispatch
 *   - QQ/CQ subtraction dispatch
 *   - QQ/CQ multiplication dispatch
 *   - Division dispatch (Toffoli only, QFT returns error)
 *   - Modular operations dispatch (Toffoli only)
 *   - QQ comparison (equal, less-than)
 *   - Bitwise operations (mode-independent)
 *   - NULL/invalid input handling
 *
 * Max 17 qubits for circuit simulations.
 */

#include "quantum_circuit.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Test infrastructure                                                     */
/* ====================================================================== */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  %-55s ", name); \
    } while (0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while (0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf("FAIL: %s\n", msg); \
    } while (0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            FAIL(msg); \
            return; \
        } \
    } while (0)

/* ====================================================================== */
/* Test: QQ addition dispatch (QFT mode)                                   */
/* ====================================================================== */

static void test_dispatch_qq_add_qft(void) {
    TEST("dispatch_qq_add QFT mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t a[] = {0, 1, 2, 3};
    uint32_t b[] = {4, 5, 6, 7};

    qc_error_t err = qc_dispatch_qq_add(ctx, a, b, 4);
    ASSERT(err == QC_OK, "dispatch_qq_add QFT failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: QQ addition dispatch (Toffoli mode)                               */
/* ====================================================================== */

static void test_dispatch_qq_add_toffoli(void) {
    TEST("dispatch_qq_add Toffoli mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t a[] = {0, 1, 2, 3};
    uint32_t b[] = {4, 5, 6, 7};

    qc_error_t err = qc_dispatch_qq_add(ctx, a, b, 4);
    ASSERT(err == QC_OK, "dispatch_qq_add Toffoli failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: CQ addition dispatch (both modes)                                 */
/* ====================================================================== */

static void test_dispatch_cq_add_qft(void) {
    TEST("dispatch_cq_add QFT mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t target[] = {0, 1, 2, 3};
    qc_error_t err = qc_dispatch_cq_add(ctx, target, 4, 5);
    ASSERT(err == QC_OK, "dispatch_cq_add QFT failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_dispatch_cq_add_toffoli(void) {
    TEST("dispatch_cq_add Toffoli mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t target[] = {0, 1, 2, 3};
    qc_error_t err = qc_dispatch_cq_add(ctx, target, 4, 5);
    ASSERT(err == QC_OK, "dispatch_cq_add Toffoli failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Controlled QQ addition dispatch                                   */
/* ====================================================================== */

static void test_dispatch_cqq_add_qft(void) {
    TEST("dispatch_cqq_add QFT mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t a[] = {0, 1};
    uint32_t b[] = {2, 3};
    uint32_t control = 4;

    qc_error_t err = qc_dispatch_cqq_add(ctx, a, b, 2, control);
    ASSERT(err == QC_OK, "dispatch_cqq_add QFT failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_dispatch_cqq_add_toffoli(void) {
    TEST("dispatch_cqq_add Toffoli mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t a[] = {0, 1};
    uint32_t b[] = {2, 3};
    uint32_t control = 4;

    qc_error_t err = qc_dispatch_cqq_add(ctx, a, b, 2, control);
    ASSERT(err == QC_OK, "dispatch_cqq_add Toffoli failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Controlled CQ addition dispatch                                   */
/* ====================================================================== */

static void test_dispatch_ccq_add_qft(void) {
    TEST("dispatch_ccq_add QFT mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t target[] = {0, 1, 2};
    uint32_t control = 3;

    qc_error_t err = qc_dispatch_ccq_add(ctx, target, 3, 3, control);
    ASSERT(err == QC_OK, "dispatch_ccq_add QFT failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_dispatch_ccq_add_toffoli(void) {
    TEST("dispatch_ccq_add Toffoli mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t target[] = {0, 1, 2};
    uint32_t control = 3;

    qc_error_t err = qc_dispatch_ccq_add(ctx, target, 3, 3, control);
    ASSERT(err == QC_OK, "dispatch_ccq_add Toffoli failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Subtraction dispatch                                              */
/* ====================================================================== */

static void test_dispatch_qq_sub(void) {
    TEST("dispatch_qq_sub");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t a[] = {0, 1, 2};
    uint32_t b[] = {3, 4, 5};

    qc_error_t err = qc_dispatch_qq_sub(ctx, a, b, 3);
    ASSERT(err == QC_OK, "dispatch_qq_sub failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_dispatch_cq_sub(void) {
    TEST("dispatch_cq_sub");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t target[] = {0, 1, 2, 3};
    qc_error_t err = qc_dispatch_cq_sub(ctx, target, 4, 7);
    ASSERT(err == QC_OK, "dispatch_cq_sub failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Multiplication dispatch                                           */
/* ====================================================================== */

static void test_dispatch_qq_mul_qft(void) {
    TEST("dispatch_qq_mul QFT mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t result[] = {0, 1};
    uint32_t a[] = {2, 3};
    uint32_t b[] = {4, 5};

    qc_error_t err = qc_dispatch_qq_mul(ctx, result, a, b, 2);
    ASSERT(err == QC_OK, "dispatch_qq_mul QFT failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_dispatch_qq_mul_toffoli(void) {
    TEST("dispatch_qq_mul Toffoli mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t result[] = {0, 1};
    uint32_t a[] = {2, 3};
    uint32_t b[] = {4, 5};

    qc_error_t err = qc_dispatch_qq_mul(ctx, result, a, b, 2);
    ASSERT(err == QC_OK, "dispatch_qq_mul Toffoli failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_dispatch_cq_mul_qft(void) {
    TEST("dispatch_cq_mul QFT mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t result[] = {0, 1, 2};
    uint32_t target[] = {3, 4, 5};

    qc_error_t err = qc_dispatch_cq_mul(ctx, result, target, 3, 5);
    ASSERT(err == QC_OK, "dispatch_cq_mul QFT failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Division dispatch (Toffoli only)                                  */
/* ====================================================================== */

static void test_dispatch_divmod_cq_toffoli(void) {
    TEST("dispatch_divmod_cq Toffoli mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t dividend[] = {0, 1, 2};
    uint32_t quotient[] = {3, 4, 5};
    uint32_t remainder[] = {6, 7, 8};

    qc_error_t err = qc_dispatch_divmod_cq(ctx, dividend, 3, 3,
                                            quotient, remainder);
    ASSERT(err == QC_OK, "dispatch_divmod_cq Toffoli failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_dispatch_divmod_cq_qft_rejected(void) {
    TEST("dispatch_divmod_cq QFT mode rejected");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t dividend[] = {0, 1, 2};
    uint32_t quotient[] = {3, 4, 5};
    uint32_t remainder[] = {6, 7, 8};

    qc_error_t err = qc_dispatch_divmod_cq(ctx, dividend, 3, 3,
                                            quotient, remainder);
    ASSERT(err == QC_ERR_INVALID_OP, "should reject QFT mode for division");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Modular operations dispatch (Toffoli only)                        */
/* ====================================================================== */

static void test_dispatch_mod_reduce_toffoli(void) {
    TEST("dispatch_mod_reduce Toffoli mode");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t value[] = {0, 1, 2};
    qc_error_t err = qc_dispatch_mod_reduce(ctx, value, 3, 5);
    ASSERT(err == QC_OK, "dispatch_mod_reduce Toffoli failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_dispatch_mod_reduce_qft_rejected(void) {
    TEST("dispatch_mod_reduce QFT mode rejected");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t value[] = {0, 1, 2};
    qc_error_t err = qc_dispatch_mod_reduce(ctx, value, 3, 5);
    ASSERT(err == QC_ERR_INVALID_OP, "should reject QFT mode for mod_reduce");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: QQ equality comparison                                            */
/* ====================================================================== */

static void test_qq_equal_1bit(void) {
    TEST("qc_cmp_qq_equal 1-bit");

    circuit_ctx_t *ctx = qc_circuit_create(8);
    ASSERT(ctx != NULL, "create failed");

    uint32_t a[] = {0};
    uint32_t b[] = {1};
    uint32_t result = 2;

    qc_error_t err = qc_cmp_qq_equal(ctx, a, b, 1, result);
    ASSERT(err == QC_OK, "qq_equal 1-bit failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

static void test_qq_equal_3bit(void) {
    TEST("qc_cmp_qq_equal 3-bit");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");

    uint32_t a[] = {0, 1, 2};
    uint32_t b[] = {3, 4, 5};
    uint32_t result = 6;

    qc_error_t err = qc_cmp_qq_equal(ctx, a, b, 3, result);
    ASSERT(err == QC_OK, "qq_equal 3-bit failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: QQ less-than comparison                                           */
/* ====================================================================== */

static void test_qq_less_2bit(void) {
    TEST("qc_cmp_qq_less 2-bit");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");

    uint32_t a[] = {0, 1};
    uint32_t b[] = {2, 3};
    uint32_t result = 4;

    qc_error_t err = qc_cmp_qq_less(ctx, a, b, 2, result);
    ASSERT(err == QC_OK, "qq_less 2-bit failed");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "no gates emitted");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Mode switching within same context                                */
/* ====================================================================== */

static void test_mode_switch(void) {
    TEST("mode switch QFT -> Toffoli within context");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");

    uint32_t a[] = {0, 1};
    uint32_t b[] = {2, 3};

    /* QFT mode first */
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);
    uint64_t before_qft = qc_circuit_gate_count(ctx);
    qc_dispatch_qq_add(ctx, a, b, 2);
    uint64_t after_qft = qc_circuit_gate_count(ctx);
    ASSERT(after_qft > before_qft, "QFT add produced no gates");

    /* Switch to Toffoli mode */
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);
    qc_dispatch_qq_add(ctx, a, b, 2);
    uint64_t after_toff = qc_circuit_gate_count(ctx);
    ASSERT(after_toff > after_qft, "Toffoli add produced no gates");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Different gate counts for QFT vs Toffoli                          */
/* ====================================================================== */

static void test_different_gate_counts(void) {
    TEST("QFT and Toffoli produce different gate counts");

    /* QFT mode */
    circuit_ctx_t *ctx_qft = qc_circuit_create(16);
    ASSERT(ctx_qft != NULL, "create qft failed");
    qc_circuit_set_arith_mode(ctx_qft, QC_ARITH_QFT);

    uint32_t a[] = {0, 1, 2, 3};
    uint32_t b[] = {4, 5, 6, 7};
    qc_dispatch_qq_add(ctx_qft, a, b, 4);
    uint64_t qft_count = qc_circuit_gate_count(ctx_qft);

    /* Toffoli mode */
    circuit_ctx_t *ctx_toff = qc_circuit_create(16);
    ASSERT(ctx_toff != NULL, "create toff failed");
    qc_circuit_set_arith_mode(ctx_toff, QC_ARITH_TOFFOLI);
    qc_dispatch_qq_add(ctx_toff, a, b, 4);
    uint64_t toff_count = qc_circuit_gate_count(ctx_toff);

    /* QFT and Toffoli should produce different gate counts */
    ASSERT(qft_count != toff_count,
           "QFT and Toffoli should differ in gate count");

    qc_circuit_destroy(ctx_qft);
    qc_circuit_destroy(ctx_toff);
    PASS();
}

/* ====================================================================== */
/* Test: NULL pointer handling                                             */
/* ====================================================================== */

static void test_null_handling(void) {
    TEST("NULL pointer handling");

    qc_error_t err;

    err = qc_dispatch_qq_add(NULL, NULL, NULL, 4);
    ASSERT(err == QC_ERR_NULL, "NULL ctx not caught for qq_add");

    err = qc_dispatch_cq_add(NULL, NULL, 4, 5);
    ASSERT(err == QC_ERR_NULL, "NULL ctx not caught for cq_add");

    err = qc_dispatch_qq_mul(NULL, NULL, NULL, NULL, 4);
    ASSERT(err == QC_ERR_NULL, "NULL ctx not caught for qq_mul");

    err = qc_dispatch_divmod_cq(NULL, NULL, 4, 3, NULL, NULL);
    ASSERT(err == QC_ERR_NULL, "NULL ctx not caught for divmod_cq");

    err = qc_dispatch_mod_reduce(NULL, NULL, 4, 5);
    ASSERT(err == QC_ERR_NULL, "NULL ctx not caught for mod_reduce");

    err = qc_cmp_qq_equal(NULL, NULL, NULL, 2, 0);
    ASSERT(err == QC_ERR_NULL, "NULL ctx not caught for qq_equal");

    err = qc_cmp_qq_less(NULL, NULL, NULL, 2, 0);
    ASSERT(err == QC_ERR_NULL, "NULL ctx not caught for qq_less");

    PASS();
}

/* ====================================================================== */
/* Test: Width validation                                                  */
/* ====================================================================== */

static void test_width_validation(void) {
    TEST("width validation");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create failed");

    uint32_t a[] = {0, 1};
    uint32_t b[] = {2, 3};

    /* Width 0 should fail */
    qc_error_t err = qc_dispatch_qq_sub(ctx, a, b, 0);
    ASSERT(err == QC_ERR_WIDTH, "width 0 not caught for qq_sub");

    /* Width 65 should fail */
    err = qc_dispatch_qq_sub(ctx, a, b, 65);
    ASSERT(err == QC_ERR_WIDTH, "width 65 not caught for qq_sub");

    err = qc_cmp_qq_equal(ctx, a, b, 0, 4);
    ASSERT(err == QC_ERR_WIDTH, "width 0 not caught for qq_equal");

    err = qc_cmp_qq_less(ctx, a, b, 0, 4);
    ASSERT(err == QC_ERR_WIDTH, "width 0 not caught for qq_less");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: Bitwise operations are mode-independent                           */
/* ====================================================================== */

static void test_bitwise_mode_independent(void) {
    TEST("bitwise ops mode-independent");

    uint32_t target[] = {0, 1, 2};

    /* QFT mode */
    circuit_ctx_t *ctx_qft = qc_circuit_create(16);
    ASSERT(ctx_qft != NULL, "create qft failed");
    qc_circuit_set_arith_mode(ctx_qft, QC_ARITH_QFT);
    qc_bitwise_not(ctx_qft, target, 3);
    uint64_t qft_count = qc_circuit_gate_count(ctx_qft);

    /* Toffoli mode */
    circuit_ctx_t *ctx_toff = qc_circuit_create(16);
    ASSERT(ctx_toff != NULL, "create toff failed");
    qc_circuit_set_arith_mode(ctx_toff, QC_ARITH_TOFFOLI);
    qc_bitwise_not(ctx_toff, target, 3);
    uint64_t toff_count = qc_circuit_gate_count(ctx_toff);

    /* Should be identical */
    ASSERT(qft_count == toff_count,
           "bitwise NOT should be same in both modes");
    ASSERT(qft_count == 3, "bitwise NOT on 3 bits should emit 3 gates");

    qc_circuit_destroy(ctx_qft);
    qc_circuit_destroy(ctx_toff);
    PASS();
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Module 1.13: Arithmetic Dispatch Tests (refactor-r5b) ===\n\n");

    /* Addition dispatch */
    test_dispatch_qq_add_qft();
    test_dispatch_qq_add_toffoli();
    test_dispatch_cq_add_qft();
    test_dispatch_cq_add_toffoli();
    test_dispatch_cqq_add_qft();
    test_dispatch_cqq_add_toffoli();
    test_dispatch_ccq_add_qft();
    test_dispatch_ccq_add_toffoli();

    /* Subtraction dispatch */
    test_dispatch_qq_sub();
    test_dispatch_cq_sub();

    /* Multiplication dispatch */
    test_dispatch_qq_mul_qft();
    test_dispatch_qq_mul_toffoli();
    test_dispatch_cq_mul_qft();

    /* Division dispatch */
    test_dispatch_divmod_cq_toffoli();
    test_dispatch_divmod_cq_qft_rejected();

    /* Modular dispatch */
    test_dispatch_mod_reduce_toffoli();
    test_dispatch_mod_reduce_qft_rejected();

    /* QQ comparison */
    test_qq_equal_1bit();
    test_qq_equal_3bit();
    test_qq_less_2bit();

    /* Mode switching */
    test_mode_switch();
    test_different_gate_counts();

    /* Error handling */
    test_null_handling();
    test_width_validation();
    test_bitwise_mode_independent();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
