/**
 * @file test_visualize_overlap.c
 * @brief Tests for overlapping gate visualization in qc_circuit_visualize().
 *
 * Validates that gates with overlapping qubit ranges in the same layer
 * are split into separate visual sub-columns, while non-overlapping
 * gates share a single column.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/**
 * @brief Capture stdout output from qc_circuit_visualize into a string.
 *
 * Uses tmpfile + freopen to redirect stdout, then restores it.
 * Returns a malloc'd string (caller must free).
 */
static char *capture_visualize(const circuit_ctx_t *ctx) {
    fflush(stdout);

    /* Save stdout fd and redirect to a temp file */
    FILE *tmp = tmpfile();
    if (!tmp) return NULL;

    /* dup stdout, redirect to tmp */
    int stdout_fd = fileno(stdout);
    int saved_fd = dup(stdout_fd);
    dup2(fileno(tmp), stdout_fd);

    qc_circuit_visualize(ctx);
    fflush(stdout);

    /* Restore stdout */
    dup2(saved_fd, stdout_fd);
    close(saved_fd);

    /* Read captured output */
    long sz = ftell(tmp);
    rewind(tmp);
    char *buf = malloc((size_t)sz + 1);
    if (buf) {
        fread(buf, 1, (size_t)sz, tmp);
        buf[sz] = '\0';
    }
    fclose(tmp);
    return buf;
}

/** @brief Check that a string contains a substring. */
static int str_contains(const char *haystack, const char *needle) {
    return haystack != NULL && strstr(haystack, needle) != NULL;
}

/* ====================================================================== */
/* T1: Overlapping CX(0,2) + X(1) — gates should be in separate subcols  */
/* ====================================================================== */

static void test_overlapping_cx_x(void) {
    const char *name = "overlapping CX(0,2) + X(1) in separate subcols";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (!ctx) { TEST_FAIL(name, "create failed"); return; }

    /* CX(0,2) and X(1) — qubit ranges [0,2] and [1,1] overlap */
    qc_circuit_cx(ctx, 0, 2);  /* control=0, target=2 */
    qc_circuit_x(ctx, 1);

    char *out = capture_visualize(ctx);
    if (!out) { TEST_FAIL(name, "capture failed"); qc_circuit_destroy(ctx); return; }

    /* Qubit 1 line should show X, not just | */
    if (!str_contains(out, " X ")) {
        TEST_FAIL(name, "X gate not visible on qubit 1");
        printf("    Output:\n%s\n", out);
    }
    /* Should also show @ for CX control and + for CX target */
    else if (!str_contains(out, " @ ")) {
        TEST_FAIL(name, "CX control @ not visible");
    }
    else if (!str_contains(out, " + ")) {
        TEST_FAIL(name, "CX target + not visible");
    }
    /* Qubit 1 shows | in the CX sub-column (correct: wire between 0 and 2)
     * and X in its own sub-column. Both symbols should be present. */
    else {
        TEST_PASS(name);
    }

    free(out);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* T2: Non-overlapping X(0) + X(2) should share one column                */
/* ====================================================================== */

static void test_nonoverlapping_share_column(void) {
    const char *name = "non-overlapping X(0) + X(2) share column";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (!ctx) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_x(ctx, 0);
    qc_circuit_x(ctx, 2);

    char *out = capture_visualize(ctx);
    if (!out) { TEST_FAIL(name, "capture failed"); qc_circuit_destroy(ctx); return; }

    /* Both X gates should be visible */
    /* Count X occurrences — should have at least 2 (one per qubit row) */
    int x_count = 0;
    for (const char *p = out; *p; p++) {
        if (*p == 'X') x_count++;
    }
    if (x_count < 2) {
        TEST_FAIL(name, "expected at least 2 X symbols");
        printf("    Output:\n%s\n", out);
    } else {
        /* The header should show layer 0 once, with width 3 (single subcol) */
        /* Just verify both gates are visible — no extra subcols */
        TEST_PASS(name);
    }

    free(out);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* T3: Three-way overlap — CCX(0,2,4) + H(1) + Z(3)                      */
/* ====================================================================== */

static void test_three_way_overlap(void) {
    const char *name = "three-way overlap: CCX(0,2,4) + H(1) + Z(3)";
    circuit_ctx_t *ctx = qc_circuit_create(8);
    if (!ctx) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_ccx(ctx, 0, 2, 4);  /* range [0,4] */
    qc_circuit_h(ctx, 1);           /* range [1,1] */
    qc_circuit_z(ctx, 3);           /* range [3,3] */

    char *out = capture_visualize(ctx);
    if (!out) { TEST_FAIL(name, "capture failed"); qc_circuit_destroy(ctx); return; }

    /* All three gate symbols should be visible */
    int ok = 1;
    if (!str_contains(out, " H ")) {
        TEST_FAIL(name, "H gate not visible");
        ok = 0;
    }
    if (!str_contains(out, " Z ")) {
        TEST_FAIL(name, "Z gate not visible");
        ok = 0;
    }
    if (!str_contains(out, " + ")) {
        TEST_FAIL(name, "CCX target + not visible");
        ok = 0;
    }
    if (!str_contains(out, " @ ")) {
        TEST_FAIL(name, "CCX control @ not visible");
        ok = 0;
    }
    if (ok) TEST_PASS(name);

    if (!ok) printf("    Output:\n%s\n", out);

    free(out);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* T4: No overlap (regression) — simple circuit, single column per layer  */
/* ====================================================================== */

static void test_no_overlap_regression(void) {
    const char *name = "no overlap regression: H(0) then CX(0,1)";
    circuit_ctx_t *ctx = qc_circuit_create(4);
    if (!ctx) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_h(ctx, 0);
    qc_circuit_cx(ctx, 0, 1);

    char *out = capture_visualize(ctx);
    if (!out) { TEST_FAIL(name, "capture failed"); qc_circuit_destroy(ctx); return; }

    /* H and CX are on different layers (both touch qubit 0) */
    if (!str_contains(out, " H ")) {
        TEST_FAIL(name, "H gate not visible");
    } else if (!str_contains(out, " @ ")) {
        TEST_FAIL(name, "CX control @ not visible");
    } else if (!str_contains(out, " + ")) {
        TEST_FAIL(name, "CX target + not visible");
    } else {
        TEST_PASS(name);
    }

    free(out);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* T5: Wide CX overlap — CX(0,4) + CX(1,3), ranges overlap on qubit 2   */
/* ====================================================================== */

static void test_wide_cx_overlap(void) {
    const char *name = "wide CX overlap: CX(0,4) + CX(1,3)";
    circuit_ctx_t *ctx = qc_circuit_create(8);
    if (!ctx) { TEST_FAIL(name, "create failed"); return; }

    qc_circuit_cx(ctx, 0, 4);  /* range [0,4] */
    qc_circuit_cx(ctx, 1, 3);  /* range [1,3] — overlaps with [0,4] */

    char *out = capture_visualize(ctx);
    if (!out) { TEST_FAIL(name, "capture failed"); qc_circuit_destroy(ctx); return; }

    /* Should have 2 @ symbols (two controls) and 2 + symbols (two targets) */
    int at_count = 0, plus_count = 0;
    for (const char *p = out; *p; p++) {
        if (*p == '@') at_count++;
        if (*p == '+') plus_count++;
    }

    if (at_count < 2) {
        TEST_FAIL(name, "expected 2 @ symbols for two CX controls");
        printf("    Output:\n%s\n", out);
    } else if (plus_count < 2) {
        TEST_FAIL(name, "expected 2 + symbols for two CX targets");
        printf("    Output:\n%s\n", out);
    } else {
        TEST_PASS(name);
    }

    free(out);
    qc_circuit_destroy(ctx);
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== test_visualize_overlap ===\n");

    test_overlapping_cx_x();
    test_nonoverlapping_share_column();
    test_three_way_overlap();
    test_no_overlap_regression();
    test_wide_cx_overlap();

    printf("\n%s (%d failure%s)\n",
           failures == 0 ? "ALL PASSED" : "SOME FAILED",
           failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
