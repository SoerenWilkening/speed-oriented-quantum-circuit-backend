/**
 * @file test_qq_comparison.c
 * @brief Tests for QQ less-than comparison sequence builders.
 *
 * Issue: refactor-eap
 *
 * Tests qc_cmp_qq_less_seq (uncontrolled) and qc_c_cmp_qq_less_seq (controlled)
 * for structural correctness: non-NULL returns, total_qubits, gate counts,
 * and edge cases.
 */

#include "quantum_circuit.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        g_fail++; printf("FAIL: %s -- exp %lld got %lld (line %d)\n", \
            msg, (long long)(b), (long long)(a), __LINE__); return; } \
} while (0)

/* ====================================================================== */
/* Tests: qc_cmp_qq_less_seq (uncontrolled)                                */
/* ====================================================================== */

static void test_qq_less_seq_non_null(void) {
    TEST("qq_less_seq: non-NULL for bits 1,2,4,8");

    int widths[] = {1, 2, 4, 8};
    for (int i = 0; i < 4; i++) {
        qc_sequence_t *seq = qc_cmp_qq_less_seq(widths[i]);
        ASSERT(seq != NULL, "returned NULL");
        ASSERT(qc_sequence_gate_count(seq) > 0, "zero gates");
        qc_sequence_free(seq);
    }
    PASS();
}

static void test_qq_less_seq_total_qubits(void) {
    TEST("qq_less_seq: total_qubits == 2*bits+3");

    for (int bits = 1; bits <= 8; bits++) {
        qc_sequence_t *seq = qc_cmp_qq_less_seq(bits);
        ASSERT(seq != NULL, "returned NULL");
        uint32_t expected = (uint32_t)(2 * bits + 3);
        ASSERT_EQ(qc_sequence_total_qubits(seq), expected,
                  "total_qubits mismatch");
        qc_sequence_free(seq);
    }
    PASS();
}

static void test_qq_less_seq_edge_bits0(void) {
    TEST("qq_less_seq: bits=0 returns NULL");

    qc_sequence_t *seq = qc_cmp_qq_less_seq(0);
    ASSERT(seq == NULL, "expected NULL for bits=0");
    PASS();
}

static void test_qq_less_seq_edge_bits64(void) {
    TEST("qq_less_seq: bits=64 returns NULL");

    qc_sequence_t *seq = qc_cmp_qq_less_seq(64);
    ASSERT(seq == NULL, "expected NULL for bits=64");
    PASS();
}

static void test_qq_less_seq_edge_bits_negative(void) {
    TEST("qq_less_seq: bits=-1 returns NULL");

    qc_sequence_t *seq = qc_cmp_qq_less_seq(-1);
    ASSERT(seq == NULL, "expected NULL for bits=-1");
    PASS();
}

static void test_qq_less_seq_bits1(void) {
    TEST("qq_less_seq: bits=1 works");

    qc_sequence_t *seq = qc_cmp_qq_less_seq(1);
    ASSERT(seq != NULL, "returned NULL for bits=1");
    ASSERT_EQ(qc_sequence_total_qubits(seq), 5, "total_qubits should be 5");
    ASSERT(qc_sequence_gate_count(seq) > 0, "zero gates for bits=1");
    qc_sequence_free(seq);
    PASS();
}

static void test_qq_less_seq_gate_count_increases(void) {
    TEST("qq_less_seq: gate count increases with bits");

    qc_sequence_t *s1 = qc_cmp_qq_less_seq(1);
    qc_sequence_t *s2 = qc_cmp_qq_less_seq(2);
    qc_sequence_t *s4 = qc_cmp_qq_less_seq(4);
    ASSERT(s1 != NULL && s2 != NULL && s4 != NULL, "returned NULL");

    uint32_t gc1 = qc_sequence_gate_count(s1);
    uint32_t gc2 = qc_sequence_gate_count(s2);
    uint32_t gc4 = qc_sequence_gate_count(s4);

    printf("(gc1=%u gc2=%u gc4=%u) ", gc1, gc2, gc4);

    ASSERT(gc2 > gc1, "bits=2 should have more gates than bits=1");
    ASSERT(gc4 > gc2, "bits=4 should have more gates than bits=2");

    qc_sequence_free(s1);
    qc_sequence_free(s2);
    qc_sequence_free(s4);
    PASS();
}

/* ====================================================================== */
/* Tests: qc_c_cmp_qq_less_seq (controlled)                                */
/* ====================================================================== */

static void test_c_qq_less_seq_non_null(void) {
    TEST("c_qq_less_seq: non-NULL for bits 1,2,4,8");

    int widths[] = {1, 2, 4, 8};
    for (int i = 0; i < 4; i++) {
        qc_sequence_t *seq = qc_c_cmp_qq_less_seq(widths[i]);
        ASSERT(seq != NULL, "returned NULL");
        ASSERT(qc_sequence_gate_count(seq) > 0, "zero gates");
        qc_sequence_free(seq);
    }
    PASS();
}

static void test_c_qq_less_seq_total_qubits(void) {
    TEST("c_qq_less_seq: total_qubits == 2*bits+4");

    for (int bits = 1; bits <= 8; bits++) {
        qc_sequence_t *seq = qc_c_cmp_qq_less_seq(bits);
        ASSERT(seq != NULL, "returned NULL");
        uint32_t expected = (uint32_t)(2 * bits + 4);
        ASSERT_EQ(qc_sequence_total_qubits(seq), expected,
                  "total_qubits mismatch");
        qc_sequence_free(seq);
    }
    PASS();
}

static void test_c_qq_less_seq_edge_bits0(void) {
    TEST("c_qq_less_seq: bits=0 returns NULL");

    qc_sequence_t *seq = qc_c_cmp_qq_less_seq(0);
    ASSERT(seq == NULL, "expected NULL for bits=0");
    PASS();
}

static void test_c_qq_less_seq_edge_bits64(void) {
    TEST("c_qq_less_seq: bits=64 returns NULL");

    qc_sequence_t *seq = qc_c_cmp_qq_less_seq(64);
    ASSERT(seq == NULL, "expected NULL for bits=64");
    PASS();
}

static void test_c_qq_less_seq_more_gates_than_uncontrolled(void) {
    TEST("c_qq_less_seq: more gates than uncontrolled");

    for (int bits = 1; bits <= 4; bits++) {
        qc_sequence_t *unc = qc_cmp_qq_less_seq(bits);
        qc_sequence_t *ctl = qc_c_cmp_qq_less_seq(bits);
        ASSERT(unc != NULL && ctl != NULL, "returned NULL");

        uint32_t gc_unc = qc_sequence_gate_count(unc);
        uint32_t gc_ctl = qc_sequence_gate_count(ctl);

        ASSERT(gc_ctl > gc_unc,
               "controlled should have more gates than uncontrolled");

        qc_sequence_free(unc);
        qc_sequence_free(ctl);
    }
    PASS();
}

static void test_c_qq_less_seq_bits1(void) {
    TEST("c_qq_less_seq: bits=1 works");

    qc_sequence_t *seq = qc_c_cmp_qq_less_seq(1);
    ASSERT(seq != NULL, "returned NULL for bits=1");
    ASSERT_EQ(qc_sequence_total_qubits(seq), 6, "total_qubits should be 6");
    ASSERT(qc_sequence_gate_count(seq) > 0, "zero gates for bits=1");
    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* Test: replay on circuit produces gates                                  */
/* ====================================================================== */

static void test_qq_less_seq_replay(void) {
    TEST("qq_less_seq: replay on circuit produces gates");

    int bits = 2;
    qc_sequence_t *seq = qc_cmp_qq_less_seq(bits);
    ASSERT(seq != NULL, "returned NULL");

    uint32_t total_q = qc_sequence_total_qubits(seq);
    circuit_ctx_t *ctx = qc_circuit_create(total_q + 16);
    ASSERT(ctx != NULL, "failed to create circuit");
    qc_circuit_set_simulate(ctx, true);

    uint32_t start = 0;
    qc_qubit_alloc_n(ctx, total_q, &start);

    /* Identity qubit map: abstract qubit i -> physical qubit i */
    uint32_t map[16];
    for (uint32_t i = 0; i < total_q; i++)
        map[i] = i;

    qc_run_instruction(ctx, seq, map, 0);

    uint64_t gc = qc_circuit_gate_count(ctx);
    printf("(gates=%llu) ", (unsigned long long)gc);
    ASSERT(gc > 0, "no gates after replay");

    qc_sequence_free(seq);
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_c_qq_less_seq_replay(void) {
    TEST("c_qq_less_seq: replay on circuit produces gates");

    int bits = 2;
    qc_sequence_t *seq = qc_c_cmp_qq_less_seq(bits);
    ASSERT(seq != NULL, "returned NULL");

    uint32_t total_q = qc_sequence_total_qubits(seq);
    circuit_ctx_t *ctx = qc_circuit_create(total_q + 16);
    ASSERT(ctx != NULL, "failed to create circuit");
    qc_circuit_set_simulate(ctx, true);

    uint32_t start = 0;
    qc_qubit_alloc_n(ctx, total_q, &start);

    uint32_t map[16];
    for (uint32_t i = 0; i < total_q; i++)
        map[i] = i;

    qc_run_instruction(ctx, seq, map, 0);

    uint64_t gc = qc_circuit_gate_count(ctx);
    printf("(gates=%llu) ", (unsigned long long)gc);
    ASSERT(gc > 0, "no gates after replay");

    qc_sequence_free(seq);
    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== QQ comparison sequence tests ===\n");

    /* Uncontrolled */
    test_qq_less_seq_non_null();
    test_qq_less_seq_total_qubits();
    test_qq_less_seq_edge_bits0();
    test_qq_less_seq_edge_bits64();
    test_qq_less_seq_edge_bits_negative();
    test_qq_less_seq_bits1();
    test_qq_less_seq_gate_count_increases();
    test_qq_less_seq_replay();

    /* Controlled */
    test_c_qq_less_seq_non_null();
    test_c_qq_less_seq_total_qubits();
    test_c_qq_less_seq_edge_bits0();
    test_c_qq_less_seq_edge_bits64();
    test_c_qq_less_seq_bits1();
    test_c_qq_less_seq_more_gates_than_uncontrolled();
    test_c_qq_less_seq_replay();

    printf("\n  Results: %d run, %d passed, %d failed\n", g_run, g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
