/**
 * @file test_toffoli_seq_builders.c
 * @brief Smoke tests for toffoli multiplication and CQ comparison sequence builders.
 *
 * Issue: refactor-4ma, refactor-fdg (Issues 4+5)
 */

#include "../src/internal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define PASS(name) printf("  PASS: %s\n", name)

/* ====================================================================== */
/* Test: toffoli CQ multiplication sequence                                */
/* ====================================================================== */

static void test_toffoli_cq_mul_seq(void) {
    qc_sequence_t *seq = qc_toffoli_cq_mul_seq(4, 3);
    assert(seq != NULL);
    assert(seq->total_qubits >= 8);       /* at least 2*n = 8 register qubits */
    assert(seq->total_gate_count > 0);
    qc_sequence_free(seq);
    PASS("test_toffoli_cq_mul_seq");
}

/* ====================================================================== */
/* Test: toffoli QQ multiplication sequence                                */
/* ====================================================================== */

static void test_toffoli_qq_mul_seq(void) {
    qc_sequence_t *seq = qc_toffoli_qq_mul_seq(4);
    assert(seq != NULL);
    assert(seq->total_qubits >= 12);      /* at least 3*n = 12 register qubits */
    assert(seq->total_gate_count > 0);
    qc_sequence_free(seq);
    PASS("test_toffoli_qq_mul_seq");
}

/* ====================================================================== */
/* Test: toffoli CQ less-than sequence                                     */
/* ====================================================================== */

static void test_toffoli_cq_less_seq(void) {
    qc_sequence_t *seq = qc_cmp_cq_less_toffoli_seq(4, 5);
    assert(seq != NULL);
    assert(seq->total_qubits >= 6);       /* result + 4 A + borrow = 6 */
    assert(seq->total_gate_count > 0);
    qc_sequence_free(seq);
    PASS("test_toffoli_cq_less_seq");
}

/* ====================================================================== */
/* Test: toffoli CQ greater-than sequence                                  */
/* ====================================================================== */

static void test_toffoli_cq_greater_seq(void) {
    qc_sequence_t *seq = qc_cmp_cq_greater_toffoli_seq(4, 5);
    assert(seq != NULL);
    assert(seq->total_qubits >= 6);
    assert(seq->total_gate_count > 0);
    qc_sequence_free(seq);
    PASS("test_toffoli_cq_greater_seq");
}

/* ====================================================================== */
/* Test: controlled toffoli CQ less-than sequence                          */
/* ====================================================================== */

static void test_controlled_cq_less_seq(void) {
    qc_sequence_t *seq = qc_c_cmp_cq_less_toffoli_seq(4, 5);
    assert(seq != NULL);
    assert(seq->total_qubits >= 7);       /* result + 4 A + borrow + ctrl = 7 */
    assert(seq->total_gate_count > 0);
    qc_sequence_free(seq);
    PASS("test_controlled_cq_less_seq");
}

/* ====================================================================== */
/* Test: controlled toffoli CQ greater-than sequence                       */
/* ====================================================================== */

static void test_controlled_cq_greater_seq(void) {
    qc_sequence_t *seq = qc_c_cmp_cq_greater_toffoli_seq(4, 5);
    assert(seq != NULL);
    assert(seq->total_qubits >= 7);
    assert(seq->total_gate_count > 0);
    qc_sequence_free(seq);
    PASS("test_controlled_cq_greater_seq");
}

/* ====================================================================== */
/* Test: invalid inputs return NULL                                        */
/* ====================================================================== */

static void test_invalid_inputs(void) {
    /* bits = 0 */
    assert(qc_toffoli_cq_mul_seq(0, 3) == NULL);
    assert(qc_toffoli_qq_mul_seq(0) == NULL);
    assert(qc_cmp_cq_less_toffoli_seq(0, 5) == NULL);
    assert(qc_cmp_cq_greater_toffoli_seq(0, 5) == NULL);
    assert(qc_c_cmp_cq_less_toffoli_seq(0, 5) == NULL);
    assert(qc_c_cmp_cq_greater_toffoli_seq(0, 5) == NULL);

    /* bits = 65 (mul) or bits = 64 (cmp, max is 63) */
    assert(qc_toffoli_cq_mul_seq(65, 3) == NULL);
    assert(qc_toffoli_qq_mul_seq(65) == NULL);
    assert(qc_cmp_cq_less_toffoli_seq(64, 5) == NULL);
    assert(qc_cmp_cq_greater_toffoli_seq(64, 5) == NULL);
    assert(qc_c_cmp_cq_less_toffoli_seq(64, 5) == NULL);
    assert(qc_c_cmp_cq_greater_toffoli_seq(64, 5) == NULL);

    PASS("test_invalid_inputs");
}

/* ====================================================================== */
/* Test: CQ greater-than with max value returns NULL (always false)         */
/* ====================================================================== */

static void test_edge_cq_greater_max(void) {
    /* For 4 bits, max value = 15.  A > 15 is always false. */
    assert(qc_cmp_cq_greater_toffoli_seq(4, 15) == NULL);
    assert(qc_c_cmp_cq_greater_toffoli_seq(4, 15) == NULL);

    /* value = 14 should still work (A > 14 iff A < 15) */
    qc_sequence_t *seq = qc_cmp_cq_greater_toffoli_seq(4, 14);
    assert(seq != NULL);
    qc_sequence_free(seq);

    PASS("test_edge_cq_greater_max");
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Toffoli Sequence Builders Tests ===\n");

    test_toffoli_cq_mul_seq();
    test_toffoli_qq_mul_seq();
    test_toffoli_cq_less_seq();
    test_toffoli_cq_greater_seq();
    test_controlled_cq_less_seq();
    test_controlled_cq_greater_seq();
    test_invalid_inputs();
    test_edge_cq_greater_max();

    printf("All tests passed.\n");
    return 0;
}
