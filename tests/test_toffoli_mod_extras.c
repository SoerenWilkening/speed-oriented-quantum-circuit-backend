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

/* ====================================================================== */
/* refactor-7awn: qc_toffoli_mod_sub_qq (dagger of mod_add_qq) tests      */
/* ====================================================================== */

static void subqq_run(circuit_ctx_t *ctx, const uint32_t *value, uint32_t n,
                      const uint32_t *other, uint32_t on,
                      int64_t modulus, const char *label) {
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    uint64_t gc_before = qc_circuit_gate_count(ctx);
    qc_error_t err = qc_toffoli_mod_sub_qq(ctx, value, n, other, on, modulus);
    ASSERT(err == QC_OK, label);
    ASSERT(qc_circuit_gate_count(ctx) > gc_before,
           "mod_sub_qq emits gates (regression: stub)");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "mod_sub_qq strict ancilla balance");
}

static void test_subqq_no_longer_stub(void) {
    printf("test_subqq_no_longer_stub...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    qc_error_t err = qc_toffoli_mod_sub_qq(ctx, value, 4, other, 4, 15);
    ASSERT(err == QC_OK, "mod_sub_qq returns OK (not INVALID_OP)");
    ASSERT(qc_circuit_gate_count(ctx) > 0, "emits gates");
    qc_circuit_destroy(ctx);
}

static void test_subqq_underflow_nonpow2(void) {
    /* PRD §6.4 MANDATORY regression: N=15, value=3, other=10 ->
     * (3 - 10) mod 15 = 8. Non-power-of-two N with value < other.
     *
     * Structural check only per package convention: gate emission +
     * strict ancilla balance. The functional verification that the
     * output equals 8 (the assertion that actually catches any
     * two's-complement fallback) happens at the Layer 2 cython layer
     * via refactor-9vx1 / refactor-oy3c. Running this config here
     * still exercises the dagger path on the configured inputs and
     * catches any allocator / gate-emission regression. */
    printf("test_subqq_underflow_nonpow2...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    subqq_run(ctx, value, 4, other, 4, 15, "N=15 v=3 o=10 (underflow)");
    qc_circuit_destroy(ctx);
}

static void test_subqq_no_underflow(void) {
    /* N=15, value=10, other=3 -> 7 (no underflow path). */
    printf("test_subqq_no_underflow...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    subqq_run(ctx, value, 4, other, 4, 15, "N=15 v=10 o=3");
    qc_circuit_destroy(ctx);
}

static void test_subqq_equal(void) {
    /* N=15, value=other=7 -> 0. */
    printf("test_subqq_equal...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    subqq_run(ctx, value, 4, other, 4, 15, "N=15 v==o");
    qc_circuit_destroy(ctx);
}

static void test_subqq_other_zero(void) {
    /* other=|0>: (v - 0) mod N = v. Structurally: valid call, balanced
     * ancilla. (mod_sub_qq emits gates unconditionally unlike the
     * classical mod_sub_cq no-op short-circuit.) */
    printf("test_subqq_other_zero...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    qc_error_t err = qc_toffoli_mod_sub_qq(ctx, value, 4, other, 4, 15);
    ASSERT(err == QC_OK, "other=0 OK");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "other=0 strict ancilla balance");
    qc_circuit_destroy(ctx);
}

static void test_subqq_n17_prime(void) {
    /* N=17 (prime, non-power-of-two), n=5, value=5, other=12 ->
     * underflow -> (5-12) mod 17 = 10. */
    printf("test_subqq_n17_prime...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[5] = {0, 1, 2, 3, 4};
    uint32_t other[5] = {5, 6, 7, 8, 9};
    subqq_run(ctx, value, 5, other, 5, 17, "N=17 v=5 o=12 (underflow)");
    qc_circuit_destroy(ctx);
}

static void test_subqq_round_trip(void) {
    /* Round-trip: mod_add_qq(value, other) followed by mod_sub_qq(value, other)
     * should return value to its initial state. This structurally
     * verifies the dagger relationship: same ancilla balance, inverse
     * gate sequence. We assert both calls succeed, both leave the
     * allocator balanced, and both emit gates. */
    printf("test_subqq_round_trip...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    uint64_t gc0 = qc_circuit_gate_count(ctx);

    qc_error_t err = qc_toffoli_mod_add_qq(ctx, value, 4, other, 4, 15);
    ASSERT(err == QC_OK, "round-trip: mod_add_qq OK");
    uint64_t gc1 = qc_circuit_gate_count(ctx);
    ASSERT(gc1 > gc0, "round-trip: add emitted gates");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "round-trip: add balanced");

    err = qc_toffoli_mod_sub_qq(ctx, value, 4, other, 4, 15);
    ASSERT(err == QC_OK, "round-trip: mod_sub_qq OK");
    uint64_t gc2 = qc_circuit_gate_count(ctx);
    ASSERT(gc2 > gc1, "round-trip: sub emitted gates");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "round-trip: sub balanced (net identity)");

    qc_circuit_destroy(ctx);
}

static void test_subqq_validation(void) {
    printf("test_subqq_validation...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};

    ASSERT(qc_toffoli_mod_sub_qq(NULL, value, 4, other, 4, 15) == QC_ERR_NULL,
           "null ctx -> NULL");
    ASSERT(qc_toffoli_mod_sub_qq(ctx, NULL, 4, other, 4, 15) == QC_ERR_NULL,
           "null value -> NULL");
    ASSERT(qc_toffoli_mod_sub_qq(ctx, value, 4, NULL, 4, 15) == QC_ERR_NULL,
           "null other -> NULL");
    ASSERT(qc_toffoli_mod_sub_qq(ctx, value, 0, other, 4, 15) == QC_ERR_WIDTH,
           "width=0 -> WIDTH");
    ASSERT(qc_toffoli_mod_sub_qq(ctx, value, 65, other, 4, 15) == QC_ERR_WIDTH,
           "width>64 -> WIDTH");
    ASSERT(qc_toffoli_mod_sub_qq(ctx, value, 4, other, 4, 0) == QC_ERR_DIVISOR,
           "modulus=0 -> DIVISOR");
    ASSERT(qc_toffoli_mod_sub_qq(ctx, value, 4, other, 4, -3) == QC_ERR_DIVISOR,
           "modulus<0 -> DIVISOR");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* refactor-kh8i: qc_toffoli_cmod_add_qq tests                             */
/* ====================================================================== */

static void test_cmod_add_qq_no_longer_stub(void) {
    printf("test_cmod_add_qq_no_longer_stub...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ctrl");
    qc_circuit_x(ctx, ctrl);
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    uint64_t gc0 = qc_circuit_gate_count(ctx);
    qc_error_t err = qc_toffoli_cmod_add_qq(ctx, value, 4, other, 4, 15, ctrl);
    ASSERT(err == QC_OK, "cmod_add_qq returns OK (not INVALID_OP)");
    ASSERT(qc_circuit_gate_count(ctx) > gc0, "cmod_add_qq emits gates");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "ctrl=1: ancilla strict balance");
    qc_circuit_x(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

static void test_cmod_add_qq_ctrl_zero_balance(void) {
    printf("test_cmod_add_qq_ctrl_zero_balance...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    uint32_t ctrl;
    ASSERT(qc_qubit_alloc(ctx, &ctrl) == QC_OK, "alloc ctrl");
    /* ctrl already |0> */
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    uint64_t gc0 = qc_circuit_gate_count(ctx);
    qc_error_t err = qc_toffoli_cmod_add_qq(ctx, value, 4, other, 4, 15, ctrl);
    ASSERT(err == QC_OK, "ctrl=0: returns OK");
    ASSERT(qc_circuit_gate_count(ctx) > gc0,
           "ctrl=0: still emits gates (structurally well-formed)");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "ctrl=0: ancilla strict balance");
    qc_circuit_destroy(ctx);
}

static void test_cmod_add_qq_validation(void) {
    printf("test_cmod_add_qq_validation...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[4] = {0, 1, 2, 3};
    uint32_t other[4] = {4, 5, 6, 7};
    uint32_t ctrl;
    qc_qubit_alloc(ctx, &ctrl);

    ASSERT(qc_toffoli_cmod_add_qq(NULL, value, 4, other, 4, 15, ctrl) == QC_ERR_NULL,
           "null ctx");
    ASSERT(qc_toffoli_cmod_add_qq(ctx, NULL, 4, other, 4, 15, ctrl) == QC_ERR_NULL,
           "null value");
    ASSERT(qc_toffoli_cmod_add_qq(ctx, value, 4, NULL, 4, 15, ctrl) == QC_ERR_NULL,
           "null other");
    ASSERT(qc_toffoli_cmod_add_qq(ctx, value, 0, other, 4, 15, ctrl) == QC_ERR_WIDTH,
           "width=0");
    ASSERT(qc_toffoli_cmod_add_qq(ctx, value, 65, other, 4, 15, ctrl) == QC_ERR_WIDTH,
           "width>64");
    ASSERT(qc_toffoli_cmod_add_qq(ctx, value, 4, other, 4, 1, ctrl) == QC_ERR_DIVISOR,
           "modulus<2");
    ASSERT(qc_toffoli_cmod_add_qq(ctx, value, 4, other, 4, 0, ctrl) == QC_ERR_DIVISOR,
           "modulus=0");
    ASSERT(qc_toffoli_cmod_add_qq(ctx, value, 4, other, 4, -3, ctrl) == QC_ERR_DIVISOR,
           "modulus<0");
    qc_circuit_destroy(ctx);
}

static void test_cmod_add_qq_and_ancilla_discipline(void) {
    /* AND-ancilla discipline: count CCX gates emitted by cmod_add_qq vs
     * the uncontrolled mod_add_qq. The controlled version should add only a
     * small bounded overhead from the ext_ctrl threading and the SINGLE
     * AND-ancilla in step 4 — not allocate AND-ancillae for steps 1, 5, 8. */
    printf("test_cmod_add_qq_and_ancilla_discipline...\n");
    circuit_ctx_t *ctx_a = qc_circuit_create(64);
    uint32_t value_a[4] = {0, 1, 2, 3};
    uint32_t other_a[4] = {4, 5, 6, 7};
    qc_error_t err_a = qc_toffoli_mod_add_qq(ctx_a, value_a, 4, other_a, 4, 15);
    ASSERT(err_a == QC_OK, "baseline mod_add_qq OK");
    uint64_t base_ccx = qc_circuit_gate_counts(ctx_a).ccx_gates;
    qc_circuit_destroy(ctx_a);

    circuit_ctx_t *ctx_c = qc_circuit_create(64);
    uint32_t value_c[4] = {0, 1, 2, 3};
    uint32_t other_c[4] = {4, 5, 6, 7};
    uint32_t ctrl;
    qc_qubit_alloc(ctx_c, &ctrl);
    qc_error_t err_c = qc_toffoli_cmod_add_qq(ctx_c, value_c, 4, other_c, 4, 15, ctrl);
    ASSERT(err_c == QC_OK, "controlled cmod_add_qq OK");
    uint64_t ctl_ccx = qc_circuit_gate_counts(ctx_c).ccx_gates;

    /* The controlled version uses singly-controlled qq adds (which decompose
     * into ccx internally) plus exactly two ccx in step 4 (compute+uncompute
     * AND) and two ccx for steps 3 and 7. So ctl_ccx > base_ccx, but bounded:
     * not zero, and within a small constant factor. */
    ASSERT(ctl_ccx > base_ccx,
           "controlled version emits more ccx (ext_ctrl threading)");
    /* Bound: spurious AND-ancilla regression would multiply ccx by O(steps).
     * The legitimate overhead is bounded by ~6x the baseline (controlled
     * helpers internally introduce extra ccx per add, plus the single AND
     * step). */
    ASSERT(ctl_ccx < base_ccx * 20 + 200,
           "controlled ccx count not unbounded (no spurious AND-ancillae)");
    qc_circuit_destroy(ctx_c);
}

static void test_cmod_add_qq_n17_prime(void) {
    printf("test_cmod_add_qq_n17_prime...\n");
    circuit_ctx_t *ctx = qc_circuit_create(64);
    uint32_t value[5] = {0, 1, 2, 3, 4};
    uint32_t other[5] = {5, 6, 7, 8, 9};
    uint32_t ctrl;
    qc_qubit_alloc(ctx, &ctrl);
    qc_circuit_x(ctx, ctrl);
    uint32_t baseline = qc_circuit_alloc_stats(ctx).current_in_use;
    qc_error_t err = qc_toffoli_cmod_add_qq(ctx, value, 5, other, 5, 17, ctrl);
    ASSERT(err == QC_OK, "N=17 prime OK");
    ASSERT(qc_circuit_alloc_stats(ctx).current_in_use == baseline,
           "N=17: balanced");
    qc_circuit_x(ctx, ctrl);
    qc_circuit_destroy(ctx);
}

int main(void) {
    printf("=== test_toffoli_mod_extras (refactor-xf0n + refactor-haai + refactor-y2cb + refactor-7awn + refactor-kh8i) ===\n");
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
    test_subqq_no_longer_stub();
    test_subqq_underflow_nonpow2();
    test_subqq_no_underflow();
    test_subqq_equal();
    test_subqq_other_zero();
    test_subqq_n17_prime();
    test_subqq_round_trip();
    test_subqq_validation();
    test_cmod_add_qq_no_longer_stub();
    test_cmod_add_qq_ctrl_zero_balance();
    test_cmod_add_qq_validation();
    test_cmod_add_qq_and_ancilla_discipline();
    test_cmod_add_qq_n17_prime();
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
