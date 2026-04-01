/**
 * @file test_comparison_fix.c
 * @brief Regression tests for CQ comparison wrapper ancilla allocation fixes.
 *
 * Tests qc_cmp_cq_greater, qc_cmp_cq_less, and qc_cmp_cq_equal at
 * various widths and boundary values, including cases that previously
 * hung or caused memory corruption due to missing ancilla allocation.
 */

#include <quantum_circuit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Need internal header for sequence gate count verification */
#include "internal.h"

/* External sequence builders */
extern qc_sequence_t *qc_cmp_cq_greater_seq(int bits, int64_t value);
extern qc_sequence_t *qc_cmp_cq_less_seq(int bits, int64_t value);
extern qc_sequence_t *qc_cmp_cq_equal_seq(int bits, int64_t value);

/* ====================================================================== */
/* Test infrastructure                                                     */
/* ====================================================================== */

static int g_run = 0, g_pass = 0, g_fail = 0;

#define TEST(name) do { g_run++; printf("  %-60s ", name); } while (0)
#define PASS()     do { g_pass++; printf("PASS\n"); } while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { g_fail++; printf("FAIL: %s (line %d)\n", msg, __LINE__); return; } \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((long long)(a) != (long long)(b)) { \
        g_fail++; printf("FAIL: %s — exp %lld got %lld (line %d)\n", \
            msg, (long long)(b), (long long)(a), __LINE__); return; } \
} while (0)

/* ====================================================================== */
/* Test: qc_cmp_cq_greater at widths 1-8 with boundary values             */
/* ====================================================================== */

static void test_cq_greater_all_widths(void) {
    TEST("cq_greater: widths 1-8, boundary values");

    for (int w = 1; w <= 8; w++) {
        int64_t max_val = (1LL << w) - 1;
        int64_t values[] = {0, 1, max_val - 1, max_val};
        int nvals = 4;

        for (int vi = 0; vi < nvals; vi++) {
            int64_t v = values[vi];
            circuit_ctx_t *ctx = qc_circuit_create(128);
            ASSERT(ctx != NULL, "create");

            /* Allocate operand qubits + result */
            uint32_t a_start;
            qc_qubit_alloc_n(ctx, (uint32_t)w, &a_start);
            uint32_t a[64];
            for (int i = 0; i < w; i++) a[i] = a_start + (uint32_t)i;
            uint32_t result;
            qc_qubit_alloc(ctx, &result);

            qc_error_t err = qc_cmp_cq_greater(ctx, a, (uint32_t)w, v, result);
            ASSERT_EQ(err, QC_OK, "returns QC_OK");

            /* Verify gate count matches sequence */
            qc_sequence_t *seq = qc_cmp_cq_greater_seq(w, v);
            uint32_t expected_gates = qc_sequence_gate_count(seq);
            uint64_t actual_gates = qc_circuit_gate_count(ctx);
            qc_sequence_free(seq);

            ASSERT_EQ(actual_gates, expected_gates, "gate count matches seq");

            /* Trivially-false case: value == max_val => 0 gates */
            if (v == max_val) {
                ASSERT_EQ(actual_gates, 0, "trivially false => 0 gates");
            }

            qc_circuit_destroy(ctx);
        }
    }
    PASS();
}

/* ====================================================================== */
/* Test: qc_cmp_cq_less at widths 1-8 with boundary values                */
/* ====================================================================== */

static void test_cq_less_all_widths(void) {
    TEST("cq_less: widths 1-8, boundary values");

    for (int w = 1; w <= 8; w++) {
        int64_t max_val = (1LL << w) - 1;
        int64_t values[] = {0, 1, max_val / 2, max_val};
        int nvals = 4;

        for (int vi = 0; vi < nvals; vi++) {
            int64_t v = values[vi];
            circuit_ctx_t *ctx = qc_circuit_create(128);
            ASSERT(ctx != NULL, "create");

            uint32_t a_start;
            qc_qubit_alloc_n(ctx, (uint32_t)w, &a_start);
            uint32_t a[64];
            for (int i = 0; i < w; i++) a[i] = a_start + (uint32_t)i;
            uint32_t result;
            qc_qubit_alloc(ctx, &result);

            qc_error_t err = qc_cmp_cq_less(ctx, a, (uint32_t)w, v, result);
            ASSERT_EQ(err, QC_OK, "returns QC_OK");

            /* Verify gate count matches sequence */
            qc_sequence_t *seq = qc_cmp_cq_less_seq(w, v);
            uint32_t expected_gates = qc_sequence_gate_count(seq);
            uint64_t actual_gates = qc_circuit_gate_count(ctx);
            qc_sequence_free(seq);

            ASSERT_EQ(actual_gates, expected_gates, "gate count matches seq");

            qc_circuit_destroy(ctx);
        }
    }
    PASS();
}

/* ====================================================================== */
/* Test: qc_cmp_cq_equal at widths 1-8 with boundary values               */
/* ====================================================================== */

static void test_cq_equal_all_widths(void) {
    TEST("cq_equal: widths 1-8, boundary values");

    for (int w = 1; w <= 8; w++) {
        int64_t max_val = (1LL << w) - 1;
        int64_t values[] = {0, 1, max_val};
        int nvals = 3;

        for (int vi = 0; vi < nvals; vi++) {
            int64_t v = values[vi];
            circuit_ctx_t *ctx = qc_circuit_create(128);
            ASSERT(ctx != NULL, "create");

            uint32_t a_start;
            qc_qubit_alloc_n(ctx, (uint32_t)w, &a_start);
            uint32_t a[64];
            for (int i = 0; i < w; i++) a[i] = a_start + (uint32_t)i;
            uint32_t result;
            qc_qubit_alloc(ctx, &result);

            qc_error_t err = qc_cmp_cq_equal(ctx, a, (uint32_t)w, v, result);
            ASSERT_EQ(err, QC_OK, "returns QC_OK");

            /* Verify gate count matches sequence */
            qc_sequence_t *seq = qc_cmp_cq_equal_seq(w, v);
            uint32_t expected_gates = qc_sequence_gate_count(seq);
            uint64_t actual_gates = qc_circuit_gate_count(ctx);
            qc_sequence_free(seq);

            ASSERT_EQ(actual_gates, expected_gates, "gate count matches seq");

            qc_circuit_destroy(ctx);
        }
    }
    PASS();
}

/* ====================================================================== */
/* Test: Previously hanging combinations                                   */
/* ====================================================================== */

static void test_cq_less_previously_hanging(void) {
    TEST("cq_less: previously hanging combos (w=1,v=2)(w=2,v=0,1,3)");

    struct { int w; int64_t v; } cases[] = {
        {1, 2}, {2, 0}, {2, 1}, {2, 3}
    };
    int ncases = 4;

    for (int ci = 0; ci < ncases; ci++) {
        int w = cases[ci].w;
        int64_t v = cases[ci].v;
        circuit_ctx_t *ctx = qc_circuit_create(128);
        ASSERT(ctx != NULL, "create");

        uint32_t a_start;
        qc_qubit_alloc_n(ctx, (uint32_t)w, &a_start);
        uint32_t a[64];
        for (int i = 0; i < w; i++) a[i] = a_start + (uint32_t)i;
        uint32_t result;
        qc_qubit_alloc(ctx, &result);

        qc_error_t err = qc_cmp_cq_less(ctx, a, (uint32_t)w, v, result);
        ASSERT_EQ(err, QC_OK, "returns QC_OK");

        qc_circuit_destroy(ctx);
    }
    PASS();
}

/* ====================================================================== */
/* Test: cq_greater in simulate mode                                       */
/* ====================================================================== */

static void test_cq_greater_simulate_mode(void) {
    TEST("cq_greater: simulate=true, width=2, gate count matches");

    circuit_ctx_t *ctx = qc_circuit_create(128);
    ASSERT(ctx != NULL, "create");
    qc_circuit_set_simulate(ctx, true);

    uint32_t a_start;
    qc_qubit_alloc_n(ctx, 2, &a_start);
    uint32_t a[2] = {a_start, a_start + 1};
    uint32_t result;
    qc_qubit_alloc(ctx, &result);

    qc_error_t err = qc_cmp_cq_greater(ctx, a, 2, 0, result);
    ASSERT_EQ(err, QC_OK, "returns QC_OK");

    qc_sequence_t *seq = qc_cmp_cq_greater_seq(2, 0);
    uint32_t expected = qc_sequence_gate_count(seq);
    uint64_t actual = qc_circuit_gate_count(ctx);
    qc_sequence_free(seq);

    ASSERT_EQ(actual, expected, "gate count matches seq");
    ASSERT(actual > 0, "non-zero gates");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Test: qq_less wrapper works correctly (Step 4 verification)             */
/* ====================================================================== */

static void test_qq_less_widths(void) {
    TEST("qq_less: widths 1-4 return QC_OK with non-zero gates");

    for (uint32_t w = 1; w <= 4; w++) {
        circuit_ctx_t *ctx = qc_circuit_create(128);
        ASSERT(ctx != NULL, "create");

        uint32_t a_start, b_start;
        qc_qubit_alloc_n(ctx, w, &a_start);
        qc_qubit_alloc_n(ctx, w, &b_start);
        uint32_t a[64], b[64];
        for (uint32_t i = 0; i < w; i++) {
            a[i] = a_start + i;
            b[i] = b_start + i;
        }
        uint32_t result;
        qc_qubit_alloc(ctx, &result);

        qc_error_t err = qc_cmp_qq_less(ctx, a, b, w, result);
        ASSERT_EQ(err, QC_OK, "returns QC_OK");
        ASSERT(qc_circuit_gate_count(ctx) > 0, "non-zero gates");

        qc_circuit_destroy(ctx);
    }
    PASS();
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    printf("=== Comparison Fix Regression Tests ===\n\n");

    printf("[1] CQ greater-than (primary downstream path)\n");
    test_cq_greater_all_widths();

    printf("\n[2] CQ less-than\n");
    test_cq_less_all_widths();

    printf("\n[3] CQ equal (AND-ancilla for width >= 3)\n");
    test_cq_equal_all_widths();

    printf("\n[4] Previously hanging combinations\n");
    test_cq_less_previously_hanging();

    printf("\n[5] Simulate mode\n");
    test_cq_greater_simulate_mode();

    printf("\n[6] QQ less-than (Step 4 verification)\n");
    test_qq_less_widths();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_pass, g_run, g_fail);
    return g_fail > 0 ? 1 : 0;
}
