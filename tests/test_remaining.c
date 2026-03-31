/**
 * @file test_remaining.c
 * @brief Tests for Module 1.12: integer, comparison, logic, and hot path ops.
 *
 * Tests:
 *   1. qc_two_complement basic values
 *   2. qc_two_complement edge cases
 *   3. Bitwise NOT via qc_bitwise_not
 *   4. Bitwise XOR via qc_bitwise_xor
 *   5. Bitwise AND via qc_bitwise_and
 *   6. Bitwise OR via qc_bitwise_or
 *   7. NOT/XOR/AND/OR NULL safety
 *   8. NOT/XOR width validation
 *   9. Hot path add sequence construction
 *   10. Hot path sub sequence construction
 *   11. CQ equality sequence construction
 *   12. CQ less-than sequence construction
 *   13. CQ greater-than sequence construction
 *   14. CQ equality via public API (gate count check)
 *   15. Logic operations with circuit context (gate count)
 *
 * Max 17 qubits in simulations.
 *
 * Issue: refactor-ovp
 */

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b, msg)                                              \
    do {                                                                  \
        if ((a) == (b)) {                                                 \
            tests_passed++;                                               \
        } else {                                                          \
            fprintf(stderr, "FAIL [%s:%d]: %s: expected %ld, got %ld\n",  \
                    __FILE__, __LINE__, (msg),                            \
                    (long)(b), (long)(a));                                \
            tests_failed++;                                               \
        }                                                                 \
    } while (0)

#define ASSERT_TRUE(cond, msg)                                            \
    do {                                                                  \
        if ((cond)) {                                                     \
            tests_passed++;                                               \
        } else {                                                          \
            fprintf(stderr, "FAIL [%s:%d]: %s\n",                        \
                    __FILE__, __LINE__, (msg));                           \
            tests_failed++;                                               \
        }                                                                 \
    } while (0)

/* External declarations for functions under test */
extern int *qc_two_complement(int64_t x, int n);

/* Sequence builders */
extern qc_sequence_t *qc_split_cq_add_seq(int bits, int64_t value);
extern qc_sequence_t *qc_split_cq_sub_seq(int bits, int64_t value);
extern qc_sequence_t *qc_cmp_cq_equal_seq(int bits, int64_t value);
extern qc_sequence_t *qc_cmp_cq_less_seq(int bits, int64_t value);
extern qc_sequence_t *qc_cmp_cq_greater_seq(int bits, int64_t value);
extern qc_sequence_t *qc_not_seq(int bits);
extern qc_sequence_t *qc_xor_seq(int bits);
extern qc_sequence_t *qc_and_seq(int bits);
extern qc_sequence_t *qc_or_seq(int bits);
extern void qc_sequence_free(qc_sequence_t *seq);

/* ====================================================================== */
/* Tests: qc_two_complement                                                */
/* ====================================================================== */

static void test_two_complement_basic(void) {
    printf("  test_two_complement_basic...\n");

    /* 5 in 4 bits: 0101 */
    int *bin = qc_two_complement(5, 4);
    ASSERT_TRUE(bin != NULL, "alloc");
    ASSERT_EQ(bin[0], 0, "bit 3");
    ASSERT_EQ(bin[1], 1, "bit 2");
    ASSERT_EQ(bin[2], 0, "bit 1");
    ASSERT_EQ(bin[3], 1, "bit 0");
    free(bin);

    /* -1 in 4 bits: 1111 */
    bin = qc_two_complement(-1, 4);
    ASSERT_TRUE(bin != NULL, "alloc -1");
    ASSERT_EQ(bin[0], 1, "-1 bit 3");
    ASSERT_EQ(bin[1], 1, "-1 bit 2");
    ASSERT_EQ(bin[2], 1, "-1 bit 1");
    ASSERT_EQ(bin[3], 1, "-1 bit 0");
    free(bin);

    /* 0 in 4 bits: 0000 */
    bin = qc_two_complement(0, 4);
    ASSERT_TRUE(bin != NULL, "alloc 0");
    ASSERT_EQ(bin[0], 0, "0 bit 3");
    ASSERT_EQ(bin[1], 0, "0 bit 2");
    ASSERT_EQ(bin[2], 0, "0 bit 1");
    ASSERT_EQ(bin[3], 0, "0 bit 0");
    free(bin);
}

static void test_two_complement_edge(void) {
    printf("  test_two_complement_edge...\n");

    /* 1-bit: 1 = 1, 0 = 0 */
    int *bin = qc_two_complement(1, 1);
    ASSERT_TRUE(bin != NULL, "1-bit alloc");
    ASSERT_EQ(bin[0], 1, "1 in 1 bit");
    free(bin);

    bin = qc_two_complement(0, 1);
    ASSERT_TRUE(bin != NULL, "1-bit 0 alloc");
    ASSERT_EQ(bin[0], 0, "0 in 1 bit");
    free(bin);

    /* Invalid width */
    bin = qc_two_complement(0, 0);
    ASSERT_TRUE(bin == NULL, "width 0 should fail");

    bin = qc_two_complement(0, 65);
    ASSERT_TRUE(bin == NULL, "width 65 should fail");
}

/* ====================================================================== */
/* Tests: NOT sequence                                                     */
/* ====================================================================== */

static void test_not_seq(void) {
    printf("  test_not_seq...\n");

    qc_sequence_t *seq = qc_not_seq(4);
    ASSERT_TRUE(seq != NULL, "not_seq alloc");
    ASSERT_EQ(seq->used_layer, 1, "not_seq layers");
    ASSERT_EQ(seq->gates_per_layer[0], 4, "not_seq gates");
    ASSERT_EQ(seq->total_gate_count, 4, "not_seq total");

    /* Each gate should be X */
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(seq->seq[0][i].Gate, QC_IGATE_X, "gate type X");
        ASSERT_EQ(seq->seq[0][i].Target, (uint32_t)i, "gate target");
    }
    qc_sequence_free(seq);

    /* Edge: width 1 */
    seq = qc_not_seq(1);
    ASSERT_TRUE(seq != NULL, "not_seq w1 alloc");
    ASSERT_EQ(seq->total_gate_count, 1, "not_seq w1 total");
    qc_sequence_free(seq);

    /* Invalid width */
    seq = qc_not_seq(0);
    ASSERT_TRUE(seq == NULL, "not_seq w0");
    seq = qc_not_seq(65);
    ASSERT_TRUE(seq == NULL, "not_seq w65");
}

/* ====================================================================== */
/* Tests: XOR sequence                                                     */
/* ====================================================================== */

static void test_xor_seq(void) {
    printf("  test_xor_seq...\n");

    qc_sequence_t *seq = qc_xor_seq(3);
    ASSERT_TRUE(seq != NULL, "xor_seq alloc");
    ASSERT_EQ(seq->used_layer, 1, "xor_seq layers");
    ASSERT_EQ(seq->gates_per_layer[0], 3, "xor_seq gates");
    ASSERT_EQ(seq->total_gate_count, 3, "xor_seq total");

    /* Each gate: CX(target=i, control=3+i) */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(seq->seq[0][i].Gate, QC_IGATE_X, "xor gate type");
        ASSERT_EQ(seq->seq[0][i].Target, (uint32_t)i, "xor target");
        ASSERT_EQ(seq->seq[0][i].NumControls, 1, "xor num ctrl");
        ASSERT_EQ(seq->seq[0][i].Control[0], (uint32_t)(3 + i), "xor ctrl");
    }
    qc_sequence_free(seq);
}

/* ====================================================================== */
/* Tests: AND sequence                                                     */
/* ====================================================================== */

static void test_and_seq(void) {
    printf("  test_and_seq...\n");

    qc_sequence_t *seq = qc_and_seq(2);
    ASSERT_TRUE(seq != NULL, "and_seq alloc");
    ASSERT_EQ(seq->used_layer, 2, "and_seq layers");
    ASSERT_EQ(seq->total_gate_count, 2, "and_seq total");

    /* Gate 0: CCX(result[0], a[0], b[0]) = CCX(0, 2, 4) */
    ASSERT_EQ(seq->seq[0][0].Gate, QC_IGATE_X, "and gate type");
    ASSERT_EQ(seq->seq[0][0].Target, 0, "and target 0");
    ASSERT_EQ(seq->seq[0][0].NumControls, 2, "and num ctrl");
    ASSERT_EQ(seq->seq[0][0].Control[0], 2, "and ctrl0");
    ASSERT_EQ(seq->seq[0][0].Control[1], 4, "and ctrl1");

    qc_sequence_free(seq);
}

/* ====================================================================== */
/* Tests: OR sequence                                                      */
/* ====================================================================== */

static void test_or_seq(void) {
    printf("  test_or_seq...\n");

    qc_sequence_t *seq = qc_or_seq(2);
    ASSERT_TRUE(seq != NULL, "or_seq alloc");
    /* Layers: 1(NOT) + 2(AND) + 1(NOT) + 1(NOT result) = 5 */
    ASSERT_EQ(seq->used_layer, 5, "or_seq layers");
    ASSERT_TRUE(seq->total_gate_count > 0, "or_seq has gates");

    qc_sequence_free(seq);
}

/* ====================================================================== */
/* Tests: NULL safety for logic operations                                 */
/* ====================================================================== */

static void test_logic_null_safety(void) {
    printf("  test_logic_null_safety...\n");

    ASSERT_EQ(qc_bitwise_not(NULL, NULL, 4), QC_ERR_NULL, "not null ctx");
    ASSERT_EQ(qc_bitwise_xor(NULL, NULL, NULL, 4), QC_ERR_NULL, "xor null ctx");
    ASSERT_EQ(qc_bitwise_and(NULL, NULL, NULL, NULL, 4), QC_ERR_NULL, "and null ctx");
    ASSERT_EQ(qc_bitwise_or(NULL, NULL, NULL, NULL, 4), QC_ERR_NULL, "or null ctx");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT_TRUE(ctx != NULL, "create ctx");

    ASSERT_EQ(qc_bitwise_not(ctx, NULL, 4), QC_ERR_NULL, "not null target");
    ASSERT_EQ(qc_bitwise_xor(ctx, NULL, NULL, 4), QC_ERR_NULL, "xor null a");

    /* Width validation */
    uint32_t target[4] = {0, 1, 2, 3};
    ASSERT_EQ(qc_bitwise_not(ctx, target, 0), QC_ERR_WIDTH, "not width 0");
    ASSERT_EQ(qc_bitwise_not(ctx, target, 65), QC_ERR_WIDTH, "not width 65");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Tests: Hot path add sequence                                            */
/* ====================================================================== */

static void test_hot_path_add_seq(void) {
    printf("  test_hot_path_add_seq...\n");

    /* 3-bit base register + MSB = 4-bit total, adding value 5 */
    qc_sequence_t *seq = qc_split_cq_add_seq(3, 5);
    ASSERT_TRUE(seq != NULL, "split add alloc");
    /* w=4, total layers = 5*4 - 2 = 18 */
    ASSERT_TRUE(seq->used_layer > 0, "split add has layers");
    ASSERT_TRUE(seq->total_gate_count > 0, "split add has gates");

    /* Verify QFT structure: first gate should be H on qubit w-1 (MSB) */
    ASSERT_EQ(seq->seq[0][0].Gate, QC_IGATE_H, "first gate is H");
    ASSERT_EQ(seq->seq[0][0].Target, 3, "H on qubit w-1 (MSB)");

    qc_sequence_free(seq);

    /* Edge: 1-bit base register */
    seq = qc_split_cq_add_seq(1, 1);
    ASSERT_TRUE(seq != NULL, "split add w1 alloc");
    ASSERT_TRUE(seq->total_gate_count > 0, "split add w1 gates");
    qc_sequence_free(seq);

    /* Invalid: bits=0 */
    seq = qc_split_cq_add_seq(0, 5);
    ASSERT_TRUE(seq == NULL, "split add w0");
}

static void test_hot_path_sub_seq(void) {
    printf("  test_hot_path_sub_seq...\n");

    qc_sequence_t *seq = qc_split_cq_sub_seq(3, 5);
    ASSERT_TRUE(seq != NULL, "split sub alloc");
    ASSERT_TRUE(seq->total_gate_count > 0, "split sub gates");

    /* sub(bits, value) should equal add(bits, -value) */
    qc_sequence_t *add_neg = qc_split_cq_add_seq(3, -5);
    ASSERT_TRUE(add_neg != NULL, "add neg alloc");
    ASSERT_EQ(seq->total_gate_count, add_neg->total_gate_count,
              "sub gate count == add(-value) gate count");

    qc_sequence_free(seq);
    qc_sequence_free(add_neg);
}

/* ====================================================================== */
/* Tests: CQ equality sequence                                             */
/* ====================================================================== */

static void test_cq_equal_seq(void) {
    printf("  test_cq_equal_seq...\n");

    /* 2-bit equality against value 3 (binary 11): no X gates needed */
    qc_sequence_t *seq = qc_cmp_cq_equal_seq(2, 3);
    ASSERT_TRUE(seq != NULL, "cq_equal alloc");
    ASSERT_TRUE(seq->total_gate_count > 0, "cq_equal has gates");
    /* For value 3 (all 1s), no X gates needed, just one CCX */
    ASSERT_EQ(seq->used_layer, 1, "cq_equal 2bit/3 layers");
    qc_sequence_free(seq);

    /* 2-bit equality against value 0 (binary 00): 2 X + 1 CCX + 2 X = 5 layers */
    seq = qc_cmp_cq_equal_seq(2, 0);
    ASSERT_TRUE(seq != NULL, "cq_equal 2/0 alloc");
    ASSERT_EQ(seq->used_layer, 5, "cq_equal 2bit/0 layers");
    qc_sequence_free(seq);

    /* 1-bit equality against value 1 */
    seq = qc_cmp_cq_equal_seq(1, 1);
    ASSERT_TRUE(seq != NULL, "cq_equal 1/1 alloc");
    ASSERT_EQ(seq->used_layer, 1, "cq_equal 1bit/1 layers (just CX)");
    qc_sequence_free(seq);

    /* Out-of-range value: should return empty seq */
    seq = qc_cmp_cq_equal_seq(2, 8);
    ASSERT_TRUE(seq != NULL, "cq_equal out-of-range alloc");
    ASSERT_EQ(seq->used_layer, 0, "cq_equal out-of-range empty");
    qc_sequence_free(seq);
}

/* ====================================================================== */
/* Tests: CQ less-than sequence                                            */
/* ====================================================================== */

static void test_cq_less_seq(void) {
    printf("  test_cq_less_seq...\n");

    /* 3-bit less than value 5 */
    qc_sequence_t *seq = qc_cmp_cq_less_seq(3, 5);
    ASSERT_TRUE(seq != NULL, "cq_less alloc");
    ASSERT_TRUE(seq->used_layer > 0, "cq_less has layers");
    ASSERT_TRUE(seq->total_gate_count > 0, "cq_less has gates");
    qc_sequence_free(seq);

    /* Invalid width */
    seq = qc_cmp_cq_less_seq(0, 5);
    ASSERT_TRUE(seq == NULL, "cq_less w0");

    seq = qc_cmp_cq_less_seq(64, 5);
    ASSERT_TRUE(seq == NULL, "cq_less w64");
}

/* ====================================================================== */
/* Tests: CQ greater-than sequence                                         */
/* ====================================================================== */

static void test_cq_greater_seq(void) {
    printf("  test_cq_greater_seq...\n");

    /* 3-bit greater than value 3 = lt(4) */
    qc_sequence_t *seq = qc_cmp_cq_greater_seq(3, 3);
    ASSERT_TRUE(seq != NULL, "cq_greater alloc");
    ASSERT_TRUE(seq->total_gate_count > 0, "cq_greater has gates");

    qc_sequence_t *lt_seq = qc_cmp_cq_less_seq(3, 4);
    ASSERT_TRUE(lt_seq != NULL, "lt(4) alloc");
    ASSERT_EQ(seq->total_gate_count, lt_seq->total_gate_count,
              "gt(3) == lt(4) gate count");

    qc_sequence_free(seq);
    qc_sequence_free(lt_seq);

    /* Greater than max value: always false */
    seq = qc_cmp_cq_greater_seq(3, 7);
    ASSERT_TRUE(seq != NULL, "cq_greater max alloc");
    ASSERT_EQ(seq->total_gate_count, 0, "cq_greater max empty");
    qc_sequence_free(seq);
}

/* ====================================================================== */
/* Tests: Logic operations with circuit context                            */
/* ====================================================================== */

static void test_logic_with_ctx(void) {
    printf("  test_logic_with_ctx...\n");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT_TRUE(ctx != NULL, "create ctx");

    /* NOT on 4 qubits: should produce 4 X gates */
    uint32_t target[4] = {0, 1, 2, 3};
    qc_error_t err = qc_bitwise_not(ctx, target, 4);
    ASSERT_EQ(err, QC_OK, "not ok");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 4, "not gate count");

    /* Reset */
    qc_circuit_reset(ctx);

    /* XOR: 3-bit, should produce 3 CX gates */
    uint32_t a[3] = {0, 1, 2};
    uint32_t b[3] = {3, 4, 5};
    err = qc_bitwise_xor(ctx, a, b, 3);
    ASSERT_EQ(err, QC_OK, "xor ok");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 3, "xor gate count");

    /* Reset */
    qc_circuit_reset(ctx);

    /* AND: 2-bit, should produce 2 CCX gates */
    uint32_t r[2] = {0, 1};
    uint32_t a2[2] = {2, 3};
    uint32_t b2[2] = {4, 5};
    err = qc_bitwise_and(ctx, r, a2, b2, 2);
    ASSERT_EQ(err, QC_OK, "and ok");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 2, "and gate count");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Tests: CQ equality with circuit context                                 */
/* ====================================================================== */

static void test_cq_equal_with_ctx(void) {
    printf("  test_cq_equal_with_ctx...\n");

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT_TRUE(ctx != NULL, "create ctx");

    /* 2-bit equality against value 3: just 1 CCX gate */
    uint32_t a[2] = {1, 2};
    uint32_t result = 0;
    qc_error_t err = qc_cmp_cq_equal(ctx, a, 2, 3, result);
    ASSERT_EQ(err, QC_OK, "cq_equal ok");
    ASSERT_TRUE(qc_circuit_gate_count(ctx) > 0, "cq_equal produced gates");

    /* NULL safety */
    ASSERT_EQ(qc_cmp_cq_equal(NULL, a, 2, 3, result), QC_ERR_NULL, "null ctx");
    ASSERT_EQ(qc_cmp_cq_equal(ctx, NULL, 2, 3, result), QC_ERR_NULL, "null a");

    /* Width validation */
    ASSERT_EQ(qc_cmp_cq_equal(ctx, a, 0, 3, result), QC_ERR_WIDTH, "width 0");
    ASSERT_EQ(qc_cmp_cq_equal(ctx, a, 65, 3, result), QC_ERR_WIDTH, "width 65");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("test_remaining: Module 1.12 tests\n");

    test_two_complement_basic();
    test_two_complement_edge();
    test_not_seq();
    test_xor_seq();
    test_and_seq();
    test_or_seq();
    test_logic_null_safety();
    test_hot_path_add_seq();
    test_hot_path_sub_seq();
    test_cq_equal_seq();
    test_cq_less_seq();
    test_cq_greater_seq();
    test_logic_with_ctx();
    test_cq_equal_with_ctx();

    printf("\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
