/**
 * @file test_output_stats.c
 * @brief Tests for circuit_output.c and circuit_stats.c (Module 1.7).
 *
 * Validates QASM export, visualization, and statistics collection
 * through the public API (circuit_ctx_t*).
 *
 * All tests create small circuits (well under 17 qubits).
 */

#include <assert.h>
#include <inttypes.h>
#define _USE_MATH_DEFINES
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include the public header only — tests should not access internals */
#include "quantum_circuit.h"

/* ====================================================================== */
/* Helpers                                                                 */
/* ====================================================================== */

#define TEST_PASS(name) printf("  PASS: %s\n", (name))
#define TEST_FAIL(name, msg) do { \
    printf("  FAIL: %s — %s\n", (name), (msg)); \
    failures++; \
} while (0)

static int failures = 0;

/** @brief Check that a string contains a substring. */
static bool str_contains(const char *haystack, const char *needle) {
    return haystack != NULL && strstr(haystack, needle) != NULL;
}

/* ====================================================================== */
/* Test: empty circuit QASM                                                */
/* ====================================================================== */

static void test_empty_circuit_qasm(void) {
    const char *name = "empty circuit QASM";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    char *qasm = qc_circuit_to_qasm(ctx);
    if (qasm == NULL) {
        TEST_FAIL(name, "to_qasm returned NULL");
        qc_circuit_destroy(ctx);
        return;
    }

    if (!str_contains(qasm, "OPENQASM 3.0;")) {
        TEST_FAIL(name, "missing OPENQASM header");
    } else if (!str_contains(qasm, "include \"stdgates.inc\";")) {
        TEST_FAIL(name, "missing stdgates include");
    } else if (!str_contains(qasm, "qubit[")) {
        TEST_FAIL(name, "missing qubit declaration");
    } else {
        TEST_PASS(name);
    }

    free(qasm);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: single-qubit gates in QASM                                        */
/* ====================================================================== */

static void test_single_qubit_qasm(void) {
    const char *name = "single-qubit gates QASM";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_x(ctx, 0);
    qc_circuit_h(ctx, 1);
    qc_circuit_z(ctx, 2);
    qc_circuit_y(ctx, 3);
    qc_circuit_t_gate(ctx, 0);
    qc_circuit_tdg(ctx, 1);

    char *qasm = qc_circuit_to_qasm(ctx);
    if (qasm == NULL) {
        TEST_FAIL(name, "to_qasm returned NULL");
        qc_circuit_destroy(ctx);
        return;
    }

    bool ok = true;
    if (!str_contains(qasm, "x q[0];"))   { ok = false; TEST_FAIL(name, "missing x q[0]"); }
    if (!str_contains(qasm, "h q[1];"))   { ok = false; TEST_FAIL(name, "missing h q[1]"); }
    if (!str_contains(qasm, "z q[2];"))   { ok = false; TEST_FAIL(name, "missing z q[2]"); }
    if (!str_contains(qasm, "y q[3];"))   { ok = false; TEST_FAIL(name, "missing y q[3]"); }
    if (!str_contains(qasm, "t q[0];"))   { ok = false; TEST_FAIL(name, "missing t q[0]"); }
    if (!str_contains(qasm, "tdg q[1];")) { ok = false; TEST_FAIL(name, "missing tdg q[1]"); }
    if (ok) TEST_PASS(name);

    free(qasm);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: controlled gates in QASM                                          */
/* ====================================================================== */

static void test_controlled_qasm(void) {
    const char *name = "controlled gates QASM";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_cx(ctx, 0, 1);
    qc_circuit_cz(ctx, 1, 2);
    qc_circuit_ccx(ctx, 0, 1, 2);

    char *qasm = qc_circuit_to_qasm(ctx);
    if (qasm == NULL) {
        TEST_FAIL(name, "to_qasm returned NULL");
        qc_circuit_destroy(ctx);
        return;
    }

    bool ok = true;
    if (!str_contains(qasm, "cx q[0], q[1];"))         { ok = false; TEST_FAIL(name, "missing cx"); }
    if (!str_contains(qasm, "cz q[1], q[2];"))         { ok = false; TEST_FAIL(name, "missing cz"); }
    if (!str_contains(qasm, "ccx q[0], q[1], q[2];"))  { ok = false; TEST_FAIL(name, "missing ccx"); }
    if (ok) TEST_PASS(name);

    free(qasm);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: phase gate angle in QASM                                          */
/* ====================================================================== */

static void test_phase_gate_qasm(void) {
    const char *name = "phase gate angle QASM";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_p(ctx, 0, M_PI / 4.0);

    char *qasm = qc_circuit_to_qasm(ctx);
    if (qasm == NULL) {
        TEST_FAIL(name, "to_qasm returned NULL");
        qc_circuit_destroy(ctx);
        return;
    }

    if (!str_contains(qasm, "p(")) {
        TEST_FAIL(name, "missing p( in output");
    } else if (!str_contains(qasm, "q[0];")) {
        TEST_FAIL(name, "missing q[0] target");
    } else {
        TEST_PASS(name);
    }

    free(qasm);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: QASM file export                                                  */
/* ====================================================================== */

static void test_qasm_file_export(void) {
    const char *name = "QASM file export";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_x(ctx, 0);
    qc_circuit_h(ctx, 1);

    const char *path = "/tmp/test_output_stats_qasm.qasm";
    qc_error_t err = qc_circuit_to_qasm_file(ctx, path);
    if (err != QC_OK) {
        TEST_FAIL(name, "to_qasm_file returned error");
        qc_circuit_destroy(ctx);
        return;
    }

    /* Verify file exists and contains QASM */
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        TEST_FAIL(name, "output file not found");
        qc_circuit_destroy(ctx);
        return;
    }

    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    if (!str_contains(buf, "OPENQASM 3.0;")) {
        TEST_FAIL(name, "file missing OPENQASM header");
    } else {
        TEST_PASS(name);
    }

    /* Clean up */
    remove(path);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: NULL safety                                                       */
/* ====================================================================== */

static void test_null_safety(void) {
    const char *name = "NULL safety";

    /* All functions should handle NULL gracefully */
    char *qasm = qc_circuit_to_qasm(NULL);
    if (qasm != NULL) { TEST_FAIL(name, "to_qasm(NULL) should return NULL"); free(qasm); return; }

    qc_error_t err = qc_circuit_to_qasm_file(NULL, "/tmp/x.qasm");
    if (err != QC_ERR_NULL) { TEST_FAIL(name, "to_qasm_file(NULL) wrong error"); return; }

    err = qc_circuit_to_qasm_file(NULL, NULL);
    if (err != QC_ERR_NULL) { TEST_FAIL(name, "to_qasm_file(NULL,NULL) wrong error"); return; }

    uint64_t gc = qc_circuit_gate_count(NULL);
    if (gc != 0) { TEST_FAIL(name, "gate_count(NULL) should be 0"); return; }

    uint32_t d = qc_circuit_depth(NULL);
    if (d != 0) { TEST_FAIL(name, "depth(NULL) should be 0"); return; }

    uint32_t w = qc_circuit_width(NULL);
    if (w != 0) { TEST_FAIL(name, "width(NULL) should be 0"); return; }

    qc_gate_counts_t counts = qc_circuit_gate_counts(NULL);
    if (counts.x_gates != 0) { TEST_FAIL(name, "gate_counts(NULL) not zeroed"); return; }

    qc_alloc_stats_t as = qc_circuit_alloc_stats(NULL);
    if (as.peak_allocated != 0) { TEST_FAIL(name, "alloc_stats(NULL) not zeroed"); return; }

    /* Visualize and print_stats on NULL should not crash */
    qc_circuit_visualize(NULL);
    qc_circuit_print_stats(NULL);

    TEST_PASS(name);
}

/* ====================================================================== */
/* Test: gate count statistics                                             */
/* ====================================================================== */

static void test_gate_count_stats(void) {
    const char *name = "gate count statistics";
    circuit_ctx_t *ctx = qc_circuit_create(8);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    /* Add known gate mix */
    qc_circuit_x(ctx, 0);
    qc_circuit_x(ctx, 1);
    qc_circuit_h(ctx, 2);
    qc_circuit_h(ctx, 3);
    qc_circuit_h(ctx, 4);
    qc_circuit_cx(ctx, 0, 1);
    qc_circuit_cx(ctx, 2, 3);
    qc_circuit_ccx(ctx, 0, 1, 2);
    qc_circuit_t_gate(ctx, 0);
    qc_circuit_tdg(ctx, 1);
    qc_circuit_p(ctx, 2, 1.0);

    qc_gate_counts_t counts = qc_circuit_gate_counts(ctx);

    bool ok = true;
    if (counts.x_gates != 2)   { ok = false; printf("    x_gates=%" PRIu64 " (expected 2)\n", counts.x_gates); }
    if (counts.h_gates != 3)   { ok = false; printf("    h_gates=%" PRIu64 " (expected 3)\n", counts.h_gates); }
    if (counts.cx_gates != 2)  { ok = false; printf("    cx_gates=%" PRIu64 " (expected 2)\n", counts.cx_gates); }
    if (counts.ccx_gates != 1) { ok = false; printf("    ccx_gates=%" PRIu64 " (expected 1)\n", counts.ccx_gates); }
    if (counts.t_gates != 1)   { ok = false; printf("    t_gates=%" PRIu64 " (expected 1)\n", counts.t_gates); }
    if (counts.tdg_gates != 1) { ok = false; printf("    tdg_gates=%" PRIu64 " (expected 1)\n", counts.tdg_gates); }
    if (counts.p_gates != 1)   { ok = false; printf("    p_gates=%" PRIu64 " (expected 1)\n", counts.p_gates); }
    /* T-count: actual T+Tdg = 2 (since T/Tdg present) */
    if (counts.t_count != 2)   { ok = false; printf("    t_count=%" PRIu64 " (expected 2)\n", counts.t_count); }

    if (ok) TEST_PASS(name); else TEST_FAIL(name, "counts mismatch (see above)");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: T-count estimation (no actual T gates)                            */
/* ====================================================================== */

static void test_t_count_estimation(void) {
    const char *name = "T-count estimation from CCX";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    /* Only CCX gates, no explicit T/Tdg */
    qc_circuit_ccx(ctx, 0, 1, 2);
    qc_circuit_ccx(ctx, 0, 1, 3);

    qc_gate_counts_t counts = qc_circuit_gate_counts(ctx);

    if (counts.ccx_gates != 2) {
        TEST_FAIL(name, "expected 2 CCX gates");
    } else if (counts.t_count != 14) {
        /* 2 CCX * 7 T per CCX = 14 */
        printf("    t_count=%" PRIu64 " (expected 14)\n", counts.t_count);
        TEST_FAIL(name, "wrong T-count estimate");
    } else {
        TEST_PASS(name);
    }

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: scalar statistics (depth, width, gate_count)                      */
/* ====================================================================== */

static void test_scalar_stats(void) {
    const char *name = "scalar statistics";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_x(ctx, 0);
    qc_circuit_h(ctx, 1);
    qc_circuit_cx(ctx, 0, 1);

    uint64_t gc = qc_circuit_gate_count(ctx);
    uint32_t d  = qc_circuit_depth(ctx);
    uint32_t w  = qc_circuit_width(ctx);

    bool ok = true;
    if (gc != 3) { ok = false; printf("    gate_count=%" PRIu64 " (expected 3)\n", gc); }
    if (w < 2)   { ok = false; printf("    width=%u (expected >= 2)\n", w); }
    if (d == 0)  { ok = false; printf("    depth=%u (expected > 0)\n", d); }

    if (ok) TEST_PASS(name); else TEST_FAIL(name, "scalar mismatch (see above)");

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: gate counts range                                                 */
/* ====================================================================== */

static void test_gate_counts_range(void) {
    const char *name = "gate counts range";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    /* Add gates — they will be placed in layers automatically */
    qc_circuit_x(ctx, 0);
    qc_circuit_x(ctx, 0);  /* Must go to a later layer (same qubit) */

    uint32_t depth = qc_circuit_depth(ctx);

    /* Full range should match full counts */
    qc_gate_counts_t full = qc_circuit_gate_counts(ctx);
    qc_gate_counts_t range = qc_circuit_gate_counts_range(ctx, 0, depth);

    if (full.x_gates != range.x_gates) {
        TEST_FAIL(name, "full vs range mismatch");
    } else {
        /* Empty range should return zeros */
        qc_gate_counts_t empty = qc_circuit_gate_counts_range(ctx, 0, 0);
        if (empty.x_gates != 0) {
            TEST_FAIL(name, "empty range not zero");
        } else {
            TEST_PASS(name);
        }
    }

    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: visualization does not crash                                       */
/* ====================================================================== */

static void test_visualize_no_crash(void) {
    const char *name = "visualize no crash";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    /* Empty circuit */
    qc_circuit_visualize(ctx);

    /* Circuit with gates */
    qc_circuit_x(ctx, 0);
    qc_circuit_h(ctx, 1);
    qc_circuit_cx(ctx, 0, 1);
    qc_circuit_visualize(ctx);

    TEST_PASS(name);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Test: print_stats does not crash                                        */
/* ====================================================================== */

static void test_print_stats_no_crash(void) {
    const char *name = "print_stats no crash";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (ctx == NULL) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_x(ctx, 0);
    qc_circuit_h(ctx, 1);
    qc_circuit_ccx(ctx, 0, 1, 2);
    qc_circuit_print_stats(ctx);

    TEST_PASS(name);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Module 1.7: Output + Stats Tests ===\n");

    test_empty_circuit_qasm();
    test_single_qubit_qasm();
    test_controlled_qasm();
    test_phase_gate_qasm();
    test_qasm_file_export();
    test_null_safety();
    test_gate_count_stats();
    test_t_count_estimation();
    test_scalar_stats();
    test_gate_counts_range();
    test_visualize_no_crash();
    test_print_stats_no_crash();

    printf("\n");
    if (failures == 0) {
        printf("All tests passed.\n");
    } else {
        printf("%d test(s) FAILED.\n", failures);
    }

    return failures > 0 ? 1 : 0;
}
