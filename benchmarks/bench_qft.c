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

    printf("%4u qubits | %10llu gates | depth %6u | gen %10.3f ms | full %10.3f ms | insert %10.3f ms\n",
           n, (unsigned long long)gates, depth, ms_gen, ms_full, ms_full - ms_gen);

    qc_circuit_destroy(ctx_full);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("QFT Circuit Generation Benchmark (libquantum)\n");
    printf("==============================================\n");
    printf("  gen    = gate sequence computation only (count-only mode)\n");
    printf("  full   = compute + insert into circuit data structure\n");
    printf("  insert = full - gen (time spent on circuit insertion)\n\n");

    uint32_t sizes[] = {
        10, 25, 50, 100, 200, 300, 400, 500,
        750, 1000, 1250, 1500, 1750, 2000
    };
    int n_sizes = sizeof(sizes) / sizeof(sizes[0]);

    uint32_t max_n = 2000;
    if (argc > 1) max_n = (uint32_t)atoi(argv[1]);

    for (int i = 0; i < n_sizes; i++) {
        if (sizes[i] > max_n) break;
        if (bench_qft(sizes[i]) != 0) {
            fprintf(stderr, "Benchmark failed at %u qubits\n", sizes[i]);
            return 1;
        }
    }

    return 0;
}
