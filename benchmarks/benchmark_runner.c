/**
 * @file benchmark_runner.c
 * @brief Benchmark runner for libquantum arithmetic operations.
 *
 * Generates circuits for various arithmetic operations at a given width,
 * then outputs gate count, depth, and qubit statistics as JSON to stdout.
 *
 * Usage: benchmark_runner <operation> <width>
 *
 * Operations: qft_add, toffoli_cdkm, toffoli_cla, qft_mul, toffoli_mul
 */

/* Ensure CLOCK_MONOTONIC is declared on Linux/POSIX systems */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "quantum_circuit.h"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static void print_json_result(const char *operation, uint32_t width,
                              const circuit_ctx_t *ctx, double elapsed_ms) {
    qc_gate_counts_t gc = qc_circuit_gate_counts(ctx);
    uint64_t total = qc_circuit_gate_count(ctx);
    uint32_t depth = qc_circuit_depth(ctx);
    uint32_t circ_width = qc_circuit_width(ctx);
    qc_alloc_stats_t as = qc_circuit_alloc_stats(ctx);

    printf("{\n");
    printf("  \"operation\": \"%s\",\n", operation);
    printf("  \"width\": %u,\n", width);
    printf("  \"total_gates\": %llu,\n", (unsigned long long)total);
    printf("  \"depth\": %u,\n", depth);
    printf("  \"circuit_width\": %u,\n", circ_width);
    printf("  \"peak_qubits\": %u,\n", as.peak_allocated);
    printf("  \"elapsed_ms\": %.3f,\n", elapsed_ms);
    printf("  \"gate_breakdown\": {\n");
    printf("    \"x\": %llu,\n", (unsigned long long)gc.x_gates);
    printf("    \"h\": %llu,\n", (unsigned long long)gc.h_gates);
    printf("    \"p\": %llu,\n", (unsigned long long)gc.p_gates);
    printf("    \"t\": %llu,\n", (unsigned long long)gc.t_gates);
    printf("    \"tdg\": %llu,\n", (unsigned long long)gc.tdg_gates);
    printf("    \"cx\": %llu,\n", (unsigned long long)gc.cx_gates);
    printf("    \"ccx\": %llu,\n", (unsigned long long)gc.ccx_gates);
    printf("    \"other\": %llu\n", (unsigned long long)gc.other_gates);
    printf("  }\n");
    printf("}\n");
}

static double elapsed_ms(struct timespec *start, struct timespec *end) {
    double s = (double)(end->tv_sec - start->tv_sec);
    double ns = (double)(end->tv_nsec - start->tv_nsec);
    return s * 1000.0 + ns / 1e6;
}

/* ── Benchmark functions ─────────────────────────────────────────────── */

static int bench_qft_add(uint32_t width) {
    circuit_ctx_t *ctx = qc_circuit_create(width * 4);
    if (!ctx) return 1;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t a_start, b_start;
    qc_qubit_alloc_n(ctx, width, &a_start);
    qc_qubit_alloc_n(ctx, width, &b_start);

    uint32_t *a = malloc(width * sizeof(uint32_t));
    uint32_t *b = malloc(width * sizeof(uint32_t));
    for (uint32_t i = 0; i < width; i++) {
        a[i] = a_start + i;
        b[i] = b_start + i;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    qc_arith_qq_add(ctx, a, b, width);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    print_json_result("qft_add", width, ctx, elapsed_ms(&t0, &t1));

    free(a);
    free(b);
    qc_circuit_destroy(ctx);
    return 0;
}

static int bench_toffoli_cdkm(uint32_t width) {
    circuit_ctx_t *ctx = qc_circuit_create(width * 4);
    if (!ctx) return 1;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t a_start, b_start;
    qc_qubit_alloc_n(ctx, width, &a_start);
    qc_qubit_alloc_n(ctx, width, &b_start);

    uint32_t *a = malloc(width * sizeof(uint32_t));
    uint32_t *b = malloc(width * sizeof(uint32_t));
    for (uint32_t i = 0; i < width; i++) {
        a[i] = a_start + i;
        b[i] = b_start + i;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    qc_toffoli_qq_add(ctx, a, b, width);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    print_json_result("toffoli_cdkm", width, ctx, elapsed_ms(&t0, &t1));

    free(a);
    free(b);
    qc_circuit_destroy(ctx);
    return 0;
}

static int bench_toffoli_cla(uint32_t width) {
    if (width < 2) {
        fprintf(stderr, "CLA requires width >= 2\n");
        return 1;
    }

    circuit_ctx_t *ctx = qc_circuit_create(width * 6);
    if (!ctx) return 1;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t a_start, b_start;
    qc_qubit_alloc_n(ctx, width, &a_start);
    qc_qubit_alloc_n(ctx, width, &b_start);

    uint32_t *a = malloc(width * sizeof(uint32_t));
    uint32_t *b = malloc(width * sizeof(uint32_t));
    for (uint32_t i = 0; i < width; i++) {
        a[i] = a_start + i;
        b[i] = b_start + i;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    qc_toffoli_qq_add_bk(ctx, a, b, width);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    print_json_result("toffoli_cla", width, ctx, elapsed_ms(&t0, &t1));

    free(a);
    free(b);
    qc_circuit_destroy(ctx);
    return 0;
}

static int bench_qft_mul(uint32_t width) {
    uint32_t result_bits = width * 2;
    circuit_ctx_t *ctx = qc_circuit_create(result_bits * 4);
    if (!ctx) return 1;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_QFT);

    uint32_t a_start, b_start, r_start;
    qc_qubit_alloc_n(ctx, width, &a_start);
    qc_qubit_alloc_n(ctx, width, &b_start);
    qc_qubit_alloc_n(ctx, result_bits, &r_start);

    uint32_t *a = malloc(width * sizeof(uint32_t));
    uint32_t *b = malloc(width * sizeof(uint32_t));
    uint32_t *r = malloc(result_bits * sizeof(uint32_t));
    for (uint32_t i = 0; i < width; i++) {
        a[i] = a_start + i;
        b[i] = b_start + i;
    }
    for (uint32_t i = 0; i < result_bits; i++) {
        r[i] = r_start + i;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    qc_arith_qq_mul(ctx, r, a, b, width);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    print_json_result("qft_mul", width, ctx, elapsed_ms(&t0, &t1));

    free(a);
    free(b);
    free(r);
    qc_circuit_destroy(ctx);
    return 0;
}

static int bench_toffoli_mul(uint32_t width) {
    uint32_t result_bits = width * 2;
    circuit_ctx_t *ctx = qc_circuit_create(result_bits * 4);
    if (!ctx) return 1;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    uint32_t a_start, b_start, r_start;
    qc_qubit_alloc_n(ctx, width, &a_start);
    qc_qubit_alloc_n(ctx, width, &b_start);
    qc_qubit_alloc_n(ctx, result_bits, &r_start);

    uint32_t *a = malloc(width * sizeof(uint32_t));
    uint32_t *b = malloc(width * sizeof(uint32_t));
    uint32_t *r = malloc(result_bits * sizeof(uint32_t));
    for (uint32_t i = 0; i < width; i++) {
        a[i] = a_start + i;
        b[i] = b_start + i;
    }
    for (uint32_t i = 0; i < result_bits; i++) {
        r[i] = r_start + i;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    qc_toffoli_qq_mul(ctx, r, result_bits, a, width, b, width);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    print_json_result("toffoli_mul", width, ctx, elapsed_ms(&t0, &t1));

    free(a);
    free(b);
    free(r);
    qc_circuit_destroy(ctx);
    return 0;
}

/* ── Main ────────────────────────────────────────────────────────────── */

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <operation> <width>\n", prog);
    fprintf(stderr, "Operations: qft_add, toffoli_cdkm, toffoli_cla, "
                    "qft_mul, toffoli_mul\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage(argv[0]);
        return 1;
    }

    const char *op = argv[1];
    int width = atoi(argv[2]);
    if (width < 1 || width > 64) {
        fprintf(stderr, "Width must be 1-64, got %d\n", width);
        return 1;
    }

    if (strcmp(op, "qft_add") == 0)          return bench_qft_add((uint32_t)width);
    if (strcmp(op, "toffoli_cdkm") == 0)     return bench_toffoli_cdkm((uint32_t)width);
    if (strcmp(op, "toffoli_cla") == 0)      return bench_toffoli_cla((uint32_t)width);
    if (strcmp(op, "qft_mul") == 0)          return bench_qft_mul((uint32_t)width);
    if (strcmp(op, "toffoli_mul") == 0)      return bench_toffoli_mul((uint32_t)width);

    fprintf(stderr, "Unknown operation: %s\n", op);
    usage(argv[0]);
    return 1;
}
