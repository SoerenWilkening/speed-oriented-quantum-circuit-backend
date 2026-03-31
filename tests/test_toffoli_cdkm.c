/**
 * @file test_toffoli_cdkm.c
 * @brief Tests for toffoli_cdkm.c and toffoli_helpers.c (Module 1.9).
 *
 * Issue: refactor-w6f
 *
 * Tests:
 *   1. qc_two_complement: binary conversion for various values
 *   2. qc_toffoli_seq_alloc / qc_toffoli_seq_free: lifecycle
 *   3. qc_toffoli_qq_add: widths 1-8, gate count checks
 *   4. qc_toffoli_cq_add: widths 1-8, various classical values
 *   5. qc_toffoli_cqq_add: widths 1-4, gate count checks
 *   6. qc_toffoli_ccq_add: widths 1-4, various classical values
 *   7. Edge cases: width 1, value 0, negative values
 *   8. NULL safety and error codes
 *
 * Max qubits: 17 (per constraint).
 */

#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================== */
/* Test framework macros                                                   */
/* ====================================================================== */

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b, msg)                                                \
    do {                                                                    \
        long _a = (long)(a), _b = (long)(b);                               \
        if (_a == _b) {                                                     \
            tests_passed++;                                                 \
        } else {                                                            \
            fprintf(stderr, "FAIL [%s:%d]: %s: expected %ld, got %ld\n",   \
                    __FILE__, __LINE__, (msg), _b, _a);                    \
            tests_failed++;                                                 \
        }                                                                   \
    } while (0)

#define ASSERT_TRUE(cond, msg)                                              \
    do {                                                                    \
        if ((cond)) {                                                       \
            tests_passed++;                                                 \
        } else {                                                            \
            fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg)); \
            tests_failed++;                                                 \
        }                                                                   \
    } while (0)

#define ASSERT_OK(err, msg)  ASSERT_EQ((err), QC_OK, (msg))

/* qc_toffoli_seq_alloc, qc_toffoli_seq_free, qc_two_complement
 * are declared in internal.h (included above) */

/* ====================================================================== */
/* Test: qc_two_complement                                                 */
/* ====================================================================== */

static void test_two_complement(void) {
    printf("  test_two_complement...\n");

    /* 3 in 4 bits = 0011 -> bin[0]=0, bin[1]=0, bin[2]=1, bin[3]=1 */
    int *bin = qc_two_complement(3, 4);
    ASSERT_TRUE(bin != NULL, "two_complement(3,4) non-null");
    if (bin) {
        ASSERT_EQ(bin[0], 0, "3 in 4 bits: MSB=0");
        ASSERT_EQ(bin[1], 0, "3 in 4 bits: bit 2=0");
        ASSERT_EQ(bin[2], 1, "3 in 4 bits: bit 1=1");
        ASSERT_EQ(bin[3], 1, "3 in 4 bits: LSB=1");
        free(bin);
    }

    /* 1 in 1 bit = 1 */
    bin = qc_two_complement(1, 1);
    ASSERT_TRUE(bin != NULL, "two_complement(1,1) non-null");
    if (bin) {
        ASSERT_EQ(bin[0], 1, "1 in 1 bit: LSB=1");
        free(bin);
    }

    /* 0 in 4 bits = 0000 */
    bin = qc_two_complement(0, 4);
    ASSERT_TRUE(bin != NULL, "two_complement(0,4) non-null");
    if (bin) {
        ASSERT_EQ(bin[0], 0, "0 bit3");
        ASSERT_EQ(bin[1], 0, "0 bit2");
        ASSERT_EQ(bin[2], 0, "0 bit1");
        ASSERT_EQ(bin[3], 0, "0 bit0");
        free(bin);
    }

    /* -1 in 4 bits = 1111 (two's complement) */
    bin = qc_two_complement(-1, 4);
    ASSERT_TRUE(bin != NULL, "two_complement(-1,4) non-null");
    if (bin) {
        ASSERT_EQ(bin[0], 1, "-1 bit3");
        ASSERT_EQ(bin[1], 1, "-1 bit2");
        ASSERT_EQ(bin[2], 1, "-1 bit1");
        ASSERT_EQ(bin[3], 1, "-1 bit0");
        free(bin);
    }

    /* Invalid: n=0 */
    bin = qc_two_complement(5, 0);
    ASSERT_TRUE(bin == NULL, "two_complement with n=0 returns NULL");

    printf("  test_two_complement DONE\n");
}

/* ====================================================================== */
/* Test: qc_toffoli_seq_alloc / qc_toffoli_seq_free                        */
/* ====================================================================== */

static void test_seq_alloc_free(void) {
    printf("  test_seq_alloc_free...\n");

    qc_sequence_t *seq = qc_toffoli_seq_alloc(5);
    ASSERT_TRUE(seq != NULL, "alloc(5) non-null");
    if (seq) {
        ASSERT_EQ(seq->num_layer, 5, "num_layer=5");
        ASSERT_EQ(seq->used_layer, 0, "used_layer=0");
        ASSERT_EQ(seq->total_gate_count, 0, "total_gate_count=0");
        ASSERT_TRUE(seq->gates_per_layer != NULL, "gates_per_layer non-null");
        ASSERT_TRUE(seq->seq != NULL, "seq non-null");
        qc_toffoli_seq_free(seq);
    }

    /* alloc(0) -> NULL */
    seq = qc_toffoli_seq_alloc(0);
    ASSERT_TRUE(seq == NULL, "alloc(0) returns NULL");

    /* free(NULL) is safe */
    qc_toffoli_seq_free(NULL);
    tests_passed++; /* survived */

    printf("  test_seq_alloc_free DONE\n");
}

/* ====================================================================== */
/* Test: qc_toffoli_qq_add                                                 */
/* ====================================================================== */

static void test_qq_add(void) {
    printf("  test_qq_add...\n");

    /* Width 1: should produce 1 CX gate */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        ASSERT_TRUE(ctx != NULL, "ctx created for qq_add w=1");
        qc_circuit_set_simulate(ctx, true);

        uint32_t a_q[1] = {0};
        uint32_t b_q[1] = {1};
        qc_error_t err = qc_toffoli_qq_add(ctx, a_q, b_q, 1);
        ASSERT_OK(err, "qq_add w=1 OK");
        ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "qq_add w=1: 1 gate");

        qc_circuit_destroy(ctx);
    }

    /* Width 2: 6*2 = 12 layers, each 1 gate = 12 gates (6 CX + 6 CCX) */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t a_q[2] = {0, 1};
        uint32_t b_q[2] = {2, 3};
        qc_error_t err = qc_toffoli_qq_add(ctx, a_q, b_q, 2);
        ASSERT_OK(err, "qq_add w=2 OK");
        /* 2 MAJ (3 gates each) + 2 UMA (3 gates each) = 12 gates */
        ASSERT_EQ(qc_circuit_gate_count(ctx), 12, "qq_add w=2: 12 gates");

        qc_circuit_destroy(ctx);
    }

    /* Width 4 */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t a_q[4] = {0, 1, 2, 3};
        uint32_t b_q[4] = {4, 5, 6, 7};
        qc_error_t err = qc_toffoli_qq_add(ctx, a_q, b_q, 4);
        ASSERT_OK(err, "qq_add w=4 OK");
        /* 4 MAJ + 4 UMA = 24 gates */
        ASSERT_EQ(qc_circuit_gate_count(ctx), 24, "qq_add w=4: 24 gates");

        qc_circuit_destroy(ctx);
    }

    /* Count-only mode: width 3 */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, false);

        uint32_t a_q[3] = {0, 1, 2};
        uint32_t b_q[3] = {3, 4, 5};
        qc_error_t err = qc_toffoli_qq_add(ctx, a_q, b_q, 3);
        ASSERT_OK(err, "qq_add w=3 count-only OK");

        qc_circuit_destroy(ctx);
    }

    printf("  test_qq_add DONE\n");
}

/* ====================================================================== */
/* Test: qc_toffoli_cq_add                                                 */
/* ====================================================================== */

static void test_cq_add(void) {
    printf("  test_cq_add...\n");

    /* Width 1, value=1: single X gate */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[1] = {0};
        qc_error_t err = qc_toffoli_cq_add(ctx, tgt, 1, 1);
        ASSERT_OK(err, "cq_add w=1 v=1 OK");
        ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "cq_add w=1 v=1: 1 gate");

        qc_circuit_destroy(ctx);
    }

    /* Width 1, value=0: identity */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[1] = {0};
        qc_error_t err = qc_toffoli_cq_add(ctx, tgt, 1, 0);
        ASSERT_OK(err, "cq_add w=1 v=0 OK");
        ASSERT_EQ(qc_circuit_gate_count(ctx), 0, "cq_add w=1 v=0: 0 gates");

        qc_circuit_destroy(ctx);
    }

    /* Width 2, value=1 (binary 01): CQ simplification */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[2] = {0, 1};
        qc_error_t err = qc_toffoli_cq_add(ctx, tgt, 2, 1);
        ASSERT_OK(err, "cq_add w=2 v=1 OK");
        ASSERT_TRUE(qc_circuit_gate_count(ctx) > 0, "cq_add w=2 v=1: gates emitted");

        qc_circuit_destroy(ctx);
    }

    /* Width 4, value=5: should produce gates */
    {
        circuit_ctx_t *ctx = qc_circuit_create(32);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[4] = {0, 1, 2, 3};
        qc_error_t err = qc_toffoli_cq_add(ctx, tgt, 4, 5);
        ASSERT_OK(err, "cq_add w=4 v=5 OK");
        ASSERT_TRUE(qc_circuit_gate_count(ctx) > 0, "cq_add w=4 v=5: gates emitted");

        qc_circuit_destroy(ctx);
    }

    /* Width 3, value=-1 (all 1s in two's complement) */
    {
        circuit_ctx_t *ctx = qc_circuit_create(32);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[3] = {0, 1, 2};
        qc_error_t err = qc_toffoli_cq_add(ctx, tgt, 3, -1);
        ASSERT_OK(err, "cq_add w=3 v=-1 OK");
        ASSERT_TRUE(qc_circuit_gate_count(ctx) > 0, "cq_add w=3 v=-1: gates emitted");

        qc_circuit_destroy(ctx);
    }

    printf("  test_cq_add DONE\n");
}

/* ====================================================================== */
/* Test: qc_toffoli_cqq_add                                               */
/* ====================================================================== */

static void test_cqq_add(void) {
    printf("  test_cqq_add...\n");

    /* Width 1: single CCX */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t a_q[1] = {0};
        uint32_t b_q[1] = {1};
        uint32_t ctrl = 2;
        qc_error_t err = qc_toffoli_cqq_add(ctx, a_q, b_q, 1, ctrl);
        ASSERT_OK(err, "cqq_add w=1 OK");
        ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "cqq_add w=1: 1 gate (CCX)");

        qc_circuit_destroy(ctx);
    }

    /* Width 2: 10*2 = 20 gates (all CCX) */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t a_q[2] = {0, 1};
        uint32_t b_q[2] = {2, 3};
        uint32_t ctrl = 4;
        qc_error_t err = qc_toffoli_cqq_add(ctx, a_q, b_q, 2, ctrl);
        ASSERT_OK(err, "cqq_add w=2 OK");
        /* 2 cMAJ (5 each) + 2 cUMA (5 each) = 20 */
        ASSERT_EQ(qc_circuit_gate_count(ctx), 20, "cqq_add w=2: 20 gates");

        qc_circuit_destroy(ctx);
    }

    /* Width 3 */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t a_q[3] = {0, 1, 2};
        uint32_t b_q[3] = {3, 4, 5};
        uint32_t ctrl = 6;
        qc_error_t err = qc_toffoli_cqq_add(ctx, a_q, b_q, 3, ctrl);
        ASSERT_OK(err, "cqq_add w=3 OK");
        /* 3 cMAJ (5 each) + 3 cUMA (5 each) = 30 */
        ASSERT_EQ(qc_circuit_gate_count(ctx), 30, "cqq_add w=3: 30 gates");

        qc_circuit_destroy(ctx);
    }

    printf("  test_cqq_add DONE\n");
}

/* ====================================================================== */
/* Test: qc_toffoli_ccq_add                                               */
/* ====================================================================== */

static void test_ccq_add(void) {
    printf("  test_ccq_add...\n");

    /* Width 1, value=1: single CX */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[1] = {0};
        uint32_t ctrl = 1;
        qc_error_t err = qc_toffoli_ccq_add(ctx, tgt, 1, 1, ctrl);
        ASSERT_OK(err, "ccq_add w=1 v=1 OK");
        ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "ccq_add w=1 v=1: 1 gate (CX)");

        qc_circuit_destroy(ctx);
    }

    /* Width 1, value=0: identity */
    {
        circuit_ctx_t *ctx = qc_circuit_create(16);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[1] = {0};
        uint32_t ctrl = 1;
        qc_error_t err = qc_toffoli_ccq_add(ctx, tgt, 1, 0, ctrl);
        ASSERT_OK(err, "ccq_add w=1 v=0 OK");
        ASSERT_EQ(qc_circuit_gate_count(ctx), 0, "ccq_add w=1 v=0: 0 gates");

        qc_circuit_destroy(ctx);
    }

    /* Width 2, value=1: cCQ with classical bit simplification */
    {
        circuit_ctx_t *ctx = qc_circuit_create(32);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[2] = {0, 1};
        uint32_t ctrl = 2;
        qc_error_t err = qc_toffoli_ccq_add(ctx, tgt, 2, 1, ctrl);
        ASSERT_OK(err, "ccq_add w=2 v=1 OK");
        ASSERT_TRUE(qc_circuit_gate_count(ctx) > 0, "ccq_add w=2 v=1: gates emitted");

        qc_circuit_destroy(ctx);
    }

    /* Width 3, value=5 */
    {
        circuit_ctx_t *ctx = qc_circuit_create(32);
        qc_circuit_set_simulate(ctx, true);

        uint32_t tgt[3] = {0, 1, 2};
        uint32_t ctrl = 3;
        qc_error_t err = qc_toffoli_ccq_add(ctx, tgt, 3, 5, ctrl);
        ASSERT_OK(err, "ccq_add w=3 v=5 OK");
        ASSERT_TRUE(qc_circuit_gate_count(ctx) > 0, "ccq_add w=3 v=5: gates emitted");

        qc_circuit_destroy(ctx);
    }

    printf("  test_ccq_add DONE\n");
}

/* ====================================================================== */
/* Test: NULL safety and error codes                                       */
/* ====================================================================== */

static void test_null_safety(void) {
    printf("  test_null_safety...\n");

    uint32_t a_q[2] = {0, 1};
    uint32_t b_q[2] = {2, 3};

    ASSERT_EQ(qc_toffoli_qq_add(NULL, a_q, b_q, 2), QC_ERR_NULL,
              "qq_add NULL ctx");
    ASSERT_EQ(qc_toffoli_cq_add(NULL, a_q, 2, 1), QC_ERR_NULL,
              "cq_add NULL ctx");
    ASSERT_EQ(qc_toffoli_cqq_add(NULL, a_q, b_q, 2, 4), QC_ERR_NULL,
              "cqq_add NULL ctx");
    ASSERT_EQ(qc_toffoli_ccq_add(NULL, a_q, 2, 1, 4), QC_ERR_NULL,
              "ccq_add NULL ctx");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT_EQ(qc_toffoli_qq_add(ctx, NULL, b_q, 2), QC_ERR_NULL,
              "qq_add NULL a");
    ASSERT_EQ(qc_toffoli_qq_add(ctx, a_q, NULL, 2), QC_ERR_NULL,
              "qq_add NULL b");
    ASSERT_EQ(qc_toffoli_cq_add(ctx, NULL, 2, 1), QC_ERR_NULL,
              "cq_add NULL target");

    /* Width 0 -> ERR_WIDTH */
    ASSERT_EQ(qc_toffoli_qq_add(ctx, a_q, b_q, 0), QC_ERR_WIDTH,
              "qq_add width=0");
    ASSERT_EQ(qc_toffoli_qq_add(ctx, a_q, b_q, 65), QC_ERR_WIDTH,
              "qq_add width=65");

    qc_circuit_destroy(ctx);
    printf("  test_null_safety DONE\n");
}

/* ====================================================================== */
/* Test: larger widths in count-only mode                                  */
/* ====================================================================== */

static void test_larger_widths(void) {
    printf("  test_larger_widths...\n");

    /* Test widths 5-8 in count-only mode (no qubit limit issues) */
    for (int w = 5; w <= 8; w++) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        qc_circuit_set_simulate(ctx, false);

        uint32_t a_q[8], b_q[8];
        for (int i = 0; i < w; i++) {
            a_q[i] = (uint32_t)i;
            b_q[i] = (uint32_t)(w + i);
        }

        qc_error_t err = qc_toffoli_qq_add(ctx, a_q, b_q, (uint32_t)w);
        ASSERT_OK(err, "qq_add count-only larger width");

        qc_circuit_destroy(ctx);
    }

    /* CQ add for widths 5-8 */
    for (int w = 5; w <= 8; w++) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        qc_circuit_set_simulate(ctx, false);

        uint32_t tgt[8];
        for (int i = 0; i < w; i++)
            tgt[i] = (uint32_t)i;

        qc_error_t err = qc_toffoli_cq_add(ctx, tgt, (uint32_t)w, 7);
        ASSERT_OK(err, "cq_add count-only larger width");

        qc_circuit_destroy(ctx);
    }

    printf("  test_larger_widths DONE\n");
}

/* ====================================================================== */
/* Test: QQ add gate count formula                                         */
/* ====================================================================== */

static void test_qq_gate_formula(void) {
    printf("  test_qq_gate_formula...\n");

    /* For width n >= 2, QQ add should produce exactly 6*n gates */
    for (uint32_t w = 2; w <= 8; w++) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        qc_circuit_set_simulate(ctx, true);

        uint32_t a_q[8], b_q[8];
        for (uint32_t i = 0; i < w; i++) {
            a_q[i] = i;
            b_q[i] = w + i;
        }

        qc_error_t err = qc_toffoli_qq_add(ctx, a_q, b_q, w);
        ASSERT_OK(err, "qq_add gate formula OK");

        uint64_t expected = 6 * (uint64_t)w;
        ASSERT_EQ(qc_circuit_gate_count(ctx), expected,
                  "qq_add gate count = 6*width");

        qc_circuit_destroy(ctx);
    }

    printf("  test_qq_gate_formula DONE\n");
}

/* ====================================================================== */
/* main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== test_toffoli_cdkm ===\n");

    test_two_complement();
    test_seq_alloc_free();
    test_qq_add();
    test_cq_add();
    test_cqq_add();
    test_ccq_add();
    test_null_safety();
    test_larger_widths();
    test_qq_gate_formula();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
