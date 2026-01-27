//
// comparison_ops.h - Comparison Operations Module
// Dependencies: types.h
//
// This header provides comparison operations (==, <, >, <=, >=).
// Part of CODE-04 reorganization to establish clear module boundaries.
//

#ifndef QUANTUM_COMPARISON_OPS_H
#define QUANTUM_COMPARISON_OPS_H

#include "types.h"
#include <stdint.h>

// ======================================================
// Legacy Comparison Operations (INTEGERSIZE-based)
// ======================================================

// OWNERSHIP: Returns NULL (performs classical computation only via QPU_state)
sequence_t *CC_equal();

// OWNERSHIP: Caller owns returned sequence_t*, must free gates_per_layer, seq arrays, and seq
// READS: QPU_state->R0 for classical value
sequence_t *CQ_equal();

// OWNERSHIP: Caller owns returned sequence_t*, must free gates_per_layer, seq arrays, and seq
// READS: QPU_state->R0 for classical value
sequence_t *cCQ_equal();

// ======================================================
// Width-Parameterized Comparison Operations (Phase 7)
// ======================================================

// OWNERSHIP: Returns cached sequence - DO NOT FREE
// Qubit layout: [0] = result qbool, [1:bits+1] = ancilla,
//               [bits+1:2*bits+1] = operand A, [2*bits+1:3*bits+1] = operand B

/**
 * Optimized equality comparison: A == B
 * Uses XOR-based circuit (O(n) gates) instead of subtraction (O(n^2) gates)
 *
 * @param bits Width of operands (1-64)
 * @return Sequence for equality comparison, NULL if invalid bits
 */
sequence_t *QQ_equal(int bits);

/**
 * Less-than comparison: A < B
 * Uses subtraction and MSB check
 *
 * @param bits Width of operands (1-64)
 * @return Sequence for less-than comparison, NULL if invalid bits
 */
sequence_t *QQ_less_than(int bits);

/**
 * Classical-quantum equality: A == value
 *
 * @param bits Width of quantum operand (1-64)
 * @param value Classical value to compare against
 * @return Sequence for equality comparison
 */
sequence_t *CQ_equal_width(int bits, int64_t value);

/**
 * Classical-quantum less-than: A < value
 *
 * @param bits Width of quantum operand (1-64)
 * @param value Classical value to compare against
 * @return Sequence for comparison
 */
sequence_t *CQ_less_than(int bits, int64_t value);

#endif // QUANTUM_COMPARISON_OPS_H
