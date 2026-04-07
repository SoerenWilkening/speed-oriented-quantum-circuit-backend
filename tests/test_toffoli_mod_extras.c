/**
 * @file test_toffoli_mod_extras.c
 * @brief Tests for extended Toffoli modular primitives (toffoli_mod_extras.c).
 *
 * Issue: refactor-xf0n (Step 1 of PLAN_qint_mod_primitives.md)
 *
 * Currently covers:
 *   - qc_toffoli_cmod_add_cq public wrapper (Phase A Shor unblocker)
 *
 * Convention note: like the other modular tests in test_toffoli_mult_div.c,
 * these are structural / gate-count / validation / ancilla-leak tests.
 * Pure-C state-vector simulation is not part of this package's test
 * convention; functional verification of the controlled Beauregard block
 * is exercised end-to-end by the QASM Python tests at the umbrella level.
 */

#include "quantum_circuit.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
/* Helper: run cmod_add_cq once and assert gate emission + ancilla leak    */
/* ====================================================================== */

static void run_one(circuit_ctx_t *ctx, const uint32_t *value, uint32_t n,
                    uint32_t ext_ctrl, int64_t addend, int64_t modulus,
                    const char *label) {
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    uint64_t gc_before = qc_circuit_gate_count(ctx);

    qc_error_t err = qc_toffoli_cmod_add_cq(ctx, value, n,
                                            addend, modulus, ext_ctrl);
    ASSERT(err == QC_OK, label);

    uint64_t gc_after = qc_circuit_gate_count(ctx);
    ASSERT(gc_after > gc_before, "cmod_add_cq emits gates (regression: stub returned error)");

    uint32_t after = qc_circuit_alloc_stats(ctx).current_in_use;
    ASSERT(after == baseline, "cmod_add_cq leaves no ancilla in flight (==)");
}

/* ====================================================================== */
/* Test: stub regression — confirms no longer returns INVALID_OP           */
/* ====================================================================== */

static void test_no_longer_stub(void) {
    printf("test_no_longer_stub...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context creation");

    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ext_ctrl");

    qc_error_t err = qc_toffoli_cmod_add_cq(ctx, value, 4, 3, 7, ctrl);
    ASSERT(err == QC_OK, "wrapper returns OK (not INVALID_OP)");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "wrapper emits gates");

    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: ctrl=1 and ctrl=0 polarities, multiple modulus and value choices  */
/* ====================================================================== */

static void test_ctrl_polarities(void) {
    printf("test_ctrl_polarities...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    ASSERT(ctx != NULL, "context creation");

    /* n=4 register, N=15 (Shor's classic small case) */
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ext_ctrl");

    /* ctrl=|1>: prepare ext_ctrl in |1> via X */
    qc_circuit_x(ctx, ctrl);
    run_one(ctx, value, 4, ctrl, 5, 15, "ctrl=1, N=15, addend=5");
    qc_circuit_x(ctx, ctrl);  /* restore to |0> */

    /* ctrl=|0>: ext_ctrl already |0> */
    run_one(ctx, value, 4, ctrl, 5, 15, "ctrl=0, N=15, addend=5");

    /* Different prime modulus N=17 needs n=5 */
    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);

    ctx = qc_circuit_create(64);
    uint32_t value5[5] = {0, 1, 2, 3, 4};
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ext_ctrl (N=17)");
    qc_circuit_x(ctx, ctrl);
    run_one(ctx, value5, 5, ctrl, 9, 17, "ctrl=1, N=17, addend=9");
    qc_circuit_x(ctx, ctrl);
    run_one(ctx, value5, 5, ctrl, 9, 17, "ctrl=0, N=17, addend=9");

    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: edge case — addend > N triggers internal reduction                */
/* ====================================================================== */

static void test_addend_gt_modulus(void) {
    printf("test_addend_gt_modulus...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ext_ctrl");

    /* addend = 100, modulus = 15 -> reduces to 100 % 15 = 10 */
    run_one(ctx, value, 4, ctrl, 100, 15, "addend=100 > N=15");

    /* addend == modulus -> reduces to 0 -> no-op (no gates, no ancilla) */
    uint64_t gc_before = qc_circuit_gate_count(ctx);
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    qc_error_t err = qc_toffoli_cmod_add_cq(ctx, value, 4, 15, 15, ctrl);
    ASSERT(err == QC_OK, "addend == N returns OK");
    ASSERT(qc_circuit_gate_count(ctx) == gc_before, "addend == N is no-op");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "addend == N leaves allocator untouched");

    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: boundary case — value + addend == N (the conditional sub region)  */
/* ====================================================================== */

static void test_boundary_sum_eq_modulus(void) {
    printf("test_boundary_sum_eq_modulus...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ext_ctrl");

    /* Boundary of the conditional N-subtract: e.g., v=10, addend=5, N=15. */
    qc_circuit_x(ctx, ctrl);  /* ctrl=|1> */
    run_one(ctx, value, 4, ctrl, 5, 15, "boundary 10+5==15 (ctrl=1)");
    qc_circuit_x(ctx, ctrl);

    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: input validation                                                  */
/* ====================================================================== */

static void test_validation(void) {
    printf("test_validation...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};

    ASSERT(qc_toffoli_cmod_add_cq(NULL, value, 4, 3, 7, 10) == QC_ERR_NULL,
           "null ctx -> QC_ERR_NULL");
    ASSERT(qc_toffoli_cmod_add_cq(ctx, NULL, 4, 3, 7, 10) == QC_ERR_NULL,
           "null value -> QC_ERR_NULL");
    ASSERT(qc_toffoli_cmod_add_cq(ctx, value, 0, 3, 7, 10) == QC_ERR_WIDTH,
           "width=0 -> QC_ERR_WIDTH");
    ASSERT(qc_toffoli_cmod_add_cq(ctx, value, 65, 3, 7, 10) == QC_ERR_WIDTH,
           "width>64 -> QC_ERR_WIDTH");
    ASSERT(qc_toffoli_cmod_add_cq(ctx, value, 4, 3, 0, 10) == QC_ERR_DIVISOR,
           "modulus=0 -> QC_ERR_DIVISOR");
    ASSERT(qc_toffoli_cmod_add_cq(ctx, value, 4, 3, -5, 10) == QC_ERR_DIVISOR,
           "modulus<0 -> QC_ERR_DIVISOR");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* refactor-haai: qc_toffoli_cmod_mul_cq tests (Beauregard ladder)         */
/* ====================================================================== */

static void cmul_run(circuit_ctx_t *ctx, const uint32_t *value, uint32_t n,
                     const uint32_t *result, uint32_t rn, uint32_t ext_ctrl,
                     int64_t multiplier, int64_t modulus, const char *label) {
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    uint64_t gc_before = qc_circuit_gate_count(ctx);
    qc_error_t err = qc_toffoli_cmod_mul_cq(ctx, value, n, result, rn,
                                            multiplier, modulus, ext_ctrl);
    ASSERT(err == QC_OK, label);
    (void)gc_before;
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "cmod_mul_cq strict ancilla balance");
}

static void test_cmul_no_longer_stub(void) {
    printf("test_cmul_no_longer_stub...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4]  = {0, 1, 2, 3};
    uint32_t result[4] = {4, 5, 6, 7};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ctrl");
    qc_circuit_x(ctx, ctrl);  /* ctrl=|1> */

    uint64_t gc_before = qc_circuit_gate_count(ctx);
    qc_error_t err = qc_toffoli_cmod_mul_cq(ctx, value, 4, result, 4, 3, 15, ctrl);
    ASSERT(err == QC_OK, "cmod_mul_cq returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > gc_before, "cmod_mul_cq emits gates");

    qc_circuit_x(ctx, ctrl);
    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

static void test_cmul_ancilla_balance_both_polarities(void) {
    printf("test_cmul_ancilla_balance_both_polarities...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4]  = {0, 1, 2, 3};
    uint32_t result[4] = {4, 5, 6, 7};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ctrl");

    /* ctrl=|0> */
    cmul_run(ctx, value, 4, result, 4, ctrl, 3, 15, "ctrl=0 N=15 m=3");
    /* ctrl=|1> */
    qc_circuit_x(ctx, ctrl);
    cmul_run(ctx, value, 4, result, 4, ctrl, 3, 15, "ctrl=1 N=15 m=3");
    qc_circuit_x(ctx, ctrl);

    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

static void test_cmul_c_eq_one_uses_ccx(void) {
    printf("test_cmul_c_eq_one_uses_ccx...\n");
    /* multiplier mod N == 1 hits the constant copy branch. We assert it
     * emits ccx (controlled by ext_ctrl), NOT bare cx. */
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4]  = {0, 1, 2, 3};
    uint32_t result[4] = {4, 5, 6, 7};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ctrl");

    qc_gate_counts_t gc_before = qc_circuit_gate_counts(ctx);

    /* multiplier == 1 */
    qc_error_t err = qc_toffoli_cmod_mul_cq(ctx, value, 4, result, 4, 1, 15, ctrl);
    ASSERT(err == QC_OK, "m=1 OK");
    qc_gate_counts_t gc_after1 = qc_circuit_gate_counts(ctx);
    ASSERT(gc_after1.ccx_gates > gc_before.ccx_gates,
           "m=1 emits ccx (regression: bare cx breaks ext_ctrl)");
    ASSERT(gc_after1.cx_gates == gc_before.cx_gates,
           "m=1 emits NO bare cx");

    /* multiplier == N+1 == 16 (also reduces to 1 mod 15) */
    err = qc_toffoli_cmod_mul_cq(ctx, value, 4, result, 4, 16, 15, ctrl);
    ASSERT(err == QC_OK, "m=N+1 OK");
    qc_gate_counts_t gc_after2 = qc_circuit_gate_counts(ctx);
    ASSERT(gc_after2.ccx_gates > gc_after1.ccx_gates,
           "m=N+1 emits ccx");
    ASSERT(gc_after2.cx_gates == gc_after1.cx_gates,
           "m=N+1 emits NO bare cx");

    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == 1,
           "c==1 path leaves no ancilla in flight");

    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

static void test_cmul_multiplier_gt_modulus(void) {
    printf("test_cmul_multiplier_gt_modulus...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[3]  = {0, 1, 2};
    uint32_t result[3] = {3, 4, 5};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ctrl");

    cmul_run(ctx, value, 3, result, 3, ctrl, 8, 5, "m=8 N=5 ctrl=0");
    qc_circuit_x(ctx, ctrl);
    cmul_run(ctx, value, 3, result, 3, ctrl, 8, 5, "m=8 N=5 ctrl=1");
    qc_circuit_x(ctx, ctrl);

    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

static void test_cmul_value_zero(void) {
    printf("test_cmul_value_zero...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t vstart, rstart;
    ASSERT(qc_qubit_alloc_n(ctx, 4, &vstart) == QC_OK, "alloc value");
    ASSERT(qc_qubit_alloc_n(ctx, 4, &rstart) == QC_OK, "alloc result");
    uint32_t value[4]  = {vstart, vstart+1, vstart+2, vstart+3};
    uint32_t result[4] = {rstart, rstart+1, rstart+2, rstart+3};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ctrl");

    cmul_run(ctx, value, 4, result, 4, ctrl, 7, 15, "value=0 ctrl=0");
    qc_circuit_x(ctx, ctrl);
    cmul_run(ctx, value, 4, result, 4, ctrl, 7, 15, "value=0 ctrl=1");
    qc_circuit_x(ctx, ctrl);

    qc_qubit_free(ctx, ctrl);
    qc_qubit_free_n(ctx, rstart, 4);
    qc_qubit_free_n(ctx, vstart, 4);
    qc_circuit_destroy(ctx);
}

static void test_cmul_n17_prime(void) {
    printf("test_cmul_n17_prime...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[5]  = {0, 1, 2, 3, 4};
    uint32_t result[5] = {5, 6, 7, 8, 9};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ctrl");

    qc_circuit_x(ctx, ctrl);
    cmul_run(ctx, value, 5, result, 5, ctrl, 6, 17, "N=17 m=6 ctrl=1");
    qc_circuit_x(ctx, ctrl);
    cmul_run(ctx, value, 5, result, 5, ctrl, 6, 17, "N=17 m=6 ctrl=0");

    qc_qubit_free(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

static void test_cmul_validation(void) {
    printf("test_cmul_validation...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4]  = {0, 1, 2, 3};
    uint32_t result[4] = {4, 5, 6, 7};

    ASSERT(qc_toffoli_cmod_mul_cq(NULL, value, 4, result, 4, 3, 15, 8) == QC_ERR_NULL,
           "null ctx -> NULL");
    ASSERT(qc_toffoli_cmod_mul_cq(ctx, NULL, 4, result, 4, 3, 15, 8) == QC_ERR_NULL,
           "null value -> NULL");
    ASSERT(qc_toffoli_cmod_mul_cq(ctx, value, 4, NULL, 4, 3, 15, 8) == QC_ERR_NULL,
           "null result -> NULL");
    ASSERT(qc_toffoli_cmod_mul_cq(ctx, value, 0, result, 4, 3, 15, 8) == QC_ERR_WIDTH,
           "value_bits=0 -> WIDTH");
    ASSERT(qc_toffoli_cmod_mul_cq(ctx, value, 4, result, 0, 3, 15, 8) == QC_ERR_WIDTH,
           "result_bits=0 -> WIDTH");
    ASSERT(qc_toffoli_cmod_mul_cq(ctx, value, 65, result, 4, 3, 15, 8) == QC_ERR_WIDTH,
           "value_bits>64 -> WIDTH");
    ASSERT(qc_toffoli_cmod_mul_cq(ctx, value, 4, result, 4, 3, 0, 8) == QC_ERR_DIVISOR,
           "modulus=0 -> DIVISOR");
    ASSERT(qc_toffoli_cmod_mul_cq(ctx, value, 4, result, 4, 3, -3, 8) == QC_ERR_DIVISOR,
           "modulus<0 -> DIVISOR");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* refactor-y2cb: qc_toffoli_mod_sub_cq classical adapter tests           */
/* ====================================================================== */

static void sub_run(circuit_ctx_t *ctx, const uint32_t *value, uint32_t n,
                    int64_t subtrahend, int64_t modulus, const char *label) {
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    uint64_t gc_before = qc_circuit_gate_count(ctx);
    qc_error_t err = qc_toffoli_mod_sub_cq(ctx, value, n, subtrahend, modulus);
    ASSERT(err == QC_OK, label);
    ASSERT(qc_circuit_gate_count(ctx) > gc_before,
           "mod_sub_cq emits gates");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "mod_sub_cq strict ancilla balance");
}

static void test_sub_no_longer_stub(void) {
    printf("test_sub_no_longer_stub...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    qc_error_t err = qc_toffoli_mod_sub_cq(ctx, value, 4, 3, 7);
    ASSERT(err == QC_OK, "wrapper returns OK (not INVALID_OP)");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "emits gates");
    qc_circuit_destroy(ctx);
}

static void test_sub_underflow_nonpow2(void) {
    /* N=15, value=3, classical_val=10: (3 - 10) mod 15 = 8.
     * Non-power-of-two N — this is the test that catches any
     * two's-complement underflow bug the planner forbade. Structural
     * check only: gate emission + ancilla balance. */
    printf("test_sub_underflow_nonpow2...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    sub_run(ctx, value, 4, 10, 15, "N=15, s=10 (underflow path)");
    qc_circuit_destroy(ctx);
}

static void test_sub_classical_gt_n(void) {
    /* N=5, s=8: reduces to 8 mod 5 = 3, negated = 2. */
    printf("test_sub_classical_gt_n...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[3] = {0, 1, 2};
    sub_run(ctx, value, 3, 8, 5, "N=5, s=8 > N");
    qc_circuit_destroy(ctx);
}

static void test_sub_classical_zero(void) {
    /* s=0 -> negated=0 -> underlying mod_add_cq is a no-op. */
    printf("test_sub_classical_zero...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    qc_error_t err = qc_toffoli_mod_sub_cq(ctx, value, 4, 0, 15);
    ASSERT(err == QC_OK, "s=0 OK");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "s=0 strict ancilla balance");
    qc_circuit_destroy(ctx);
}

static void test_sub_classical_eq_n(void) {
    /* s=N -> reduced=0 -> negated=0 -> no-op. */
    printf("test_sub_classical_eq_n...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    qc_error_t err = qc_toffoli_mod_sub_cq(ctx, value, 4, 15, 15);
    ASSERT(err == QC_OK, "s=N OK");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "s=N strict ancilla balance");
    qc_circuit_destroy(ctx);
}

static void test_sub_validation(void) {
    printf("test_sub_validation...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};

    ASSERT(qc_toffoli_mod_sub_cq(NULL, value, 4, 3, 7) == QC_ERR_NULL,
           "null ctx -> NULL");
    ASSERT(qc_toffoli_mod_sub_cq(ctx, NULL, 4, 3, 7) == QC_ERR_NULL,
           "null value -> NULL");
    ASSERT(qc_toffoli_mod_sub_cq(ctx, value, 0, 3, 7) == QC_ERR_WIDTH,
           "n_bits=0 -> WIDTH");
    ASSERT(qc_toffoli_mod_sub_cq(ctx, value, 65, 3, 7) == QC_ERR_WIDTH,
           "n_bits>64 -> WIDTH");
    ASSERT(qc_toffoli_mod_sub_cq(ctx, value, 4, 3, 1) == QC_ERR_DIVISOR,
           "modulus<2 -> DIVISOR");
    ASSERT(qc_toffoli_mod_sub_cq(ctx, value, 4, 3, 0) == QC_ERR_DIVISOR,
           "modulus=0 -> DIVISOR");

    qc_circuit_destroy(ctx);
}

int main(void) {
    printf("=== test_toffoli_mod_extras (refactor-xf0n + refactor-haai + refactor-y2cb) ===\n");
    test_no_longer_stub();
    test_ctrl_polarities();
    test_addend_gt_modulus();
    test_boundary_sum_eq_modulus();
    test_validation();
    test_cmul_no_longer_stub();
    test_cmul_ancilla_balance_both_polarities();
    test_cmul_c_eq_one_uses_ccx();
    test_cmul_multiplier_gt_modulus();
    test_cmul_value_zero();
    test_cmul_n17_prime();
    test_cmul_validation();
    test_sub_no_longer_stub();
    test_sub_underflow_nonpow2();
    test_sub_classical_gt_n();
    test_sub_classical_zero();
    test_sub_classical_eq_n();
    test_sub_validation();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
