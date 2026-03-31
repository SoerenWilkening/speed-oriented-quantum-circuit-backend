/**
 * @file integer.c
 * @brief Integer utility functions refactored for the micro-package backend.
 *
 * Refactored from: Quantum_Assembly/c_backend/src/Integer.c
 * Module: 1.12 (Phase 1)
 * Issue: refactor-ovp
 *
 * Provides helper functions for two's complement conversion used by
 * arithmetic and comparison operations across the library. These are
 * pure utility functions that do not depend on circuit_ctx_t.
 *
 * Thread safety: All functions are stateless and thread-safe.
 */

#include "internal.h"

#include <stdint.h>

/* ====================================================================== */
/* Two's complement binary representation                                  */
/* ====================================================================== */

/**
 * @brief Compute n-bit two's complement binary representation of x.
 *
 * Returns an array of n ints, each 0 or 1, MSB first.
 * Caller must free the returned array.
 *
 * @param x  Value to convert.
 * @param n  Number of bits (1-64).
 * @return Heap-allocated array of n ints, or NULL on failure.
 */
int *qc_two_complement(int64_t x, int n) {
    if (n < 1 || n > 64) {
        return NULL;
    }

    uint64_t mask = 1ULL << (n - 1);
    int *bin = calloc((size_t)n, sizeof(int));
    if (bin == NULL) {
        return NULL;
    }

    for (int i = 0; i < n; ++i) {
        bin[i] = ((uint64_t)x & mask) ? 1 : 0;
        mask >>= 1;
    }
    return bin;
}
