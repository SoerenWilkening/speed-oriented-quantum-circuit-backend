/**
 * @file test_sequence_qubit_count.c
 * @brief Tests for qc_sequence_total_qubits() metadata on all sequence builders.
 *
 * Covers:
 *   - NULL safety
 *   - Non-capture QFT addition and multiplication
 *   - Non-capture comparison sequences
 *   - Non-capture bitwise sequences
 *   - Non-capture hot-path add/sub
 *   - Capture-based divmod sequences
 *   - Capture-based controlled multiplication sequences
 *   - Replay with identity mapping using total_qubits
 */

#include <quantum_circuit.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                          \
    do {                                                                    \
        tests_run++;                                                        \
        printf("  %-55s ", #name);                                          \
        fflush(stdout);                                                     \
    } while (0)

#define PASS()                                                              \
    do {                                                                    \
        tests_passed++;                                                     \
        printf("[PASS]\n");                                                 \
    } while (0)

/* Helper: build seq, assert total_qubits == expected, free. */
#define CHECK_TOTAL(seq_ptr, expected)                                      \
    do {                                                                    \
        assert((seq_ptr) != NULL);                                          \
        uint32_t _tq = qc_sequence_total_qubits(seq_ptr);                  \
        if (_tq != (uint32_t)(expected)) {                                  \
            printf("[FAIL] expected %u, got %u\n",                          \
                   (uint32_t)(expected), _tq);                              \
            assert(_tq == (uint32_t)(expected));                            \
        }                                                                   \
        qc_sequence_free(seq_ptr);                                          \
    } while (0)

/* Helper: build seq, assert total_qubits > threshold, free. */
#define CHECK_TOTAL_GT(seq_ptr, threshold)                                  \
    do {                                                                    \
        assert((seq_ptr) != NULL);                                          \
        uint32_t _tq = qc_sequence_total_qubits(seq_ptr);                  \
        if (_tq <= (uint32_t)(threshold)) {                                 \
            printf("[FAIL] expected > %u, got %u\n",                        \
                   (uint32_t)(threshold), _tq);                             \
            assert(_tq > (uint32_t)(threshold));                            \
        }                                                                   \
        qc_sequence_free(seq_ptr);                                          \
    } while (0)

/* ====================================================================== */
/* 1. NULL safety                                                          */
/* ====================================================================== */

static void test_null_safety(void) {
    TEST(total_qubits_null_returns_zero);
    assert(qc_sequence_total_qubits(NULL) == 0);
    PASS();
}

/* ====================================================================== */
/* 2. Non-capture QFT addition                                             */
/* ====================================================================== */

static void test_qft_addition(void) {
    TEST(qq_add_seq_4_total_8);
    CHECK_TOTAL(qc_arith_qq_add_seq(4), 8);
    PASS();

    TEST(cq_add_seq_4_5_total_4);
    CHECK_TOTAL(qc_arith_cq_add_seq(4, 5), 4);
    PASS();

    TEST(cqq_add_seq_4_total_9);
    CHECK_TOTAL(qc_arith_cqq_add_seq(4), 9);
    PASS();

    TEST(ccq_add_seq_4_5_total_5);
    CHECK_TOTAL(qc_arith_ccq_add_seq(4, 5), 5);
    PASS();
}

/* ====================================================================== */
/* 3. Non-capture QFT multiplication                                       */
/* ====================================================================== */

static void test_qft_multiplication(void) {
    TEST(cq_mul_seq_4_3_total_8);
    CHECK_TOTAL(qc_arith_cq_mul_seq(4, 3), 8);
    PASS();

    TEST(qq_mul_seq_4_total_12);
    CHECK_TOTAL(qc_arith_qq_mul_seq(4), 12);
    PASS();
}

/* ====================================================================== */
/* 4. Non-capture comparison                                               */
/* ====================================================================== */

static void test_comparison(void) {
    TEST(cmp_cq_equal_4_5_total_7);
    CHECK_TOTAL(qc_cmp_cq_equal_seq(4, 5), 7);
    PASS();

    TEST(cmp_cq_equal_2_1_total_3);
    CHECK_TOTAL(qc_cmp_cq_equal_seq(2, 1), 3);
    PASS();

    TEST(cmp_cq_less_4_5_total_6);
    CHECK_TOTAL(qc_cmp_cq_less_seq(4, 5), 6);
    PASS();

    TEST(cmp_cq_greater_4_3_total_6);
    CHECK_TOTAL(qc_cmp_cq_greater_seq(4, 3), 6);
    PASS();

    TEST(c_cmp_cq_equal_4_5_total_9);
    CHECK_TOTAL(qc_c_cmp_cq_equal_seq(4, 5), 9);
    PASS();

    TEST(c_cmp_cq_less_4_5_total_7);
    CHECK_TOTAL(qc_c_cmp_cq_less_seq(4, 5), 7);
    PASS();

    TEST(c_cmp_cq_greater_4_3_total_7);
    CHECK_TOTAL(qc_c_cmp_cq_greater_seq(4, 3), 7);
    PASS();
}

/* ====================================================================== */
/* 5. Non-capture bitwise                                                  */
/* ====================================================================== */

static void test_bitwise(void) {
    TEST(not_seq_4_total_4);
    CHECK_TOTAL(qc_not_seq(4), 4);
    PASS();

    TEST(xor_seq_4_total_8);
    CHECK_TOTAL(qc_xor_seq(4), 8);
    PASS();

    TEST(and_seq_4_total_12);
    CHECK_TOTAL(qc_and_seq(4), 12);
    PASS();

    TEST(or_seq_4_total_12);
    CHECK_TOTAL(qc_or_seq(4), 12);
    PASS();

    TEST(c_not_seq_4_total_5);
    CHECK_TOTAL(qc_c_not_seq(4), 5);
    PASS();

    TEST(c_xor_seq_4_total_9);
    CHECK_TOTAL(qc_c_xor_seq(4), 9);
    PASS();

    TEST(c_and_seq_4_total_13);
    CHECK_TOTAL(qc_c_and_seq(4), 13);
    PASS();

    TEST(c_or_seq_4_total_13);
    CHECK_TOTAL(qc_c_or_seq(4), 13);
    PASS();
}

/* ====================================================================== */
/* 6. Non-capture hot-path add                                             */
/* ====================================================================== */

static void test_hot_path_add(void) {
    TEST(split_cq_add_seq_4_3_total_5);
    CHECK_TOTAL(qc_split_cq_add_seq(4, 3), 5);
    PASS();

    TEST(split_cq_sub_seq_4_3_total_5);
    CHECK_TOTAL(qc_split_cq_sub_seq(4, 3), 5);
    PASS();
}

/* ====================================================================== */
/* 7. Capture divmod                                                       */
/* ====================================================================== */

static void test_capture_divmod(void) {
    TEST(divmod_cq_seq_5_3_total_gt_15);
    CHECK_TOTAL_GT(qc_divmod_cq_seq(5, 3), 15);
    PASS();

    TEST(divmod_qq_seq_5_total_gt_20);
    CHECK_TOTAL_GT(qc_divmod_qq_seq(5), 20);
    PASS();

    TEST(c_divmod_cq_seq_5_3_total_gt_16);
    CHECK_TOTAL_GT(qc_c_divmod_cq_seq(5, 3), 16);
    PASS();

    TEST(c_divmod_qq_seq_5_total_gt_21);
    CHECK_TOTAL_GT(qc_c_divmod_qq_seq(5), 21);
    PASS();
}

/* ====================================================================== */
/* 8. Capture controlled multiplication                                    */
/* ====================================================================== */

static void test_capture_cmul(void) {
    TEST(c_arith_cq_mul_seq_5_3_total_gt_11);
    CHECK_TOTAL_GT(qc_c_arith_cq_mul_seq(5, 3), 11);
    PASS();

    TEST(c_arith_qq_mul_seq_5_total_gt_16);
    CHECK_TOTAL_GT(qc_c_arith_qq_mul_seq(5), 16);
    PASS();
}

/* ====================================================================== */
/* 9. Replay with correct identity mapping                                 */
/* ====================================================================== */

static void test_replay_identity_mapping(void) {
    TEST(replay_divmod_cq_identity_mapping);

    /* Build a divmod sequence */
    qc_sequence_t *seq = qc_divmod_cq_seq(3, 2);
    assert(seq != NULL);

    uint32_t total = qc_sequence_total_qubits(seq);
    assert(total > 0);

    /* Create a real circuit and allocate exactly total_qubits qubits */
    circuit_ctx_t *ctx = qc_circuit_create(0);
    assert(ctx != NULL);
    qc_circuit_set_simulate(ctx, true);

    uint32_t start;
    qc_error_t err = qc_qubit_alloc_n(ctx, total, &start);
    assert(err == QC_OK);

    /* Build identity mapping: virtual qubit i -> physical qubit start+i */
    uint32_t *qmap = (uint32_t *)malloc(total * sizeof(uint32_t));
    assert(qmap != NULL);
    for (uint32_t i = 0; i < total; i++) {
        qmap[i] = start + i;
    }

    /* Replay the sequence */
    qc_run_instruction(ctx, seq, qmap, 0);

    /* Verify circuit width stays within bounds */
    uint32_t width = qc_circuit_width(ctx);
    assert(width <= total);

    free(qmap);
    qc_sequence_free(seq);
    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Sequence Qubit Count Tests ===\n\n");

    test_null_safety();
    test_qft_addition();
    test_qft_multiplication();
    test_comparison();
    test_bitwise();
    test_hot_path_add();
    test_capture_divmod();
    test_capture_cmul();
    test_replay_identity_mapping();

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
