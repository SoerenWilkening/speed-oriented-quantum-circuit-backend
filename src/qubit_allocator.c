/**
 * @file qubit_allocator.c
 * @brief Qubit allocation with state owned by circuit context.
 *
 * Refactored from monolith qubit_allocator.c (Module 1.4):
 *   - All global allocator state removed.
 *   - Allocator state (qc_allocator_t) is heap-allocated and owned by
 *     circuit_ctx_t (via pointer, created in circuit_context.c).
 *   - Every public function takes circuit_ctx_t* ctx as first parameter.
 *   - Internal helpers operate on qc_allocator_t* for clean separation.
 *
 * Features preserved from monolith:
 *   - Block-based free list with first-fit allocation.
 *   - Sorted freed-blocks with adjacent-block coalescing on free.
 *   - Dynamic capacity growth (doubling, capped at QC_ALLOCATOR_MAX_QUBITS).
 *   - Allocation statistics (peak, total allocs/frees, current in-use,
 *     ancilla count).
 *   - Double-free detection via overlap check.
 *
 * Public API (declared in quantum_circuit.h):
 *   qc_qubit_alloc()           - Allocate a single qubit.
 *   qc_qubit_alloc_n()         - Allocate a contiguous block of qubits.
 *   qc_qubit_free()            - Free a single qubit.
 *   qc_qubit_free_n()          - Free a contiguous block of qubits.
 *   qc_qubit_is_allocated()    - Check if a qubit is currently in use.
 *
 * Internal API (declared in internal.h):
 *   qc_allocator_create()      - Create heap-allocated allocator.
 *   qc_allocator_destroy()     - Free allocator and all resources.
 *   qc_allocator_alloc()       - Allocate contiguous qubits.
 *   qc_allocator_free()        - Free contiguous qubits.
 *   qc_allocator_is_allocated()- Check if qubit is allocated.
 *   qc_allocator_get_stats()   - Get statistics snapshot.
 *
 * Note: qc_circuit_num_qubits() and qc_circuit_alloc_stats() are
 * implemented in circuit_stats.c (Module 1.7).
 */

#include "internal.h"

#include <stdio.h>

/* ====================================================================== */
/* Internal API: allocator lifecycle                                        */
/* ====================================================================== */

qc_allocator_t *qc_allocator_create(uint32_t initial_capacity)
{
    if (initial_capacity == 0 || initial_capacity > QC_ALLOCATOR_MAX_QUBITS) {
        return NULL;
    }

    qc_allocator_t *alloc = calloc(1, sizeof(qc_allocator_t));
    if (alloc == NULL) {
        return NULL;
    }

    alloc->capacity   = initial_capacity;
    alloc->next_qubit = 0;

    /* Allocate and initialise indices array */
    alloc->indices = malloc(initial_capacity * sizeof(uint32_t));
    if (alloc->indices == NULL) {
        free(alloc);
        return NULL;
    }
    for (uint32_t i = 0; i < initial_capacity; i++) {
        alloc->indices[i] = i;
    }

    /* Allocate freed-blocks array */
    alloc->freed_block_capacity = 16;
    alloc->freed_block_count    = 0;
    alloc->freed_blocks = malloc(alloc->freed_block_capacity *
                                 sizeof(qc_qubit_block_t));
    if (alloc->freed_blocks == NULL) {
        free(alloc->indices);
        free(alloc);
        return NULL;
    }

    memset(&alloc->stats, 0, sizeof(alloc->stats));
    return alloc;
}

void qc_allocator_destroy(qc_allocator_t *alloc)
{
    if (alloc == NULL) {
        return;
    }
    free(alloc->indices);
    free(alloc->freed_blocks);
    free(alloc);
}

/* ====================================================================== */
/* Internal API: alloc                                                     */
/* ====================================================================== */

uint32_t qc_allocator_alloc(qc_allocator_t *alloc, uint32_t count,
                            bool is_ancilla)
{
    if (alloc == NULL || count == 0) {
        return (uint32_t)-1;
    }

    /* First-fit search in freed blocks */
    for (uint32_t i = 0; i < alloc->freed_block_count; i++) {
        if (alloc->freed_blocks[i].count >= count) {
            uint32_t start = alloc->freed_blocks[i].start;

            if (alloc->freed_blocks[i].count == count) {
                /* Exact fit: remove block entirely */
                alloc->freed_block_count--;
                if (i < alloc->freed_block_count) {
                    memmove(&alloc->freed_blocks[i],
                            &alloc->freed_blocks[i + 1],
                            (alloc->freed_block_count - i) *
                                sizeof(qc_qubit_block_t));
                }
            } else {
                /* Partial fit: shrink block (take from front) */
                alloc->freed_blocks[i].start += count;
                alloc->freed_blocks[i].count -= count;
            }

            alloc->stats.total_allocations++;
            alloc->stats.current_in_use += count;
            if (alloc->stats.current_in_use > alloc->stats.peak_allocated) {
                alloc->stats.peak_allocated = alloc->stats.current_in_use;
            }
            if (is_ancilla) {
                alloc->stats.ancilla_allocations += count;
            }
            return start;
        }
    }

    /* No suitable freed block -- allocate fresh qubits */
    if (alloc->next_qubit + count > QC_ALLOCATOR_MAX_QUBITS) {
        return (uint32_t)-1;
    }

    /* Grow indices array if needed */
    if (alloc->next_qubit + count > alloc->capacity) {
        uint32_t new_cap = alloc->capacity * 2;
        if (new_cap < alloc->next_qubit + count) {
            new_cap = alloc->next_qubit + count + QC_QUBIT_BLOCK;
        }
        if (new_cap > QC_ALLOCATOR_MAX_QUBITS) {
            new_cap = QC_ALLOCATOR_MAX_QUBITS;
        }
        uint32_t *new_indices = realloc(alloc->indices,
                                        new_cap * sizeof(uint32_t));
        if (new_indices == NULL) {
            return (uint32_t)-1;
        }
        for (uint32_t i = alloc->capacity; i < new_cap; i++) {
            new_indices[i] = i;
        }
        alloc->indices  = new_indices;
        alloc->capacity = new_cap;
    }

    uint32_t start = alloc->next_qubit;
    alloc->next_qubit += count;

    alloc->stats.total_allocations++;
    alloc->stats.current_in_use += count;
    if (alloc->stats.current_in_use > alloc->stats.peak_allocated) {
        alloc->stats.peak_allocated = alloc->stats.current_in_use;
    }
    if (is_ancilla) {
        alloc->stats.ancilla_allocations += count;
    }

    return start;
}

/* ====================================================================== */
/* Internal API: free                                                      */
/* ====================================================================== */

int qc_allocator_free(qc_allocator_t *alloc, uint32_t start, uint32_t count)
{
    if (alloc == NULL || count == 0) {
        return -1;
    }

    /* Validate range */
    if (start + count > alloc->next_qubit) {
        return -1;
    }

    /* Double-free detection: check overlap with existing freed blocks */
    for (uint32_t i = 0; i < alloc->freed_block_count; i++) {
        uint32_t bs = alloc->freed_blocks[i].start;
        uint32_t be = bs + alloc->freed_blocks[i].count;
        if (start < be && start + count > bs) {
            return -1;
        }
    }

    /* Find sorted insertion point */
    uint32_t insert_pos = 0;
    while (insert_pos < alloc->freed_block_count &&
           alloc->freed_blocks[insert_pos].start < start) {
        insert_pos++;
    }

    /* Grow freed_blocks array if needed */
    if (alloc->freed_block_count >= alloc->freed_block_capacity) {
        uint32_t new_cap = alloc->freed_block_capacity * 2;
        qc_qubit_block_t *new_blocks = realloc(
            alloc->freed_blocks, new_cap * sizeof(qc_qubit_block_t));
        if (new_blocks == NULL) {
            return -1;
        }
        alloc->freed_blocks         = new_blocks;
        alloc->freed_block_capacity = new_cap;
    }

    /* Shift right to make room for insertion */
    if (insert_pos < alloc->freed_block_count) {
        memmove(&alloc->freed_blocks[insert_pos + 1],
                &alloc->freed_blocks[insert_pos],
                (alloc->freed_block_count - insert_pos) *
                    sizeof(qc_qubit_block_t));
    }

    /* Insert new freed block */
    alloc->freed_blocks[insert_pos].start = start;
    alloc->freed_blocks[insert_pos].count = count;
    alloc->freed_block_count++;

    /* Coalesce with next block if adjacent */
    if (insert_pos + 1 < alloc->freed_block_count) {
        qc_qubit_block_t *cur  = &alloc->freed_blocks[insert_pos];
        qc_qubit_block_t *next = &alloc->freed_blocks[insert_pos + 1];
        if (cur->start + cur->count == next->start) {
            cur->count += next->count;
            alloc->freed_block_count--;
            if (insert_pos + 1 < alloc->freed_block_count) {
                memmove(&alloc->freed_blocks[insert_pos + 1],
                        &alloc->freed_blocks[insert_pos + 2],
                        (alloc->freed_block_count - insert_pos - 1) *
                            sizeof(qc_qubit_block_t));
            }
        }
    }

    /* Coalesce with previous block if adjacent */
    if (insert_pos > 0) {
        qc_qubit_block_t *prev = &alloc->freed_blocks[insert_pos - 1];
        qc_qubit_block_t *cur  = &alloc->freed_blocks[insert_pos];
        if (prev->start + prev->count == cur->start) {
            prev->count += cur->count;
            alloc->freed_block_count--;
            if (insert_pos < alloc->freed_block_count) {
                memmove(&alloc->freed_blocks[insert_pos],
                        &alloc->freed_blocks[insert_pos + 1],
                        (alloc->freed_block_count - insert_pos) *
                            sizeof(qc_qubit_block_t));
            }
        }
    }

    /* Update statistics */
    alloc->stats.total_deallocations++;
    if (alloc->stats.current_in_use >= count) {
        alloc->stats.current_in_use -= count;
    } else {
        alloc->stats.current_in_use = 0;
    }

    return 0;
}

/* ====================================================================== */
/* Internal API: query                                                     */
/* ====================================================================== */

bool qc_allocator_is_allocated(const qc_allocator_t *alloc, uint32_t qubit)
{
    if (alloc == NULL) {
        return false;
    }

    /* Never allocated? */
    if (qubit >= alloc->next_qubit) {
        return false;
    }

    /* Check if qubit falls within any freed block */
    for (uint32_t i = 0; i < alloc->freed_block_count; i++) {
        uint32_t bs = alloc->freed_blocks[i].start;
        uint32_t bc = alloc->freed_blocks[i].count;
        if (qubit >= bs && qubit < bs + bc) {
            return false;
        }
    }

    return true;
}

qc_alloc_stats_internal_t qc_allocator_get_stats(const qc_allocator_t *alloc)
{
    qc_alloc_stats_internal_t empty = {0, 0, 0, 0, 0};
    if (alloc == NULL) {
        return empty;
    }
    return alloc->stats;
}

/* ====================================================================== */
/* Public API: qubit allocation (wrappers delegating to ctx->allocator)    */
/* ====================================================================== */

QC_API qc_error_t qc_qubit_alloc(circuit_ctx_t *ctx, uint32_t *qubit)
{
    if (ctx == NULL || qubit == NULL) {
        return QC_ERR_NULL;
    }
    if (ctx->allocator == NULL) {
        return QC_ERR_NULL;
    }
    uint32_t q = qc_allocator_alloc(ctx->allocator, 1, false);
    if (q == (uint32_t)-1) {
        return QC_ERR_OVERFLOW;
    }
    *qubit = q;
    return QC_OK;
}

QC_API qc_error_t qc_qubit_alloc_n(circuit_ctx_t *ctx, uint32_t count,
                                    uint32_t *start)
{
    if (ctx == NULL || start == NULL) {
        return QC_ERR_NULL;
    }
    if (ctx->allocator == NULL) {
        return QC_ERR_NULL;
    }
    if (count == 0) {
        return QC_ERR_QUBIT;
    }
    uint32_t s = qc_allocator_alloc(ctx->allocator, count, false);
    if (s == (uint32_t)-1) {
        return QC_ERR_OVERFLOW;
    }
    *start = s;
    return QC_OK;
}

QC_API qc_error_t qc_qubit_free(circuit_ctx_t *ctx, uint32_t qubit)
{
    if (ctx == NULL) {
        return QC_ERR_NULL;
    }
    if (ctx->allocator == NULL) {
        return QC_ERR_NULL;
    }
    int rc = qc_allocator_free(ctx->allocator, qubit, 1);
    return (rc == 0) ? QC_OK : QC_ERR_DOUBLE_FREE;
}

QC_API qc_error_t qc_qubit_free_n(circuit_ctx_t *ctx, uint32_t start,
                                   uint32_t count)
{
    if (ctx == NULL) {
        return QC_ERR_NULL;
    }
    if (ctx->allocator == NULL) {
        return QC_ERR_NULL;
    }
    if (count == 0) {
        return QC_ERR_QUBIT;
    }
    int rc = qc_allocator_free(ctx->allocator, start, count);
    return (rc == 0) ? QC_OK : QC_ERR_DOUBLE_FREE;
}

QC_API bool qc_qubit_is_allocated(const circuit_ctx_t *ctx, uint32_t qubit)
{
    if (ctx == NULL || ctx->allocator == NULL) {
        return false;
    }
    return qc_allocator_is_allocated(ctx->allocator, qubit);
}
