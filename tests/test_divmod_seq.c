/**
 * @file test_divmod_seq.c
 * @brief Tests for divmod sequence builders.
 *
 * Issue: refactor-a1b
 */

#include "quantum_circuit.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { printf("  TEST: %s ... ", #name); } while (0)

#define PASS() \
    do { printf("PASS\n"); tests_passed++; } while (0)

#define FAIL(msg) \
    do { printf("FAIL: %s\n", msg); tests_failed++; } while (0)

#define ASSERT(cond, msg) \
    do { if (!(cond)) { FAIL(msg); return; } } while (0)

/* ====================================================================== */
/* Tests                                                                   */
/* ====================================================================== */

static void test_divmod_cq_seq_basic(void) {
    TEST(divmod_cq_seq_basic);

    qc_sequence_t *seq = qc_divmod_cq_seq(4, 3);
    ASSERT(seq != NULL, "qc_divmod_cq_seq(4, 3) returned NULL");

    uint32_t gc = qc_sequence_gate_count(seq);
    ASSERT(gc > 0, "sequence gate count is 0");

    printf("(gates=%u) ", gc);

    qc_sequence_free(seq);
    PASS();
}

static void test_divmod_cq_seq_div_by_zero(void) {
    TEST(divmod_cq_seq_div_by_zero);

    qc_sequence_t *seq = qc_divmod_cq_seq(4, 0);
    ASSERT(seq == NULL, "expected NULL for division by zero");

    PASS();
}

static void test_divmod_cq_seq_various_widths(void) {
    TEST(divmod_cq_seq_various_widths);

    for (int bits = 1; bits <= 8; bits++) {
        qc_sequence_t *seq = qc_divmod_cq_seq(bits, 2);
        ASSERT(seq != NULL, "returned NULL for valid width");

        uint32_t gc = qc_sequence_gate_count(seq);
        ASSERT(gc > 0, "gate count is 0");

        qc_sequence_free(seq);
    }

    PASS();
}

static void test_divmod_qq_seq_basic(void) {
    TEST(divmod_qq_seq_basic);

    /* QQ divmod is expensive (2^n iterations), only test small bits */
    qc_sequence_t *seq = qc_divmod_qq_seq(2);
    ASSERT(seq != NULL, "qc_divmod_qq_seq(2) returned NULL");

    uint32_t gc = qc_sequence_gate_count(seq);
    ASSERT(gc > 0, "sequence gate count is 0");

    printf("(gates=%u) ", gc);

    qc_sequence_free(seq);
    PASS();
}

static void test_c_divmod_cq_seq_basic(void) {
    TEST(c_divmod_cq_seq_basic);

    qc_sequence_t *seq = qc_c_divmod_cq_seq(4, 3);
    ASSERT(seq != NULL, "qc_c_divmod_cq_seq(4, 3) returned NULL");

    uint32_t gc = qc_sequence_gate_count(seq);
    ASSERT(gc > 0, "sequence gate count is 0");

    printf("(gates=%u) ", gc);

    qc_sequence_free(seq);
    PASS();
}

static void test_c_divmod_qq_seq_basic(void) {
    TEST(c_divmod_qq_seq_basic);

    qc_sequence_t *seq = qc_c_divmod_qq_seq(2);
    ASSERT(seq != NULL, "qc_c_divmod_qq_seq(2) returned NULL");

    uint32_t gc = qc_sequence_gate_count(seq);
    ASSERT(gc > 0, "sequence gate count is 0");

    printf("(gates=%u) ", gc);

    qc_sequence_free(seq);
    PASS();
}

static void test_divmod_cq_replay(void) {
    TEST(divmod_cq_replay);

    /* Build sequence and replay into a circuit */
    int bits = 4;
    int64_t divisor = 3;

    qc_sequence_t *seq = qc_divmod_cq_seq(bits, divisor);
    ASSERT(seq != NULL, "sequence is NULL");

    /* Create target circuit */
    circuit_ctx_t *ctx = qc_circuit_create(512);
    ASSERT(ctx != NULL, "failed to create circuit");
    qc_circuit_set_simulate(ctx, true);

    /* Allocate qubits for the replay */
    uint32_t start;
    qc_error_t err = qc_qubit_alloc_n(ctx, 512, &start);
    ASSERT(err == QC_OK, "qubit alloc failed");

    /* Build qubit mapping: identity for first 3*bits qubits */
    uint32_t qmap[512];
    for (int i = 0; i < 512; i++)
        qmap[i] = (uint32_t)i;

    qc_run_instruction(ctx, seq, qmap, 0);

    uint64_t gc = qc_circuit_gate_count(ctx);
    ASSERT(gc > 0, "replay produced 0 gates");
    ASSERT(gc == qc_sequence_gate_count(seq),
           "replay gate count mismatch");

    printf("(gates=%lu) ", (unsigned long)gc);

    qc_sequence_free(seq);
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_divmod_cq_seq_invalid_width(void) {
    TEST(divmod_cq_seq_invalid_width);

    ASSERT(qc_divmod_cq_seq(0, 3) == NULL, "expected NULL for bits=0");
    ASSERT(qc_divmod_cq_seq(-1, 3) == NULL, "expected NULL for bits=-1");
    ASSERT(qc_divmod_cq_seq(65, 3) == NULL, "expected NULL for bits=65");

    PASS();
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Divmod Sequence Builder Tests ===\n");

    test_divmod_cq_seq_basic();
    test_divmod_cq_seq_div_by_zero();
    test_divmod_cq_seq_various_widths();
    test_divmod_qq_seq_basic();
    test_c_divmod_cq_seq_basic();
    test_c_divmod_qq_seq_basic();
    test_divmod_cq_replay();
    test_divmod_cq_seq_invalid_width();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
