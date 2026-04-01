/**
 * @file test_sequence_api.c
 * @brief Tests for the public Sequence API exposed in quantum_circuit.h.
 *
 * Tests cover:
 *   - Lifecycle: alloc, gate_count == 0, free
 *   - Each builder family: build, check non-NULL, check gate_count > 0, free
 *   - qc_run_instruction replay: build seq, create circuit, run, verify
 *   - Inversion: run with invert=1, verify same gate count
 *   - NULL safety: free(NULL) doesn't crash, gate_count(NULL) returns 0
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
        printf("  %-50s ", #name);                                          \
        fflush(stdout);                                                     \
    } while (0)

#define PASS()                                                              \
    do {                                                                    \
        tests_passed++;                                                     \
        printf("[PASS]\n");                                                 \
    } while (0)

/* ====================================================================== */
/* NULL safety                                                             */
/* ====================================================================== */

static void test_null_safety(void) {
    TEST(null_safety_free);
    qc_sequence_free(NULL);  /* must not crash */
    PASS();

    TEST(null_safety_gate_count);
    assert(qc_sequence_gate_count(NULL) == 0);
    PASS();

    TEST(null_safety_compute_total);
    qc_sequence_compute_total(NULL);  /* must not crash */
    PASS();
}

/* ====================================================================== */
/* Lifecycle: alloc + gate_count + free                                    */
/* ====================================================================== */

static void test_lifecycle(void) {
    TEST(lifecycle_alloc_free);
    qc_sequence_t *seq = qc_sequence_alloc(4);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) == 0);
    qc_sequence_free(seq);
    PASS();

    TEST(lifecycle_alloc_invalid);
    assert(qc_sequence_alloc(0) == NULL);
    assert(qc_sequence_alloc(-1) == NULL);
    PASS();
}

/* ====================================================================== */
/* Comparison builders                                                     */
/* ====================================================================== */

static void test_comparison_builders(void) {
    TEST(cmp_cq_equal_seq);
    qc_sequence_t *seq = qc_cmp_cq_equal_seq(4, 5);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(cmp_cq_less_seq);
    seq = qc_cmp_cq_less_seq(4, 5);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(cmp_cq_greater_seq);
    seq = qc_cmp_cq_greater_seq(4, 5);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* Bitwise builders                                                        */
/* ====================================================================== */

static void test_bitwise_builders(void) {
    TEST(not_seq);
    qc_sequence_t *seq = qc_not_seq(4);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(xor_seq);
    seq = qc_xor_seq(4);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(and_seq);
    seq = qc_and_seq(4);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(or_seq);
    seq = qc_or_seq(4);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* Toffoli arithmetic builders                                             */
/* ====================================================================== */

static void test_toffoli_arith_builders(void) {
    TEST(split_cq_add_seq);
    qc_sequence_t *seq = qc_split_cq_add_seq(4, 3);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(split_cq_sub_seq);
    seq = qc_split_cq_sub_seq(4, 3);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    /* Kogge-Stone variants are stubs (return NULL) -- verify they are
       callable and NULL-safe to free. */
    TEST(toffoli_ks_seq_stubs_callable);
    seq = qc_toffoli_qq_add_ks_seq(4);
    qc_sequence_free(seq);  /* NULL-safe */
    seq = qc_toffoli_cq_add_ks_seq(4, 3);
    qc_sequence_free(seq);
    seq = qc_toffoli_cqq_add_ks_seq(4);
    qc_sequence_free(seq);
    seq = qc_toffoli_ccq_add_ks_seq(4, 3);
    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* QFT arithmetic builders                                                 */
/* ====================================================================== */

static void test_qft_arith_builders(void) {
    TEST(arith_qq_add_seq);
    qc_sequence_t *seq = qc_arith_qq_add_seq(4);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(arith_cq_add_seq);
    seq = qc_arith_cq_add_seq(4, 3);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(arith_cqq_add_seq);
    seq = qc_arith_cqq_add_seq(4);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(arith_ccq_add_seq);
    seq = qc_arith_ccq_add_seq(4, 3);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(arith_cq_mul_seq);
    seq = qc_arith_cq_mul_seq(4, 3);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();

    TEST(arith_qq_mul_seq);
    seq = qc_arith_qq_mul_seq(4);
    assert(seq != NULL);
    assert(qc_sequence_gate_count(seq) > 0);
    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* run_instruction replay                                                  */
/* ====================================================================== */

static void test_run_instruction(void) {
    TEST(run_instruction_replay);

    /* Build a NOT sequence for 2 bits */
    qc_sequence_t *seq = qc_not_seq(2);
    assert(seq != NULL);
    uint32_t seq_gates = qc_sequence_gate_count(seq);
    assert(seq_gates > 0);

    /* Create a circuit, allocate qubits, run the sequence */
    circuit_ctx_t *ctx = qc_circuit_create(0);
    assert(ctx != NULL);
    qc_circuit_set_simulate(ctx, true);

    uint32_t start;
    qc_error_t err = qc_qubit_alloc_n(ctx, 2, &start);
    assert(err == QC_OK);

    uint32_t qmap[2] = { start, start + 1 };
    qc_run_instruction(ctx, seq, qmap, 0);

    uint64_t circuit_gates = qc_circuit_gate_count(ctx);
    assert(circuit_gates == seq_gates);

    qc_sequence_free(seq);
    qc_circuit_destroy(ctx);
    PASS();

    /* Test inversion: same gate count */
    TEST(run_instruction_invert);
    seq = qc_not_seq(2);
    assert(seq != NULL);
    seq_gates = qc_sequence_gate_count(seq);

    ctx = qc_circuit_create(0);
    assert(ctx != NULL);
    qc_circuit_set_simulate(ctx, true);

    err = qc_qubit_alloc_n(ctx, 2, &start);
    assert(err == QC_OK);

    qmap[0] = start;
    qmap[1] = start + 1;
    qc_run_instruction(ctx, seq, qmap, 1);

    circuit_gates = qc_circuit_gate_count(ctx);
    assert(circuit_gates == seq_gates);

    qc_sequence_free(seq);
    qc_circuit_destroy(ctx);
    PASS();

    /* Test count-only mode */
    TEST(run_instruction_count_only);
    seq = qc_arith_qq_add_seq(3);
    assert(seq != NULL);
    seq_gates = qc_sequence_gate_count(seq);
    assert(seq_gates > 0);

    ctx = qc_circuit_create(0);
    assert(ctx != NULL);
    qc_circuit_set_simulate(ctx, false);

    uint32_t qmap6[6] = { 0, 1, 2, 3, 4, 5 };
    qc_run_instruction(ctx, seq, qmap6, 0);

    circuit_gates = qc_circuit_gate_count(ctx);
    assert(circuit_gates == seq_gates);

    qc_sequence_free(seq);
    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Sequence API Tests ===\n\n");

    test_null_safety();
    test_lifecycle();
    test_comparison_builders();
    test_bitwise_builders();
    test_toffoli_arith_builders();
    test_qft_arith_builders();
    test_run_instruction();

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
