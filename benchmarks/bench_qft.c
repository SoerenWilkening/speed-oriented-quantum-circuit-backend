#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "quantum_circuit.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double elapsed_ms(struct timespec *t0, struct timespec *t1) {
    double s  = (double)(t1->tv_sec  - t0->tv_sec);
    double ns = (double)(t1->tv_nsec - t0->tv_nsec);
    return s * 1000.0 + ns / 1e6;
}

/* Peak resident set size in KiB, read from /proc/self/status (Linux). */
static long peak_rss_kib(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long kib = -1;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "VmHWM: %ld kB", &kib) == 1) break;
    }
    fclose(f);
    return kib;
}

/**
 * Build the standard QFT on n qubits:
 *   for i in 0..n-1:
 *     H(i)
 *     for j in i+1..n-1:
 *       CP(j, i, pi / 2^(j-i))
 */
static void build_qft(circuit_ctx_t *ctx, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        qc_circuit_h(ctx, i);
        for (uint32_t j = i + 1; j < n; j++) {
            double angle = M_PI / (double)(1ULL << (j - i));
            qc_circuit_cp(ctx, j, i, angle);
        }
    }
}

static int bench_qft(uint32_t n) {
    struct timespec t0, t1, t2, t3;

    /* --- Phase 1: Generation only (count-only mode) --- */
    circuit_ctx_t *ctx_gen = qc_circuit_create(n);
    if (!ctx_gen) return 1;
    qc_circuit_set_simulate(ctx_gen, false);
    uint32_t q;
    for (uint32_t i = 0; i < n; i++)
        qc_qubit_alloc(ctx_gen, &q);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    build_qft(ctx_gen, n);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    uint64_t gates = qc_circuit_gate_count(ctx_gen);
    qc_circuit_destroy(ctx_gen);

    /* --- Phase 2: Full circuit insertion --- */
    circuit_ctx_t *ctx_full = qc_circuit_create(n);
    if (!ctx_full) return 1;
    qc_circuit_set_simulate(ctx_full, true);
    for (uint32_t i = 0; i < n; i++)
        qc_qubit_alloc(ctx_full, &q);

    clock_gettime(CLOCK_MONOTONIC, &t2);
    build_qft(ctx_full, n);
    clock_gettime(CLOCK_MONOTONIC, &t3);

    uint32_t depth = qc_circuit_depth(ctx_full);
    double ms_gen  = elapsed_ms(&t0, &t1);
    double ms_full = elapsed_ms(&t2, &t3);

    long rss_kib = peak_rss_kib();
    printf("%4u qubits | %10llu gates | depth %6u | gen %14.7f ms | full %14.7f ms | insert %14.7f ms | peakRSS %8.1f MiB\n",
           n, (unsigned long long)gates, depth, ms_gen, ms_full, ms_full - ms_gen,
           rss_kib > 0 ? rss_kib / 1024.0 : 0.0);

    qc_circuit_destroy(ctx_full);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("QFT Circuit Generation Benchmark (libquantum)\n");
    printf("==============================================\n");
    printf("  gen    = gate sequence computation only (count-only mode)\n");
    printf("  full   = compute + insert into circuit data structure\n");
    printf("  insert = full - gen (time spent on circuit insertion)\n\n");

    uint32_t default_sizes[] = {
        10, 25, 50, 100, 200, 300, 400, 500,
        750, 1000, 1250, 1500, 1750, 2000
    };
    uint32_t *sizes = default_sizes;
    int n_sizes = sizeof(default_sizes) / sizeof(default_sizes[0]);
    uint32_t *parsed = NULL;

    if (argc > 2) {
        n_sizes = argc - 1;
        parsed = (uint32_t*)malloc(sizeof(uint32_t) * n_sizes);
        for (int i = 0; i < n_sizes; i++) parsed[i] = (uint32_t)atoi(argv[i + 1]);
        sizes = parsed;
    } else if (argc == 2) {
        uint32_t max_n = (uint32_t)atoi(argv[1]);
        int k = 0;
        for (int i = 0; i < n_sizes; i++) if (default_sizes[i] <= max_n) k++;
        n_sizes = k;
    }

    for (int i = 0; i < n_sizes; i++) {
        if (bench_qft(sizes[i]) != 0) {
            fprintf(stderr, "Benchmark failed at %u qubits\n", sizes[i]);
            free(parsed);
            return 1;
        }
    }
    free(parsed);

    return 0;
}
