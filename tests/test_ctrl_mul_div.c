/**
 * @file test_ctrl_mul_div.c
 * @brief Tests for controlled Toffoli multiplication and division.
 *
 * Issue: refactor-d8y
 *
 * Tests the externally-controlled variants:
 *   qc_toffoli_cmul_qq   -- controlled quantum * quantum multiplication
 *   qc_toffoli_cmul_cq   -- controlled quantum * classical multiplication
 *   qc_toffoli_cdivmod_cq -- controlled CQ division
 *   qc_toffoli_cdivmod_qq -- controlled QQ division
 *
 * Structural / gate-count tests only (no state-vector simulation).
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
/* Test 1: cmul_qq basic — 4-bit controlled QQ mul, gate count > 0        */
/* ====================================================================== */

static void test_cmul_qq_basic(void) {
    printf("test_cmul_qq_basic...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context creation");

    /* result[0..3], a[4..7], b[8..11], ext_ctrl=12 */
    uint32_t result[4] = {0, 1, 2, 3};
    uint32_t a[4]      = {4, 5, 6, 7};
    uint32_t b[4]      = {8, 9, 10, 11};
    uint32_t ext_ctrl  = 12;

    qc_error_t err = qc_toffoli_cmul_qq(ctx, result, 4, a, 4, b, 4, ext_ctrl);
    ASSERT(err == QC_OK, "cmul_qq returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "cmul_qq emits gates");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test 2: cmul_qq gate count vs uncontrolled                              */
/* ====================================================================== */

static void test_cmul_qq_gate_count_vs_uncontrolled(void) {
    printf("test_cmul_qq_gate_count_vs_uncontrolled...\n");

    /* Uncontrolled QQ mul */
    circuit_ctx_t *ctx1 = qc_circuit_create(64);
    ASSERT(ctx1 != NULL, "ctx1 creation");
    uint32_t result1[4] = {0, 1, 2, 3};
    uint32_t a1[4]      = {4, 5, 6, 7};
    uint32_t b1[4]      = {8, 9, 10, 11};
    qc_error_t err1 = qc_toffoli_qq_mul(ctx1, result1, 4, a1, 4, b1, 4);
    ASSERT(err1 == QC_OK, "qq_mul returns OK");
    uint64_t gc_uncontrolled = qc_circuit_gate_count(ctx1);

    /* Controlled QQ mul */
    circuit_ctx_t *ctx2 = qc_circuit_create(64);
    ASSERT(ctx2 != NULL, "ctx2 creation");
    uint32_t result2[4] = {0, 1, 2, 3};
    uint32_t a2[4]      = {4, 5, 6, 7};
    uint32_t b2[4]      = {8, 9, 10, 11};
    uint32_t ext_ctrl   = 12;
    qc_error_t err2 = qc_toffoli_cmul_qq(ctx2, result2, 4, a2, 4, b2, 4, ext_ctrl);
    ASSERT(err2 == QC_OK, "cmul_qq returns OK");
    uint64_t gc_controlled = qc_circuit_gate_count(ctx2);

    ASSERT(gc_controlled > gc_uncontrolled,
           "controlled QQ mul emits MORE gates than uncontrolled");

    qc_circuit_destroy(ctx1);
    qc_circuit_destroy(ctx2);
}

/* ====================================================================== */
/* Test 3: cmul_cq basic — 4-bit controlled CQ mul with value=3           */
/* ====================================================================== */

static void test_cmul_cq_basic(void) {
    printf("test_cmul_cq_basic...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context creation");

    /* result[0..3], target[4..7], value=3, ext_ctrl=8 */
    uint32_t result[4] = {0, 1, 2, 3};
    uint32_t target[4] = {4, 5, 6, 7};
    uint32_t ext_ctrl  = 8;

    qc_error_t err = qc_toffoli_cmul_cq(ctx, result, 4, target, 4, 3, ext_ctrl);
    ASSERT(err == QC_OK, "cmul_cq returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "cmul_cq emits gates");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test 4: cmul_cq zero value — should emit zero gates                     */
/* ====================================================================== */

static void test_cmul_cq_zero_value(void) {
    printf("test_cmul_cq_zero_value...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context creation");

    uint32_t result[4] = {0, 1, 2, 3};
    uint32_t target[4] = {4, 5, 6, 7};
    uint32_t ext_ctrl  = 8;

    qc_error_t err = qc_toffoli_cmul_cq(ctx, result, 4, target, 4, 0, ext_ctrl);
    ASSERT(err == QC_OK, "cmul_cq value=0 returns OK");
    ASSERT(qc_circuit_gate_count(ctx) == 0, "cmul_cq value=0 emits zero gates");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test 5: cmul_qq NULL ctx returns QC_ERR_NULL                            */
/* ====================================================================== */

static void test_cmul_qq_null_ctx(void) {
    printf("test_cmul_qq_null_ctx...\n");
    uint32_t result[4] = {0, 1, 2, 3};
    uint32_t a[4]      = {4, 5, 6, 7};
    uint32_t b[4]      = {8, 9, 10, 11};

    qc_error_t err = qc_toffoli_cmul_qq(NULL, result, 4, a, 4, b, 4, 12);
    ASSERT(err == QC_ERR_NULL, "cmul_qq null ctx returns QC_ERR_NULL");
}

/* ====================================================================== */
/* Test 6: cmul_cq NULL ctx returns QC_ERR_NULL                            */
/* ====================================================================== */

static void test_cmul_cq_null_ctx(void) {
    printf("test_cmul_cq_null_ctx...\n");
    uint32_t result[4] = {0, 1, 2, 3};
    uint32_t target[4] = {4, 5, 6, 7};

    qc_error_t err = qc_toffoli_cmul_cq(NULL, result, 4, target, 4, 3, 8);
    ASSERT(err == QC_ERR_NULL, "cmul_cq null ctx returns QC_ERR_NULL");
}

/* ====================================================================== */
/* Test 7: cdivmod_cq basic — 4-bit controlled CQ divmod with divisor=3   */
/* ====================================================================== */

static void test_cdivmod_cq_basic(void) {
    printf("test_cdivmod_cq_basic...\n");
    circuit_ctx_t *ctx = qc_circuit_create(128);
    ASSERT(ctx != NULL, "context creation");

    /* dividend[0..3], quotient[4..7], remainder[8..11], ext_ctrl=12 */
    uint32_t dividend[4]  = {0, 1, 2, 3};
    uint32_t quotient[4]  = {4, 5, 6, 7};
    uint32_t remainder[4] = {8, 9, 10, 11};
    uint32_t ext_ctrl     = 12;

    qc_error_t err = qc_toffoli_cdivmod_cq(ctx, dividend, 4, 3,
                                             quotient, remainder, ext_ctrl);
    ASSERT(err == QC_OK, "cdivmod_cq returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "cdivmod_cq emits gates");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test 8: cdivmod_cq gate count vs uncontrolled                           */
/* ====================================================================== */

static void test_cdivmod_cq_gate_count_vs_uncontrolled(void) {
    printf("test_cdivmod_cq_gate_count_vs_uncontrolled...\n");

    /* Uncontrolled CQ divmod */
    circuit_ctx_t *ctx1 = qc_circuit_create(128);
    ASSERT(ctx1 != NULL, "ctx1 creation");
    uint32_t dividend1[4]  = {0, 1, 2, 3};
    uint32_t quotient1[4]  = {4, 5, 6, 7};
    uint32_t remainder1[4] = {8, 9, 10, 11};
    qc_error_t err1 = qc_toffoli_divmod_cq(ctx1, dividend1, 4, 3,
                                             quotient1, remainder1);
    ASSERT(err1 == QC_OK, "divmod_cq returns OK");
    uint64_t gc_uncontrolled = qc_circuit_gate_count(ctx1);

    /* Controlled CQ divmod */
    circuit_ctx_t *ctx2 = qc_circuit_create(128);
    ASSERT(ctx2 != NULL, "ctx2 creation");
    uint32_t dividend2[4]  = {0, 1, 2, 3};
    uint32_t quotient2[4]  = {4, 5, 6, 7};
    uint32_t remainder2[4] = {8, 9, 10, 11};
    uint32_t ext_ctrl      = 12;
    qc_error_t err2 = qc_toffoli_cdivmod_cq(ctx2, dividend2, 4, 3,
                                              quotient2, remainder2, ext_ctrl);
    ASSERT(err2 == QC_OK, "cdivmod_cq returns OK");
    uint64_t gc_controlled = qc_circuit_gate_count(ctx2);

    ASSERT(gc_controlled > gc_uncontrolled,
           "controlled CQ divmod emits MORE gates than uncontrolled");

    qc_circuit_destroy(ctx1);
    qc_circuit_destroy(ctx2);
}

/* ====================================================================== */
/* Test 9: cdivmod_qq basic — 4-bit controlled QQ divmod                   */
/* ====================================================================== */

static void test_cdivmod_qq_basic(void) {
    printf("test_cdivmod_qq_basic...\n");
    circuit_ctx_t *ctx = qc_circuit_create(128);
    ASSERT(ctx != NULL, "context creation");

    /* dividend[0..3], divisor[4..7], quotient[8..11], remainder[12..15], ext_ctrl=16 */
    uint32_t dividend[4]  = {0, 1, 2, 3};
    uint32_t divisor[4]   = {4, 5, 6, 7};
    uint32_t quotient[4]  = {8, 9, 10, 11};
    uint32_t remainder[4] = {12, 13, 14, 15};
    uint32_t ext_ctrl     = 16;

    qc_error_t err = qc_toffoli_cdivmod_qq(ctx, dividend, 4, divisor, 4,
                                             quotient, remainder, ext_ctrl);
    ASSERT(err == QC_OK, "cdivmod_qq returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "cdivmod_qq emits gates");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test 10: cdivmod_cq NULL ctx returns QC_ERR_NULL                        */
/* ====================================================================== */

static void test_cdivmod_cq_null_ctx(void) {
    printf("test_cdivmod_cq_null_ctx...\n");
    uint32_t dividend[4]  = {0, 1, 2, 3};
    uint32_t quotient[4]  = {4, 5, 6, 7};
    uint32_t remainder[4] = {8, 9, 10, 11};

    qc_error_t err = qc_toffoli_cdivmod_cq(NULL, dividend, 4, 3,
                                             quotient, remainder, 12);
    ASSERT(err == QC_ERR_NULL, "cdivmod_cq null ctx returns QC_ERR_NULL");
}

/* ====================================================================== */
/* Test 11: cdivmod_qq NULL ctx returns QC_ERR_NULL                        */
/* ====================================================================== */

static void test_cdivmod_qq_null_ctx(void) {
    printf("test_cdivmod_qq_null_ctx...\n");
    uint32_t dividend[4]  = {0, 1, 2, 3};
    uint32_t divisor[4]   = {4, 5, 6, 7};
    uint32_t quotient[4]  = {8, 9, 10, 11};
    uint32_t remainder[4] = {12, 13, 14, 15};

    qc_error_t err = qc_toffoli_cdivmod_qq(NULL, dividend, 4, divisor, 4,
                                             quotient, remainder, 16);
    ASSERT(err == QC_ERR_NULL, "cdivmod_qq null ctx returns QC_ERR_NULL");
}

/* ====================================================================== */
/* Test 12: cmul_qq QASM export contains "ccx" gates                       */
/* ====================================================================== */

static void test_cmul_qq_qasm_export(void) {
    printf("test_cmul_qq_qasm_export...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context creation");

    uint32_t result[4] = {0, 1, 2, 3};
    uint32_t a[4]      = {4, 5, 6, 7};
    uint32_t b[4]      = {8, 9, 10, 11};
    uint32_t ext_ctrl  = 12;

    qc_error_t err = qc_toffoli_cmul_qq(ctx, result, 4, a, 4, b, 4, ext_ctrl);
    ASSERT(err == QC_OK, "cmul_qq returns OK for QASM test");

    char *qasm = qc_circuit_to_qasm(ctx);
    ASSERT(qasm != NULL, "QASM export not NULL");
    ASSERT(strstr(qasm, "ccx") != NULL, "QASM contains ccx gates");

    free(qasm);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test 13: cdivmod_cq QASM export contains "ccx" gates                    */
/* ====================================================================== */

static void test_cdivmod_cq_qasm_export(void) {
    printf("test_cdivmod_cq_qasm_export...\n");
    circuit_ctx_t *ctx = qc_circuit_create(128);
    ASSERT(ctx != NULL, "context creation");

    uint32_t dividend[4]  = {0, 1, 2, 3};
    uint32_t quotient[4]  = {4, 5, 6, 7};
    uint32_t remainder[4] = {8, 9, 10, 11};
    uint32_t ext_ctrl     = 12;

    qc_error_t err = qc_toffoli_cdivmod_cq(ctx, dividend, 4, 3,
                                             quotient, remainder, ext_ctrl);
    ASSERT(err == QC_OK, "cdivmod_cq returns OK for QASM test");

    char *qasm = qc_circuit_to_qasm(ctx);
    ASSERT(qasm != NULL, "QASM export not NULL");
    ASSERT(strstr(qasm, "ccx") != NULL, "QASM contains ccx gates");

    free(qasm);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Test Controlled Toffoli Multiplication / Division ===\n\n");

    test_cmul_qq_basic();
    test_cmul_qq_gate_count_vs_uncontrolled();
    test_cmul_cq_basic();
    test_cmul_cq_zero_value();
    test_cmul_qq_null_ctx();
    test_cmul_cq_null_ctx();
    test_cdivmod_cq_basic();
    test_cdivmod_cq_gate_count_vs_uncontrolled();
    test_cdivmod_qq_basic();
    test_cdivmod_cq_null_ctx();
    test_cdivmod_qq_null_ctx();
    test_cmul_qq_qasm_export();
    test_cdivmod_cq_qasm_export();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
