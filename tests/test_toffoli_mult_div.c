/**
 * @file test_toffoli_mult_div.c
 * @brief Tests for Toffoli multiplication, division, and modular reduction.
 *
 * Module: 1.11 (Phase 1)
 * Issue: refactor-tpz
 *
 * Tests:
 *   1. Dynamic CDKM QQ addition (internal helper)
 *   2. Dynamic CQ addition (internal helper)
 *   3. CQ multiplication (qc_toffoli_cq_mul)
 *   4. QQ multiplication (qc_toffoli_qq_mul)
 *   5. CQ division (qc_toffoli_divmod_cq)
 *   6. Modular reduction (qc_toffoli_mod_reduce)
 *   7. Modular CQ addition (qc_toffoli_mod_add_cq)
 *
 * Max 17 qubits in simulations (structural/gate-count tests only,
 * no state-vector simulation).
 */

#include "quantum_circuit.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Test infrastructure                                                     */
/* ====================================================================== */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg)                                                  \
    do {                                                                   \
        tests_run++;                                                       \
        if (!(cond)) {                                                     \
            fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__);      \
            tests_failed++;                                                \
        } else {                                                           \
            tests_passed++;                                                \
        }                                                                  \
    } while (0)

/* ====================================================================== */
/* Test: context creation and gate emission basics                         */
/* ====================================================================== */

static void test_context_lifecycle(void) {
    printf("test_context_lifecycle...\n");
    circuit_ctx_t *ctx = qc_circuit_create(32);
    ASSERT(ctx != NULL, "context creation");

    /* Emit a few gates to verify context works */
    qc_circuit_x(ctx, 0);
    qc_circuit_cx(ctx, 0, 1);
    ASSERT(qc_circuit_gate_count(ctx) >= 2, "basic gate count");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: dynamic QQ addition emits gates                                   */
/* ====================================================================== */

static void test_dynamic_qq_add(void) {
    printf("test_dynamic_qq_add...\n");
    circuit_ctx_t *ctx = qc_circuit_create(32);
    ASSERT(ctx != NULL, "context for qq_add");

    /* 2-bit addition: a[0,1] += b[2,3] */
    uint32_t a[2] = {0, 1};
    uint32_t b[2] = {2, 3};

    qc_dynamic_qq_add(ctx, a, b, 2);
    uint64_t gc = qc_circuit_gate_count(ctx);
    ASSERT(gc > 0, "qq_add emits gates (2-bit)");

    /* 1-bit addition: single CX */
    qc_circuit_reset(ctx);
    uint32_t x[1] = {0};
    uint32_t y[1] = {1};
    qc_dynamic_qq_add(ctx, x, y, 1);
    gc = qc_circuit_gate_count(ctx);
    ASSERT(gc == 1, "qq_add 1-bit is single CX");

    /* 3-bit addition */
    qc_circuit_reset(ctx);
    uint32_t a3[3] = {0, 1, 2};
    uint32_t b3[3] = {3, 4, 5};
    qc_dynamic_qq_add(ctx, a3, b3, 3);
    gc = qc_circuit_gate_count(ctx);
    ASSERT(gc > 0, "qq_add emits gates (3-bit)");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: dynamic CQ addition emits gates                                   */
/* ====================================================================== */

static void test_dynamic_cq_add(void) {
    printf("test_dynamic_cq_add...\n");
    circuit_ctx_t *ctx = qc_circuit_create(32);
    ASSERT(ctx != NULL, "context for cq_add");

    /* 3-bit CQ add of value 3 */
    uint32_t target[3] = {0, 1, 2};
    qc_dynamic_cq_add(ctx, target, 3, 3);
    uint64_t gc = qc_circuit_gate_count(ctx);
    ASSERT(gc > 0, "cq_add emits gates (3-bit, value=3)");

    /* 1-bit CQ add of value 1 should emit X */
    qc_circuit_reset(ctx);
    uint32_t t1[1] = {0};
    qc_dynamic_cq_add(ctx, t1, 1, 1);
    gc = qc_circuit_gate_count(ctx);
    ASSERT(gc == 1, "cq_add 1-bit value=1 is single X");

    /* 1-bit CQ add of value 0 should be no-op */
    qc_circuit_reset(ctx);
    qc_dynamic_cq_add(ctx, t1, 1, 0);
    gc = qc_circuit_gate_count(ctx);
    ASSERT(gc == 0, "cq_add 1-bit value=0 is no-op");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: CQ multiplication                                                 */
/* ====================================================================== */

static void test_cq_multiplication(void) {
    printf("test_cq_multiplication...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context for cq_mul");

    /* 2-bit mul: result = target * 3
     * result[0..3], target[4,5], value=3 */
    uint32_t result[4] = {0, 1, 2, 3};
    uint32_t target[2] = {4, 5};

    qc_error_t err = qc_toffoli_cq_mul(ctx, result, 4, target, 2, 3);
    ASSERT(err == QC_OK, "cq_mul returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "cq_mul emits gates");

    /* Multiply by 0: should be no-op (result stays |0>) */
    qc_circuit_reset(ctx);
    err = qc_toffoli_cq_mul(ctx, result, 4, target, 2, 0);
    ASSERT(err == QC_OK, "cq_mul by 0 returns OK");
    ASSERT(qc_circuit_gate_count(ctx) == 0, "cq_mul by 0 is no-op");

    /* Multiply by 1: should emit CX gates */
    qc_circuit_reset(ctx);
    err = qc_toffoli_cq_mul(ctx, result, 4, target, 2, 1);
    ASSERT(err == QC_OK, "cq_mul by 1 returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "cq_mul by 1 emits gates");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: QQ multiplication                                                 */
/* ====================================================================== */

static void test_qq_multiplication(void) {
    printf("test_qq_multiplication...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context for qq_mul");

    /* 2-bit QQ mul: result[0..3] = a[4,5] * b[6,7] */
    uint32_t result[4] = {0, 1, 2, 3};
    uint32_t a[2] = {4, 5};
    uint32_t b[2] = {6, 7};

    qc_error_t err = qc_toffoli_qq_mul(ctx, result, 4, a, 2, b, 2);
    ASSERT(err == QC_OK, "qq_mul returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "qq_mul emits gates");

    /* 1-bit QQ mul: result[0,1] = a[2] * b[3] = single CCX */
    qc_circuit_reset(ctx);
    uint32_t r1[2] = {0, 1};
    uint32_t a1[1] = {2};
    uint32_t b1[1] = {3};
    err = qc_toffoli_qq_mul(ctx, r1, 2, a1, 1, b1, 1);
    ASSERT(err == QC_OK, "qq_mul 1-bit returns OK");

    /* Null checks */
    err = qc_toffoli_qq_mul(NULL, result, 4, a, 2, b, 2);
    ASSERT(err == QC_ERR_NULL, "qq_mul null ctx");

    err = qc_toffoli_qq_mul(ctx, NULL, 4, a, 2, b, 2);
    ASSERT(err == QC_ERR_NULL, "qq_mul null result");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: CQ division                                                       */
/* ====================================================================== */

static void test_cq_division(void) {
    printf("test_cq_division...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context for cq_div");

    /* 3-bit divmod: dividend[0-2] / 3 -> quotient[3-5], remainder[6-8] */
    uint32_t dividend[3]  = {0, 1, 2};
    uint32_t quotient[3]  = {3, 4, 5};
    uint32_t remainder[3] = {6, 7, 8};

    qc_error_t err = qc_toffoli_divmod_cq(ctx, dividend, 3, 3,
                                            quotient, remainder);
    ASSERT(err == QC_OK, "cq_divmod returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "cq_divmod emits gates");

    /* Division by zero: should set all quotient bits */
    qc_circuit_reset(ctx);
    err = qc_toffoli_divmod_cq(ctx, dividend, 3, 0, quotient, remainder);
    ASSERT(err == QC_ERR_DIVISOR, "cq_divmod by 0 returns ERR_DIVISOR");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "cq_divmod by 0 emits gates");

    /* Null checks */
    err = qc_toffoli_divmod_cq(NULL, dividend, 3, 3, quotient, remainder);
    ASSERT(err == QC_ERR_NULL, "cq_divmod null ctx");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: modular reduction                                                 */
/* ====================================================================== */

static void test_mod_reduce(void) {
    printf("test_mod_reduce...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context for mod_reduce");

    /* 3-bit mod reduce, modulus = 5 */
    uint32_t value[3] = {0, 1, 2};

    qc_error_t err = qc_toffoli_mod_reduce(ctx, value, 3, 5);
    ASSERT(err == QC_OK, "mod_reduce returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "mod_reduce emits gates");

    /* Invalid modulus */
    qc_circuit_reset(ctx);
    err = qc_toffoli_mod_reduce(ctx, value, 3, 0);
    ASSERT(err == QC_ERR_DIVISOR, "mod_reduce modulus=0");

    err = qc_toffoli_mod_reduce(ctx, value, 3, -1);
    ASSERT(err == QC_ERR_DIVISOR, "mod_reduce modulus=-1");

    /* Null checks */
    err = qc_toffoli_mod_reduce(NULL, value, 3, 5);
    ASSERT(err == QC_ERR_NULL, "mod_reduce null ctx");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: Beauregard modular CQ addition                                    */
/* ====================================================================== */

static void test_mod_add_cq(void) {
    printf("test_mod_add_cq...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context for mod_add_cq");

    /* 3-bit mod add: value += 2 mod 5 */
    uint32_t value[3] = {0, 1, 2};

    qc_error_t err = qc_toffoli_mod_add_cq(ctx, value, 3, 2, 5);
    ASSERT(err == QC_OK, "mod_add_cq returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "mod_add_cq emits gates");

    /* Adding 0 should be no-op */
    qc_circuit_reset(ctx);
    err = qc_toffoli_mod_add_cq(ctx, value, 3, 0, 5);
    ASSERT(err == QC_OK, "mod_add_cq addend=0 OK");
    ASSERT(qc_circuit_gate_count(ctx) == 0, "mod_add_cq addend=0 no-op");

    /* Adding modulus should be no-op (5 mod 5 = 0) */
    qc_circuit_reset(ctx);
    err = qc_toffoli_mod_add_cq(ctx, value, 3, 5, 5);
    ASSERT(err == QC_OK, "mod_add_cq addend=modulus OK");
    ASSERT(qc_circuit_gate_count(ctx) == 0, "mod_add_cq addend=modulus no-op");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: modular CQ multiplication                                         */
/* ====================================================================== */

static void test_mod_mul_cq(void) {
    printf("test_mod_mul_cq...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context for mod_mul_cq");

    /* 3-bit mod mul: result = value * 3 mod 5 */
    uint32_t value[3]  = {0, 1, 2};
    uint32_t result[3] = {3, 4, 5};

    qc_error_t err = qc_toffoli_mod_mul_cq(ctx, value, 3, result, 3, 3, 5);
    ASSERT(err == QC_OK, "mod_mul_cq returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "mod_mul_cq emits gates");

    /* Multiply by 0 = no-op */
    qc_circuit_reset(ctx);
    err = qc_toffoli_mod_mul_cq(ctx, value, 3, result, 3, 0, 5);
    ASSERT(err == QC_OK, "mod_mul_cq by 0 OK");
    ASSERT(qc_circuit_gate_count(ctx) == 0, "mod_mul_cq by 0 no-op");

    /* Multiply by 1 = copy */
    qc_circuit_reset(ctx);
    err = qc_toffoli_mod_mul_cq(ctx, value, 3, result, 3, 1, 5);
    ASSERT(err == QC_OK, "mod_mul_cq by 1 OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "mod_mul_cq by 1 emits CX gates");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: Clifford+T decomposition mode                                     */
/* ====================================================================== */

static void test_clifford_t_mode(void) {
    printf("test_clifford_t_mode...\n");

    /* Without decomposition */
    circuit_ctx_t *ctx1 = qc_circuit_create(32);
    ASSERT(ctx1 != NULL, "ctx1 creation");
    qc_circuit_set_toffoli_decompose(ctx1, false);

    uint32_t a[2] = {0, 1};
    uint32_t b[2] = {2, 3};
    qc_dynamic_qq_add(ctx1, a, b, 2);
    uint64_t gc_normal = qc_circuit_gate_count(ctx1);

    /* With decomposition: should produce more gates */
    circuit_ctx_t *ctx2 = qc_circuit_create(32);
    ASSERT(ctx2 != NULL, "ctx2 creation");
    qc_circuit_set_toffoli_decompose(ctx2, true);

    qc_dynamic_qq_add(ctx2, a, b, 2);
    uint64_t gc_decomp = qc_circuit_gate_count(ctx2);

    ASSERT(gc_decomp > gc_normal,
           "Clifford+T mode produces more gates");

    qc_circuit_destroy(ctx1);
    qc_circuit_destroy(ctx2);
}

/* ====================================================================== */
/* Test: width boundary cases                                              */
/* ====================================================================== */

static void test_width_boundaries(void) {
    printf("test_width_boundaries...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context for boundaries");

    /* Width 0: should return width error */
    qc_error_t err = qc_toffoli_qq_mul(ctx, NULL, 0, NULL, 0, NULL, 0);
    ASSERT(err != QC_OK, "qq_mul width=0 rejected");

    /* Width 1 multiplication: minimal case */
    qc_circuit_reset(ctx);
    uint32_t r[1] = {0};
    uint32_t a[1] = {1};
    uint32_t b[1] = {2};
    err = qc_toffoli_qq_mul(ctx, r, 1, a, 1, b, 1);
    ASSERT(err == QC_OK, "qq_mul width=1 OK");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Test Toffoli Multiplication / Division / Mod Reduce ===\n\n");

    test_context_lifecycle();
    test_dynamic_qq_add();
    test_dynamic_cq_add();
    test_cq_multiplication();
    test_qq_multiplication();
    test_cq_division();
    test_mod_reduce();
    test_mod_add_cq();
    test_mod_mul_cq();
    test_clifford_t_mode();
    test_width_boundaries();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
