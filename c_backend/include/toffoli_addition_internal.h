/**
 * @file toffoli_addition_internal.h
 * @brief Shared internal types and declarations for Toffoli addition modules.
 *
 * This header is used by ToffoliAdditionCDKM.c, ToffoliAdditionCLA.c, and
 * ToffoliAdditionHelpers.c to share type definitions and utility function
 * declarations. Not part of the public API.
 *
 * Phase 74: Extracted during ToffoliAddition.c split refactoring.
 */

#ifndef TOFFOLI_ADDITION_INTERNAL_H
#define TOFFOLI_ADDITION_INTERNAL_H

#include "types.h"
#include <stdint.h>

/**
 * @brief BK tree merge descriptor: one prefix-tree operation.
 */
typedef struct {
    int pos;     /* position being updated (target of group generate) */
    int partner; /* position being merged from (source) */
    int level;   /* tree level (0-based) */
    int is_down; /* 0 = up-sweep, 1 = down-sweep/tail */
} bk_merge_t;

/**
 * @brief Allocate a sequence with given number of layers, 1 gate per layer.
 *
 * @param num_layers Number of layers to allocate
 * @return Allocated sequence, or NULL on failure
 */
sequence_t *alloc_sequence(int num_layers);

/**
 * @brief Deep-copy a const (hardcoded) sequence into a fresh malloc'd sequence.
 *
 * CQ/cCQ callers free the returned sequence, so hardcoded static sequences
 * must be copied before returning. This copies large_control arrays for MCX gates.
 *
 * @param src Source sequence (typically static const from hardcoded file)
 * @return Fresh sequence_t* owned by caller, or NULL on allocation failure
 */
sequence_t *copy_hardcoded_sequence(const sequence_t *src);

/**
 * @brief Compute BK prefix tree merge list.
 *
 * Generates an ordered list of merge operations for the Brent-Kung parallel
 * prefix tree operating on n_carries carry positions (0..n_carries-1).
 *
 * @param n_carries Number of carry positions (bits - 1)
 * @param merges Output array (caller-allocated, max 128 entries)
 * @param max_merges Maximum entries in merges array
 * @return Number of merges written
 */
int bk_compute_merges(int n_carries, bk_merge_t *merges, int max_merges);

#endif /* TOFFOLI_ADDITION_INTERNAL_H */
