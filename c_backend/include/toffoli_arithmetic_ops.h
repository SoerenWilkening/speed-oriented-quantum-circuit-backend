/**
 * @file toffoli_arithmetic_ops.h
 * @brief Toffoli-based arithmetic operations (CDKM ripple-carry adder).
 *
 * Provides Toffoli-based addition using the CDKM (Cuccaro-Draper-Kutin-Moulton)
 * ripple-carry adder circuit. Uses MAJ/UMA gate chains instead of QFT rotations.
 *
 * Phase 66: Core CDKM adder implementation.
 *
 * Dependencies: types.h
 */

#ifndef TOFFOLI_ARITHMETIC_OPS_H
#define TOFFOLI_ARITHMETIC_OPS_H

#include "types.h"
#include <stdint.h>

/**
 * @brief Quantum-quantum Toffoli addition: a += b (CDKM ripple-carry).
 *
 * Generates a CDKM adder sequence using MAJ/UMA chain.
 * For bits >= 2, requires 1 ancilla qubit at virtual index 2*bits.
 * For bits == 1, uses a single CNOT (no ancilla needed).
 *
 * @param bits Width of operands (1-64)
 * @return Cached sequence - DO NOT FREE (NULL on invalid input)
 *
 * Qubit layout:
 *   [0..bits-1]       = register a (target, modified in place)
 *   [bits..2*bits-1]   = register b (source, unchanged)
 *   [2*bits]           = ancilla carry (bits >= 2 only, returned to |0>)
 *
 * OWNERSHIP: Returns cached sequence - DO NOT FREE
 */
sequence_t *toffoli_QQ_add(int bits);

/**
 * @brief Classical-quantum Toffoli addition: a += classical_value (CDKM-based).
 *
 * Generates a value-dependent gate sequence using CX/CCX/X gates only.
 * Classical bits simplify the MAJ/UMA pattern (no Toffoli when bit=0).
 * For bits >= 2, requires 1 ancilla qubit at virtual index bits.
 * For bits == 1, uses a single X gate if LSB=1, or identity if LSB=0.
 *
 * @param bits Width of target operand (1-64)
 * @param value Classical integer value to add
 * @return Fresh sequence - CALLER MUST FREE via toffoli_sequence_free()
 *
 * Qubit layout:
 *   [0..bits-1]  = register a (target, modified in place)
 *   [bits]       = ancilla carry (bits >= 2 only, returned to |0>)
 *
 * OWNERSHIP: Caller owns returned sequence_t*, must free via toffoli_sequence_free()
 */
sequence_t *toffoli_CQ_add(int bits, int64_t value);

/**
 * @brief Free a Toffoli CQ addition sequence.
 *
 * CQ sequences are value-dependent and cannot be cached, so they must be
 * freed after use. This function frees all internal arrays and the sequence itself.
 *
 * @param seq Sequence to free (can be NULL)
 */
void toffoli_sequence_free(sequence_t *seq);

#endif // TOFFOLI_ARITHMETIC_OPS_H
