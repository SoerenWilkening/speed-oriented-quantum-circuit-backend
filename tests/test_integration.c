/**
 * @file test_integration.c
 * @brief End-to-end integration tests for libquantum (Module 1.14, refactor-7qd).
 * Covers: lifecycle, gates, arithmetic (widths 1-16 QFT+Toffoli), allocation,
 * optimization, QASM output, and full pipeline. Max 17 qubits per test.
 */

#include <quantum_circuit.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====================================================================== */
/* Test infrastructure                                                     */
/* ====================================================================== */

static int g_run = 0, g_pass = 0, g_fail = 0;

#define TEST(name) do { g_run++; printf("  %-55s ", name); } while (0)
#define PASS()     do { g_pass++; printf("PASS\n"); } while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { g_fail++; printf("FAIL: %s (line %d)\n", msg, __LINE__); return; } \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((long long)(a) != (long long)(b)) { \
        g_fail++; printf("FAIL: %s — exp %lld got %lld (line %d)\n", \
            msg, (long long)(b), (long long)(a), __LINE__); return; } \
} while (0)

#define ASSERT_GT(a, b, msg) do { \
    if ((long long)(a) <= (long long)(b)) { \
        g_fail++; printf("FAIL: %s — exp >%lld got %lld (line %d)\n", \
            msg, (long long)(b), (long long)(a), __LINE__); return; } \
} while (0)

/* ====================================================================== */
/* 1. Circuit lifecycle                                                    */
/* ====================================================================== */

static void test_lifecycle(void) {
    TEST("lifecycle: create -> gates -> stats -> reset -> destroy");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 0, "initial gc");

    qc_circuit_x(ctx, 0);
    qc_circuit_h(ctx, 1);
    qc_circuit_cx(ctx, 0, 2);
    qc_circuit_ccx(ctx, 0, 1, 3);
    ASSERT_EQ(qc_circuit_gate_count(ctx), 4, "4 gates");
    ASSERT_GT(qc_circuit_depth(ctx), 0, "depth > 0");

    qc_gate_counts_t c = qc_circuit_gate_counts(ctx);
    ASSERT_EQ(c.x_gates, 1, "x"); ASSERT_EQ(c.h_gates, 1, "h");
    ASSERT_EQ(c.cx_gates, 1, "cx"); ASSERT_EQ(c.ccx_gates, 1, "ccx");

    ASSERT_EQ(qc_circuit_reset(ctx), QC_OK, "reset");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 0, "gc after reset");

    qc_circuit_destroy(ctx);
    qc_circuit_destroy(NULL); /* safe no-op */
    PASS();
}

/* ====================================================================== */
/* 2. Gate emission and retrieval                                          */
/* ====================================================================== */

static void test_single_qubit_gates(void) {
    TEST("gates: single-qubit (X,Y,Z,H,T,Tdg,P,Rx,Ry,Rz)");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    ASSERT_EQ(qc_circuit_x(ctx, 0), QC_OK, "x");
    ASSERT_EQ(qc_circuit_y(ctx, 1), QC_OK, "y");
    ASSERT_EQ(qc_circuit_z(ctx, 2), QC_OK, "z");
    ASSERT_EQ(qc_circuit_h(ctx, 3), QC_OK, "h");
    ASSERT_EQ(qc_circuit_t_gate(ctx, 4), QC_OK, "t");
    ASSERT_EQ(qc_circuit_tdg(ctx, 5), QC_OK, "tdg");
    ASSERT_EQ(qc_circuit_p(ctx, 6, M_PI / 4), QC_OK, "p");
    ASSERT_EQ(qc_circuit_rx(ctx, 7, M_PI / 2), QC_OK, "rx");
    ASSERT_EQ(qc_circuit_ry(ctx, 8, M_PI / 3), QC_OK, "ry");
    ASSERT_EQ(qc_circuit_rz(ctx, 9, M_PI / 6), QC_OK, "rz");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 10, "10 gates");
    qc_gate_counts_t gc = qc_circuit_gate_counts(ctx);
    ASSERT_EQ(gc.x_gates, 1, "x"); ASSERT_EQ(gc.h_gates, 1, "h");
    ASSERT_EQ(gc.t_gates, 1, "t"); ASSERT_EQ(gc.tdg_gates, 1, "tdg");
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_two_qubit_gates(void) {
    TEST("gates: two-qubit (CX,CY,CZ,CH,CP,CRY)");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    ASSERT_EQ(qc_circuit_cx(ctx, 0, 1), QC_OK, "cx");
    ASSERT_EQ(qc_circuit_cy(ctx, 2, 3), QC_OK, "cy");
    ASSERT_EQ(qc_circuit_cz(ctx, 4, 5), QC_OK, "cz");
    ASSERT_EQ(qc_circuit_ch(ctx, 6, 7), QC_OK, "ch");
    ASSERT_EQ(qc_circuit_cp(ctx, 8, 9, M_PI / 2), QC_OK, "cp");
    ASSERT_EQ(qc_circuit_cry(ctx, 10, 11, M_PI / 4), QC_OK, "cry");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 6, "6 gates");
    ASSERT_EQ(qc_circuit_width(ctx), 12, "width 12");
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_multi_qubit_gates(void) {
    TEST("gates: multi-qubit (CCX, MCX, MCZ) + generic add_gate");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    ASSERT_EQ(qc_circuit_ccx(ctx, 0, 1, 2), QC_OK, "ccx");
    uint32_t c3[] = {3, 4, 5};
    ASSERT_EQ(qc_circuit_mcx(ctx, c3, 3, 6), QC_OK, "mcx");
    uint32_t c2[] = {7, 8};
    ASSERT_EQ(qc_circuit_mcz(ctx, c2, 2, 9), QC_OK, "mcz");
    /* generic add_gate */
    ASSERT_EQ(qc_circuit_add_gate(ctx, QC_GATE_X, 10, NULL, 0, 0), QC_OK, "add X");
    uint32_t ctrl = 11;
    ASSERT_EQ(qc_circuit_add_gate(ctx, QC_GATE_CX, 12, &ctrl, 1, 0), QC_OK, "add CX");
    ASSERT_GT(qc_circuit_gate_count(ctx), 0, "gates");
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_depth_tracking(void) {
    TEST("gates: depth — parallel vs sequential");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    /* Parallel: disjoint qubits share layer */
    qc_circuit_x(ctx, 0); qc_circuit_x(ctx, 1);
    qc_circuit_x(ctx, 2); qc_circuit_x(ctx, 3);
    ASSERT_EQ(qc_circuit_depth(ctx), 1, "parallel depth 1");
    qc_circuit_destroy(ctx);

    ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create2");
    /* Sequential: same qubit */
    qc_circuit_x(ctx, 0); qc_circuit_h(ctx, 0); qc_circuit_x(ctx, 0);
    ASSERT_GT(qc_circuit_depth(ctx), 1, "sequential depth > 1");
    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* 3. Arithmetic: widths 1-16, QFT and Toffoli                            */
/* ====================================================================== */

/* Helper: run QQ add for all widths 1..16 in given mode */
static int arith_qq_add_widths(qc_arith_mode_t mode) {
    for (uint32_t w = 1; w <= 16; w++) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        if (!ctx) return -1;
        qc_circuit_set_arith_mode(ctx, mode);
        uint32_t a[16], b[16];
        for (uint32_t i = 0; i < w; i++) { a[i] = i; b[i] = w + i; }
        qc_error_t err = (mode == QC_ARITH_QFT)
            ? qc_arith_qq_add(ctx, a, b, w)
            : qc_toffoli_qq_add(ctx, a, b, w);
        uint64_t gc = qc_circuit_gate_count(ctx);
        qc_circuit_destroy(ctx);
        if (err != QC_OK || gc == 0) return (int)w;
    }
    return 0;
}

/* Helper: run CQ add for all widths 1..16 in given mode */
static int arith_cq_add_widths(qc_arith_mode_t mode, int64_t value) {
    for (uint32_t w = 1; w <= 16; w++) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        if (!ctx) return -1;
        qc_circuit_set_arith_mode(ctx, mode);
        uint32_t t[16];
        for (uint32_t i = 0; i < w; i++) t[i] = i;
        qc_error_t err = (mode == QC_ARITH_QFT)
            ? qc_arith_cq_add(ctx, t, w, value)
            : qc_toffoli_cq_add(ctx, t, w, value);
        uint64_t gc = qc_circuit_gate_count(ctx);
        qc_circuit_destroy(ctx);
        if (err != QC_OK || gc == 0) return (int)w;
    }
    return 0;
}

static void test_arith_qq_add(void) {
    TEST("arith: QFT + Toffoli QQ add widths 1-16");
    int r = arith_qq_add_widths(QC_ARITH_QFT);
    ASSERT(r == 0, "QFT qq_add failed");
    r = arith_qq_add_widths(QC_ARITH_TOFFOLI);
    ASSERT(r == 0, "Toffoli qq_add failed");
    PASS();
}

static void test_arith_cq_add(void) {
    TEST("arith: QFT + Toffoli CQ add widths 1-16");
    ASSERT(arith_cq_add_widths(QC_ARITH_QFT, 42) == 0, "QFT cq_add");
    ASSERT(arith_cq_add_widths(QC_ARITH_TOFFOLI, 7) == 0, "Toffoli cq_add");
    PASS();
}

static void test_arith_subtraction(void) {
    TEST("arith: subtraction (negative CQ add) widths 1-16");
    ASSERT(arith_cq_add_widths(QC_ARITH_QFT, -5) == 0, "QFT sub");
    ASSERT(arith_cq_add_widths(QC_ARITH_TOFFOLI, -3) == 0, "Toffoli sub");
    PASS();
}

static void test_arith_multiplication(void) {
    TEST("arith: QFT CQ mul (1-8) + Toffoli QQ mul (1-8)");
    /* QFT CQ multiplication */
    for (uint32_t w = 1; w <= 8; w++) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        ASSERT(ctx != NULL, "create");
        qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);
        uint32_t res[16], a[8];
        for (uint32_t i = 0; i < w; i++) { a[i] = i; res[i] = 2 * w + i; }
        qc_error_t err = qc_arith_cq_mul(ctx, res, a, w, 3);
        ASSERT_EQ(err, QC_OK, "qft cq_mul"); ASSERT_GT(qc_circuit_gate_count(ctx), 0, "gc");
        qc_circuit_destroy(ctx);
    }
    /* Toffoli QQ multiplication */
    for (uint32_t w = 1; w <= 8; w++) {
        circuit_ctx_t *ctx = qc_circuit_create(64);
        ASSERT(ctx != NULL, "create");
        qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);
        uint32_t res[16], a[8], b[8];
        for (uint32_t i = 0; i < 2 * w; i++) res[i] = i;
        for (uint32_t i = 0; i < w; i++) { a[i] = 2*w+i; b[i] = 3*w+i; }
        qc_error_t err = qc_toffoli_qq_mul(ctx, res, 2*w, a, w, b, w);
        ASSERT_EQ(err, QC_OK, "tof qq_mul"); ASSERT_GT(qc_circuit_gate_count(ctx), 0, "gc");
        qc_circuit_destroy(ctx);
    }
    PASS();
}

static void test_arith_compare_modes(void) {
    TEST("arith: QFT vs Toffoli gate counts differ (width=4 add)");
    uint32_t a[4] = {0,1,2,3}, b[4] = {4,5,6,7};
    circuit_ctx_t *cq = qc_circuit_create(16);
    circuit_ctx_t *ct = qc_circuit_create(16);
    ASSERT(cq && ct, "create");
    qc_circuit_set_arith_mode(cq, QC_ARITH_QFT);
    qc_circuit_set_arith_mode(ct, QC_ARITH_TOFFOLI);
    qc_arith_qq_add(cq, a, b, 4);
    qc_toffoli_qq_add(ct, a, b, 4);
    uint64_t gq = qc_circuit_gate_count(cq), gt = qc_circuit_gate_count(ct);
    ASSERT_GT(gq, 0, "qft > 0"); ASSERT_GT(gt, 0, "tof > 0");
    ASSERT(gq != gt, "counts differ");
    qc_circuit_destroy(cq); qc_circuit_destroy(ct);
    PASS();
}

/* ====================================================================== */
/* 4. Qubit allocation and reuse                                           */
/* ====================================================================== */

static void test_alloc_basic(void) {
    TEST("alloc: single alloc, block alloc_n, is_allocated");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    uint32_t q;
    ASSERT_EQ(qc_qubit_alloc(ctx, &q), QC_OK, "alloc");
    ASSERT(qc_qubit_is_allocated(ctx, q), "is_alloc");
    uint32_t start;
    ASSERT_EQ(qc_qubit_alloc_n(ctx, 4, &start), QC_OK, "alloc_n");
    for (uint32_t i = 0; i < 4; i++)
        ASSERT(qc_qubit_is_allocated(ctx, start + i), "block alloc");
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_alloc_free_reuse(void) {
    TEST("alloc: free -> re-alloc reuses; double free errors");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    uint32_t q1, q2;
    qc_qubit_alloc(ctx, &q1);
    ASSERT_EQ(qc_qubit_free(ctx, q1), QC_OK, "free");
    ASSERT(!qc_qubit_is_allocated(ctx, q1), "freed");
    ASSERT_EQ(qc_qubit_alloc(ctx, &q2), QC_OK, "re-alloc");
    ASSERT(qc_qubit_is_allocated(ctx, q2), "re-allocated");
    /* double free */
    qc_qubit_alloc(ctx, &q1);
    qc_qubit_free(ctx, q1);
    qc_error_t err = qc_qubit_free(ctx, q1);
    ASSERT(err != QC_OK, "double free errors");
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_alloc_stats(void) {
    TEST("alloc: stats + free_n");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    uint32_t q1, q2, q3;
    qc_qubit_alloc(ctx, &q1); qc_qubit_alloc(ctx, &q2); qc_qubit_alloc(ctx, &q3);
    qc_qubit_free(ctx, q2);
    qc_alloc_stats_t s = qc_circuit_alloc_stats(ctx);
    ASSERT_EQ(s.total_allocations, 3, "3 allocs");
    ASSERT_EQ(s.total_deallocations, 1, "1 dealloc");
    ASSERT_EQ(s.current_in_use, 2, "2 in use");
    ASSERT_GT(s.peak_allocated, 0, "peak > 0");
    /* block free */
    uint32_t blk;
    qc_qubit_alloc_n(ctx, 5, &blk);
    ASSERT_EQ(qc_qubit_free_n(ctx, blk, 5), QC_OK, "free_n");
    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* 5. Optimization passes                                                  */
/* ====================================================================== */

static void test_opt_inverse_cancel(void) {
    TEST("opt: X-X and H-H cancellation, no false cancel");
    /* X-X */
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    qc_circuit_x(ctx, 0); qc_circuit_x(ctx, 0);
    uint64_t bef = qc_circuit_gate_count(ctx);
    circuit_ctx_t *opt = qc_circuit_optimize(ctx);
    if (opt) { ASSERT(qc_circuit_gate_count(opt) < bef, "X-X reduced"); qc_circuit_destroy(opt); }
    qc_circuit_destroy(ctx);
    /* H-H */
    ctx = qc_circuit_create(16);
    qc_circuit_h(ctx, 0); qc_circuit_h(ctx, 0);
    bef = qc_circuit_gate_count(ctx);
    opt = qc_circuit_optimize(ctx);
    if (opt) { ASSERT(qc_circuit_gate_count(opt) < bef, "H-H reduced"); qc_circuit_destroy(opt); }
    qc_circuit_destroy(ctx);
    /* X-H should NOT cancel */
    ctx = qc_circuit_create(16);
    qc_circuit_x(ctx, 0); qc_circuit_h(ctx, 0);
    bef = qc_circuit_gate_count(ctx);
    opt = qc_circuit_optimize(ctx);
    if (opt) { ASSERT_EQ(qc_circuit_gate_count(opt), bef, "X-H kept"); qc_circuit_destroy(opt); }
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_opt_specific_pass(void) {
    TEST("opt: specific CANCEL_INVERSE pass + can_optimize");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    qc_circuit_h(ctx, 0); qc_circuit_h(ctx, 0); qc_circuit_x(ctx, 1);
    circuit_ctx_t *opt = qc_circuit_optimize_pass(ctx, QC_OPT_CANCEL_INVERSE);
    if (opt) {
        ASSERT(qc_circuit_gate_count(opt) < qc_circuit_gate_count(ctx), "reduced");
        qc_circuit_destroy(opt);
    }
    (void)qc_circuit_can_optimize(ctx); /* should not crash */
    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* 6. QASM output                                                          */
/* ====================================================================== */

static void test_qasm_output(void) {
    TEST("output: QASM contains expected gate names");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    qc_circuit_x(ctx, 0); qc_circuit_h(ctx, 1); qc_circuit_cx(ctx, 0, 2);
    qc_circuit_p(ctx, 3, M_PI / 4);
    char *q = qc_circuit_to_qasm(ctx);
    ASSERT(q != NULL, "qasm not NULL");
    ASSERT(strstr(q, "x") != NULL, "has x");
    ASSERT(strstr(q, "h") != NULL, "has h");
    ASSERT(strstr(q, "cx") != NULL, "has cx");
    free(q);
    qc_circuit_destroy(ctx);
    /* empty circuit */
    ctx = qc_circuit_create(16);
    q = qc_circuit_to_qasm(ctx);
    if (q) free(q);
    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* 7. Full pipeline                                                        */
/* ====================================================================== */

static void test_pipeline_qft(void) {
    TEST("pipeline: alloc -> QFT add -> optimize -> QASM -> destroy");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);
    ASSERT_EQ(qc_circuit_get_arith_mode(ctx), QC_ARITH_QFT, "mode");
    uint32_t as, bs;
    qc_qubit_alloc_n(ctx, 4, &as); qc_qubit_alloc_n(ctx, 4, &bs);
    uint32_t a[4], b[4];
    for (int i = 0; i < 4; i++) { a[i] = as+(uint32_t)i; b[i] = bs+(uint32_t)i; }
    ASSERT_EQ(qc_arith_qq_add(ctx, a, b, 4), QC_OK, "add");
    uint64_t gc = qc_circuit_gate_count(ctx);
    ASSERT_GT(gc, 0, "gc > 0");
    circuit_ctx_t *opt = qc_circuit_optimize(ctx);
    if (opt) {
        ASSERT(qc_circuit_gate_count(opt) <= gc, "opt <= orig");
        char *q = qc_circuit_to_qasm(opt);
        ASSERT(q && strlen(q) > 0, "qasm");
        free(q);
        qc_circuit_destroy(opt);
    }
    qc_qubit_free_n(ctx, as, 4); qc_qubit_free_n(ctx, bs, 4);
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_pipeline_toffoli(void) {
    TEST("pipeline: alloc -> Toffoli add -> QASM -> destroy");
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);
    uint32_t as, bs;
    qc_qubit_alloc_n(ctx, 4, &as); qc_qubit_alloc_n(ctx, 4, &bs);
    uint32_t a[4], b[4];
    for (int i = 0; i < 4; i++) { a[i] = as+(uint32_t)i; b[i] = bs+(uint32_t)i; }
    ASSERT_EQ(qc_toffoli_qq_add(ctx, a, b, 4), QC_OK, "add");
    ASSERT_GT(qc_circuit_gate_count(ctx), 0, "gc");
    char *q = qc_circuit_to_qasm(ctx);
    ASSERT(q && strlen(q) > 0, "qasm");
    free(q);
    qc_qubit_free_n(ctx, as, 4); qc_qubit_free_n(ctx, bs, 4);
    qc_circuit_destroy(ctx);
    PASS();
}

static void test_pipeline_mixed_and_misc(void) {
    TEST("pipeline: mixed + reset + version + config + multi-ctx");
    /* Mixed gates + arithmetic */
    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "create");
    qc_circuit_h(ctx, 0); qc_circuit_x(ctx, 1); qc_circuit_cx(ctx, 0, 2);
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);
    uint32_t a3[3] = {3,4,5}, b3[3] = {6,7,8};
    qc_arith_qq_add(ctx, a3, b3, 3);
    ASSERT_GT(qc_circuit_gate_count(ctx), 3, "mixed gc");
    /* Reset + reuse */
    qc_circuit_reset(ctx);
    ASSERT_EQ(qc_circuit_gate_count(ctx), 0, "reset");
    qc_circuit_cx(ctx, 0, 1);
    ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "reuse");
    qc_circuit_destroy(ctx);
    /* Version */
    ASSERT(qc_version_string() != NULL, "ver str");
    ASSERT_GT(qc_version_number(), 0, "ver num");
    /* Config roundtrip */
    ctx = qc_circuit_create(16);
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);
    ASSERT_EQ(qc_circuit_get_arith_mode(ctx), QC_ARITH_QFT, "qft");
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);
    ASSERT_EQ(qc_circuit_get_arith_mode(ctx), QC_ARITH_TOFFOLI, "tof");
    qc_circuit_set_toffoli_decompose(ctx, true);
    qc_circuit_set_qubit_saving(ctx, true);
    qc_circuit_set_simulate(ctx, true);
    qc_circuit_x(ctx, 0);
    ASSERT_EQ(qc_circuit_gate_count(ctx), 1, "after cfg");
    qc_circuit_destroy(ctx);
    /* Multiple independent contexts */
    circuit_ctx_t *c1 = qc_circuit_create(16), *c2 = qc_circuit_create(16);
    ASSERT(c1 && c2, "create2");
    qc_circuit_x(c1, 0);
    qc_circuit_h(c2, 0); qc_circuit_cx(c2, 0, 1);
    ASSERT_EQ(qc_circuit_gate_count(c1), 1, "c1=1");
    ASSERT_EQ(qc_circuit_gate_count(c2), 2, "c2=2");
    qc_circuit_destroy(c1); qc_circuit_destroy(c2);
    PASS();
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    printf("=== Integration Test Suite (Module 1.14) ===\n\n");

    printf("[1] Circuit lifecycle\n");
    test_lifecycle();

    printf("\n[2] Gate emission and retrieval\n");
    test_single_qubit_gates();
    test_two_qubit_gates();
    test_multi_qubit_gates();
    test_depth_tracking();

    printf("\n[3] Arithmetic (widths 1-16, QFT + Toffoli)\n");
    test_arith_qq_add();
    test_arith_cq_add();
    test_arith_subtraction();
    test_arith_multiplication();
    test_arith_compare_modes();

    printf("\n[4] Qubit allocation and reuse\n");
    test_alloc_basic();
    test_alloc_free_reuse();
    test_alloc_stats();

    printf("\n[5] Optimization passes\n");
    test_opt_inverse_cancel();
    test_opt_specific_pass();

    printf("\n[6] QASM output\n");
    test_qasm_output();

    printf("\n[7] Full pipeline\n");
    test_pipeline_qft();
    test_pipeline_toffoli();
    test_pipeline_mixed_and_misc();

    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           g_pass, g_run, g_fail);
    return g_fail > 0 ? 1 : 0;
}
