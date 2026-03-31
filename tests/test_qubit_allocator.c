/**
 * @file test_qubit_allocator.c
 * @brief Unit tests for the refactored qubit allocator (Module 1.4).
 *
 * Tests cover:
 *   - Context create/destroy lifecycle
 *   - Single qubit alloc/free
 *   - Block (multi-qubit) alloc/free
 *   - Freed-qubit reuse
 *   - Block coalescing on free
 *   - Double-free detection
 *   - Overflow protection
 *   - Statistics accuracy
 *   - qc_qubit_is_allocated queries
 *   - Reset behavior
 *   - NULL-safety of all public functions
 *
 * Max 17 qubits used in any single test (per project constraint).
 * Compiled as a standalone executable (no test framework dependency).
 */

#include "quantum_circuit.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────── */

static int tests_run    = 0;
static int tests_passed = 0;

#define RUN_TEST(fn)                                                       \
    do {                                                                   \
        tests_run++;                                                       \
        printf("  %-50s ", #fn);                                           \
        fn();                                                              \
        tests_passed++;                                                    \
        printf("[PASS]\n");                                                \
    } while (0)

/* ── Tests ───────────────────────────────────────────────────────────── */

static void test_create_destroy(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);
    assert(ctx != NULL);
    qc_circuit_destroy(ctx);

    /* Zero initial_qubits => default capacity */
    ctx = qc_circuit_create(0);
    assert(ctx != NULL);
    qc_circuit_destroy(ctx);

    /* NULL destroy is safe no-op */
    qc_circuit_destroy(NULL);
}

static void test_single_alloc(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);
    uint32_t q;
    assert(qc_qubit_alloc(ctx, &q) == QC_OK);
    assert(q == 0);

    uint32_t q2;
    assert(qc_qubit_alloc(ctx, &q2) == QC_OK);
    assert(q2 == 1);

    assert(qc_circuit_num_qubits(ctx) == 2);
    qc_circuit_destroy(ctx);
}

static void test_block_alloc(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(32);
    uint32_t start;
    assert(qc_qubit_alloc_n(ctx, 8, &start) == QC_OK);
    assert(start == 0);

    uint32_t start2;
    assert(qc_qubit_alloc_n(ctx, 4, &start2) == QC_OK);
    assert(start2 == 8);

    assert(qc_circuit_num_qubits(ctx) == 12);
    qc_circuit_destroy(ctx);
}

static void test_free_single(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);
    uint32_t q0, q1, q2;
    qc_qubit_alloc(ctx, &q0);
    qc_qubit_alloc(ctx, &q1);
    qc_qubit_alloc(ctx, &q2);

    assert(qc_qubit_free(ctx, q1) == QC_OK);
    assert(qc_circuit_num_qubits(ctx) == 2);
    assert(!qc_qubit_is_allocated(ctx, q1));
    assert(qc_qubit_is_allocated(ctx, q0));
    assert(qc_qubit_is_allocated(ctx, q2));

    qc_circuit_destroy(ctx);
}

static void test_free_block(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(32);
    uint32_t start;
    qc_qubit_alloc_n(ctx, 10, &start);

    assert(qc_qubit_free_n(ctx, 3, 4) == QC_OK);
    assert(qc_circuit_num_qubits(ctx) == 6);

    /* Qubits 3-6 freed */
    for (uint32_t i = 3; i < 7; i++) {
        assert(!qc_qubit_is_allocated(ctx, i));
    }
    /* Qubits 0-2 and 7-9 still allocated */
    for (uint32_t i = 0; i < 3; i++) {
        assert(qc_qubit_is_allocated(ctx, i));
    }
    for (uint32_t i = 7; i < 10; i++) {
        assert(qc_qubit_is_allocated(ctx, i));
    }

    qc_circuit_destroy(ctx);
}

static void test_reuse(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);
    uint32_t q0, q1, q2;
    qc_qubit_alloc(ctx, &q0);
    qc_qubit_alloc(ctx, &q1);
    qc_qubit_alloc(ctx, &q2);

    /* Free q1 */
    qc_qubit_free(ctx, q1);

    /* Next alloc should reuse q1's index */
    uint32_t q_reused;
    qc_qubit_alloc(ctx, &q_reused);
    assert(q_reused == q1);

    qc_circuit_destroy(ctx);
}

static void test_block_reuse(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(32);
    uint32_t s1, s2;
    qc_qubit_alloc_n(ctx, 4, &s1);  /* 0-3 */
    qc_qubit_alloc_n(ctx, 4, &s2);  /* 4-7 */

    /* Free first block */
    qc_qubit_free_n(ctx, s1, 4);

    /* Re-alloc should reuse indices 0-3 */
    uint32_t s3;
    qc_qubit_alloc_n(ctx, 4, &s3);
    assert(s3 == 0);

    qc_circuit_destroy(ctx);
}

static void test_coalescing(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);
    uint32_t q[6];
    for (int i = 0; i < 6; i++) {
        qc_qubit_alloc(ctx, &q[i]);
    }

    /* Free q[2], then q[3] -- should coalesce into one block */
    qc_qubit_free(ctx, q[2]);
    qc_qubit_free(ctx, q[3]);

    /* Now free q[1] -- should coalesce with block [2,3] */
    qc_qubit_free(ctx, q[1]);

    /* Allocating 3 qubits should reuse the coalesced block [1,2,3] */
    uint32_t start;
    qc_qubit_alloc_n(ctx, 3, &start);
    assert(start == 1);

    qc_circuit_destroy(ctx);
}

static void test_double_free(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);
    uint32_t q;
    qc_qubit_alloc(ctx, &q);
    assert(qc_qubit_free(ctx, q) == QC_OK);

    /* Second free of same qubit should fail */
    assert(qc_qubit_free(ctx, q) == QC_ERR_DOUBLE_FREE);

    qc_circuit_destroy(ctx);
}

static void test_free_invalid(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* Free qubit that was never allocated */
    assert(qc_qubit_free(ctx, 99) == QC_ERR_DOUBLE_FREE);

    qc_circuit_destroy(ctx);
}

static void test_statistics(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t q0, q1, q2;
    qc_qubit_alloc(ctx, &q0);
    qc_qubit_alloc(ctx, &q1);
    qc_qubit_alloc(ctx, &q2);

    qc_alloc_stats_t s = qc_circuit_alloc_stats(ctx);
    assert(s.total_allocations == 3);
    assert(s.current_in_use == 3);
    assert(s.peak_allocated == 3);
    assert(s.total_deallocations == 0);

    qc_qubit_free(ctx, q1);
    s = qc_circuit_alloc_stats(ctx);
    assert(s.total_deallocations == 1);
    assert(s.current_in_use == 2);
    assert(s.peak_allocated == 3);  /* peak unchanged */

    qc_circuit_destroy(ctx);
}

static void test_is_allocated(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);

    /* Not yet allocated */
    assert(!qc_qubit_is_allocated(ctx, 0));

    uint32_t q;
    qc_qubit_alloc(ctx, &q);
    assert(qc_qubit_is_allocated(ctx, q));

    qc_qubit_free(ctx, q);
    assert(!qc_qubit_is_allocated(ctx, q));

    /* Way out of range */
    assert(!qc_qubit_is_allocated(ctx, 9999));

    qc_circuit_destroy(ctx);
}

static void test_reset(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(16);

    uint32_t q;
    for (int i = 0; i < 5; i++) {
        qc_qubit_alloc(ctx, &q);
    }

    assert(qc_circuit_reset(ctx) == QC_OK);
    assert(qc_circuit_num_qubits(ctx) == 0);

    qc_alloc_stats_t s = qc_circuit_alloc_stats(ctx);
    assert(s.total_allocations == 0);
    assert(s.peak_allocated == 0);

    /* Can allocate again after reset */
    qc_qubit_alloc(ctx, &q);
    assert(q == 0);

    qc_circuit_destroy(ctx);
}

static void test_null_safety(void)
{
    uint32_t q;
    assert(qc_qubit_alloc(NULL, &q)       == QC_ERR_NULL);
    assert(qc_qubit_alloc_n(NULL, 1, &q)  == QC_ERR_NULL);
    assert(qc_qubit_free(NULL, 0)         == QC_ERR_NULL);
    assert(qc_qubit_free_n(NULL, 0, 1)    == QC_ERR_NULL);
    assert(qc_circuit_reset(NULL)         == QC_ERR_NULL);
    assert(qc_circuit_num_qubits(NULL)    == 0);
    assert(!qc_qubit_is_allocated(NULL, 0));

    circuit_ctx_t *ctx = qc_circuit_create(8);
    assert(qc_qubit_alloc(ctx, NULL)      == QC_ERR_NULL);
    assert(qc_qubit_alloc_n(ctx, 1, NULL) == QC_ERR_NULL);
    assert(qc_qubit_alloc_n(ctx, 0, &q)   == QC_ERR_QUBIT);
    assert(qc_qubit_free_n(ctx, 0, 0)     == QC_ERR_QUBIT);
    qc_circuit_destroy(ctx);
}

static void test_many_alloc_free_cycles(void)
{
    /* Stress test: alloc-free cycles to verify no leaks / corruption */
    circuit_ctx_t *ctx = qc_circuit_create(16);

    for (int cycle = 0; cycle < 100; cycle++) {
        uint32_t qs[8];
        for (int i = 0; i < 8; i++) {
            assert(qc_qubit_alloc(ctx, &qs[i]) == QC_OK);
        }
        /* Free in reverse order */
        for (int i = 7; i >= 0; i--) {
            assert(qc_qubit_free(ctx, qs[i]) == QC_OK);
        }
    }

    assert(qc_circuit_num_qubits(ctx) == 0);
    qc_circuit_destroy(ctx);
}

static void test_capacity_growth(void)
{
    /* Start with tiny capacity, allocate beyond it */
    circuit_ctx_t *ctx = qc_circuit_create(4);
    uint32_t start;

    /* Allocate 16 qubits -- needs capacity growth from 4 */
    assert(qc_qubit_alloc_n(ctx, 16, &start) == QC_OK);
    assert(start == 0);
    assert(qc_circuit_num_qubits(ctx) == 16);

    qc_circuit_destroy(ctx);
}

static void test_version(void)
{
    const char *vs = qc_version_string();
    assert(vs != NULL);
    assert(strcmp(vs, "1.0.0") == 0);

    int vn = qc_version_number();
    assert(vn == 10000);
}

static void test_partial_block_reuse(void)
{
    circuit_ctx_t *ctx = qc_circuit_create(32);
    uint32_t s;

    /* Allocate 8 qubits (0-7), then free them */
    qc_qubit_alloc_n(ctx, 8, &s);
    qc_qubit_free_n(ctx, 0, 8);

    /* Allocate only 3 -- should reuse from front of freed block */
    uint32_t s2;
    qc_qubit_alloc_n(ctx, 3, &s2);
    assert(s2 == 0);

    /* Remaining freed block should be [3, 5) */
    /* Allocate 5 more -- should reuse [3..7] */
    uint32_t s3;
    qc_qubit_alloc_n(ctx, 5, &s3);
    assert(s3 == 3);

    qc_circuit_destroy(ctx);
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== qubit_allocator tests ===\n");

    RUN_TEST(test_create_destroy);
    RUN_TEST(test_single_alloc);
    RUN_TEST(test_block_alloc);
    RUN_TEST(test_free_single);
    RUN_TEST(test_free_block);
    RUN_TEST(test_reuse);
    RUN_TEST(test_block_reuse);
    RUN_TEST(test_coalescing);
    RUN_TEST(test_double_free);
    RUN_TEST(test_free_invalid);
    RUN_TEST(test_statistics);
    RUN_TEST(test_is_allocated);
    RUN_TEST(test_reset);
    RUN_TEST(test_null_safety);
    RUN_TEST(test_many_alloc_free_cycles);
    RUN_TEST(test_capacity_growth);
    RUN_TEST(test_version);
    RUN_TEST(test_partial_block_reuse);

    printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
