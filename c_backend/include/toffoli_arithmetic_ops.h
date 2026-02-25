/**
 * @file toffoli_arithmetic_ops.h
 * @brief Toffoli-based arithmetic operations (CDKM ripple-carry adder).
 *
 * Provides Toffoli-based addition using the CDKM (Cuccaro-Draper-Kutin-Moulton)
 * ripple-carry adder circuit. Uses MAJ/UMA gate chains instead of QFT rotations.
 *
 * Phase 66: Core CDKM adder implementation (QQ, CQ).
 * Phase 67: Controlled variants (cQQ, cCQ) using CCX + MCX gates.
 *
 * Dependencies: types.h, circuit.h (for circuit_t in multiplication functions)
 */

#ifndef TOFFOLI_ARITHMETIC_OPS_H
#define TOFFOLI_ARITHMETIC_OPS_H

#include "circuit.h"
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
 * @brief Classical-quantum Toffoli addition: self += classical_value (CDKM-based).
 *
 * Uses temp-register approach: initializes a temp register to the classical value
 * via X gates, runs the proven QQ CDKM adder, then undoes the X gates.
 * For bits >= 2, requires bits+1 ancilla qubits (bits for temp + 1 carry).
 * For bits == 1, uses a single X gate if LSB=1, or identity if LSB=0.
 *
 * @param bits Width of target operand (1-64)
 * @param value Classical integer value to add
 * @return Fresh sequence - CALLER MUST FREE via toffoli_sequence_free()
 *
 * Qubit layout:
 *   [0..bits-1]       = temp register (initialized to classical value, cleaned to |0>)
 *   [bits..2*bits-1]  = self register (target, modified: self += value)
 *   [2*bits]          = carry ancilla (bits >= 2 only, returned to |0>)
 *
 * OWNERSHIP: Caller owns returned sequence_t*, must free via toffoli_sequence_free()
 */
sequence_t *toffoli_CQ_add(int bits, int64_t value);

/**
 * @brief Controlled quantum-quantum Toffoli addition: a += b, controlled by ext_ctrl.
 *
 * Generates a controlled CDKM adder sequence using cMAJ/cUMA chains (CCX + MCX).
 * The addition only occurs when the external control qubit is |1>.
 * For bits >= 2, requires 1 ancilla qubit at virtual index 2*bits.
 * For bits == 1, uses a single CCX (no ancilla needed).
 *
 * @param bits Width of operands (1-64)
 * @return Cached sequence - DO NOT FREE (NULL on invalid input)
 *
 * Qubit layout:
 *   [0..bits-1]       = register a (target, modified in place: a += b)
 *   [bits..2*bits-1]   = register b (source, unchanged)
 *   [2*bits]           = ancilla carry (bits >= 2 only, returned to |0>)
 *   [2*bits+1]         = external control qubit
 *   [2*bits+2]         = AND-ancilla for MCX decomposition (bits >= 2 only)
 *
 * For bits == 1: [0]=a, [1]=b, [2]=ext_control. Total qubits: 3.
 * For bits >= 2: Total qubits: 2*bits + 3 (Phase 74-03: +1 for AND-ancilla).
 *
 * OWNERSHIP: Returns cached sequence - DO NOT FREE
 */
sequence_t *toffoli_cQQ_add(int bits);

/**
 * @brief Controlled classical-quantum Toffoli addition: self += value, controlled.
 *
 * Uses controlled temp-register approach: CX(target=temp[i], control=ext_ctrl)
 * for initialization (only sets temp when control is |1>), controlled CDKM adder
 * core using cMAJ/cUMA, then CX cleanup.
 *
 * For bits >= 2, requires bits+1 ancilla qubits (bits for temp + 1 carry).
 * For bits == 1: CX(target=self, control=ext_ctrl) if value LSB=1, else empty.
 *
 * @param bits Width of target operand (1-64)
 * @param value Classical integer value to add
 * @return Fresh sequence - CALLER MUST FREE via toffoli_sequence_free()
 *
 * Qubit layout:
 *   [0..bits-1]       = temp register (controlled init to classical value, controlled cleanup)
 *   [bits..2*bits-1]  = self register (target, modified: self += value)
 *   [2*bits]          = carry ancilla (bits >= 2 only, returned to |0>)
 *   [2*bits+1]        = external control qubit
 *   [2*bits+2]        = AND-ancilla for MCX decomposition (bits >= 2 only)
 *
 * For bits == 1: [0]=self, [1]=ext_control. Total qubits: 2.
 * For bits >= 2: Total qubits: 2*bits + 3 (Phase 74-03: +1 for AND-ancilla).
 *
 * OWNERSHIP: Caller owns returned sequence_t*, must free via toffoli_sequence_free()
 * NOT cached (value-dependent).
 */
sequence_t *toffoli_cCQ_add(int bits, int64_t value);

// ============================================================================
// Carry Look-Ahead Addition (Phase 71)
// ============================================================================

/**
 * @brief Compute ancilla count for Brent-Kung CLA adder.
 *
 * Returns the total number of ancilla qubits needed: 2*(n-1) + tree_merges.
 * Used by hot_path_add.c to allocate the correct ancilla block.
 *
 * @param bits Width of operands (>= 2)
 * @return Number of ancilla qubits needed (0 for bits < 2)
 */
int bk_cla_ancilla_count(int bits);

/**
 * @brief Brent-Kung CLA QQ addition: b += a (O(log n) depth).
 *
 * Implements the Brent-Kung parallel prefix CLA using a compute-copy-uncompute
 * pattern: compute carries via prefix tree, copy to carry-copy ancilla, reverse
 * tree to uncompute generates/propagates, then extract sums using carries.
 *
 * @param bits Width of operands (2-64; returns NULL for bits < 2)
 * @return Cached sequence, or NULL on failure
 *
 * OWNERSHIP: Returns cached sequence - DO NOT FREE
 */
sequence_t *toffoli_QQ_add_bk(int bits);

/**
 * @brief Kogge-Stone CLA QQ addition: b += a (O(log n) depth).
 *
 * STUB: Returns NULL -- same ancilla uncomputation impossibility as BK.
 * Dispatch silently falls through to CDKM RCA adder.
 *
 * @param bits Width of operands (2-64; returns NULL for bits < 2)
 * @return NULL (CLA not yet implemented; falls through to RCA)
 *
 * OWNERSHIP: Returns cached sequence - DO NOT FREE
 */
sequence_t *toffoli_QQ_add_ks(int bits);

/**
 * @brief Brent-Kung CLA CQ addition: self += classical_value.
 *
 * Uses sequence-copy from cached QQ BK with X-init/cleanup for classical value.
 *
 * @param bits Width of target operand (2-64; returns NULL for bits < 2)
 * @param value Classical integer value to add
 * @return Fresh sequence, or NULL on failure
 *
 * OWNERSHIP: Caller owns returned sequence_t*, must free via toffoli_sequence_free()
 */
sequence_t *toffoli_CQ_add_bk(int bits, int64_t value);

/**
 * @brief Kogge-Stone CLA CQ addition: self += classical_value.
 *
 * STUB: Returns NULL -- KS QQ CLA not implemented.
 * Dispatch silently falls through to CDKM RCA CQ adder.
 *
 * @param bits Width of target operand (1-64)
 * @param value Classical integer value to add
 * @return NULL (falls through to RCA CQ)
 *
 * OWNERSHIP: Caller owns returned sequence_t*, must free via toffoli_sequence_free()
 */
sequence_t *toffoli_CQ_add_ks(int bits, int64_t value);

// ============================================================================
// Controlled Carry Look-Ahead Addition (Phase 71, Plan 03)
// ============================================================================

/**
 * @brief Controlled Brent-Kung CLA QQ addition: b += a, controlled by ext_ctrl.
 *
 * Sequence-copy from cached QQ BK with ext_ctrl injected into every gate.
 * ext_ctrl placed at qubit index 2*bits + bk_cla_ancilla_count(bits).
 *
 * @param bits Width of operands (2-64; returns NULL for bits < 2)
 * @return Cached sequence, or NULL on failure
 *
 * OWNERSHIP: Returns cached sequence - DO NOT FREE
 */
sequence_t *toffoli_cQQ_add_bk(int bits);

/**
 * @brief Controlled Kogge-Stone CLA QQ addition: b += a, controlled.
 *
 * STUB: Returns NULL -- same ancilla uncomputation impossibility as uncontrolled KS.
 * Dispatch silently falls through to controlled CDKM RCA adder.
 *
 * @param bits Width of operands (2-64; returns NULL for bits < 2)
 * @return NULL (controlled CLA not yet implemented; falls through to RCA)
 *
 * OWNERSHIP: Returns cached sequence - DO NOT FREE
 */
sequence_t *toffoli_cQQ_add_ks(int bits);

/**
 * @brief Controlled Brent-Kung CLA CQ addition: self += classical_value, controlled.
 *
 * Sequence-copy from cached cQQ BK with CX-init/cleanup for classical value.
 * ext_ctrl placed at qubit index 2*bits + bk_cla_ancilla_count(bits).
 *
 * @param bits Width of target operand (2-64; returns NULL for bits < 2)
 * @param value Classical integer value to add
 * @return Fresh sequence, or NULL on failure
 *
 * OWNERSHIP: Caller owns returned sequence_t*, must free via toffoli_sequence_free()
 */
sequence_t *toffoli_cCQ_add_bk(int bits, int64_t value);

/**
 * @brief Controlled Kogge-Stone CLA CQ addition: self += classical_value, controlled.
 *
 * STUB: Returns NULL -- controlled KS CLA not implemented.
 * Dispatch silently falls through to controlled CDKM RCA CQ adder.
 *
 * @param bits Width of target operand (1-64)
 * @param value Classical integer value to add
 * @return NULL (falls through to controlled RCA CQ)
 *
 * OWNERSHIP: Caller owns returned sequence_t*, must free via toffoli_sequence_free()
 */
sequence_t *toffoli_cCQ_add_ks(int bits, int64_t value);

// ============================================================================
// Toffoli Multiplication (Phase 68)
// ============================================================================

/**
 * @brief Toffoli schoolbook QQ multiplication: ret = self * other.
 *
 * Uses controlled CDKM adders in a shift-and-add loop. For each bit j of
 * the multiplier (other), performs a controlled addition of self[0..n-1-j]
 * into ret[j..n-1], controlled by other[j].
 *
 * @param circ          Active circuit
 * @param ret_qubits    Result register qubits (accumulator, starts at |0>)
 * @param ret_bits      Width of result register (n)
 * @param self_qubits   Multiplicand register qubits (preserved)
 * @param self_bits     Width of multiplicand
 * @param other_qubits  Multiplier register qubits (preserved)
 * @param other_bits    Width of multiplier
 */
void toffoli_mul_qq(circuit_t *circ, const unsigned int *ret_qubits, int ret_bits,
                    const unsigned int *self_qubits, int self_bits,
                    const unsigned int *other_qubits, int other_bits);

/**
 * @brief Toffoli schoolbook CQ multiplication: ret = self * classical_value.
 *
 * Decomposes classical_value into binary. For each set bit j, performs an
 * uncontrolled addition of self[0..n-1-j] into ret[j..n-1].
 *
 * @param circ            Active circuit
 * @param ret_qubits      Result register qubits (accumulator, starts at |0>)
 * @param ret_bits        Width of result register (n)
 * @param self_qubits     Multiplicand register qubits (preserved)
 * @param self_bits       Width of multiplicand
 * @param classical_value Classical integer to multiply by
 */
void toffoli_mul_cq(circuit_t *circ, const unsigned int *ret_qubits, int ret_bits,
                    const unsigned int *self_qubits, int self_bits, int64_t classical_value);

// ============================================================================
// Controlled Toffoli Multiplication (Phase 69)
// ============================================================================

/**
 * @brief Controlled Toffoli QQ multiplication: ret = self * other, controlled.
 *
 * Uses the AND-ancilla pattern: for each multiplier bit other[j], computes
 * and_ancilla = other[j] AND ext_ctrl via CCX, uses and_ancilla as the single
 * control for toffoli_cQQ_add, then uncomputes AND via a second CCX.
 *
 * For width 1: emits a single MCX with 3 controls (self[0], other[j], ext_ctrl).
 * Ancilla: 1 carry + 1 AND = 2 ancilla (allocated before loop, freed after).
 *
 * @param circ          Active circuit
 * @param ret_qubits    Result register qubits (accumulator, starts at |0>)
 * @param ret_bits      Width of result register (n)
 * @param self_qubits   Multiplicand register qubits (preserved)
 * @param self_bits     Width of multiplicand
 * @param other_qubits  Multiplier register qubits (preserved)
 * @param other_bits    Width of multiplier
 * @param ext_ctrl      External control qubit
 */
void toffoli_cmul_qq(circuit_t *circ, const unsigned int *ret_qubits, int ret_bits,
                     const unsigned int *self_qubits, int self_bits,
                     const unsigned int *other_qubits, int other_bits, unsigned int ext_ctrl);

/**
 * @brief Controlled Toffoli CQ multiplication: ret = self * classical_value, controlled.
 *
 * For each set bit j of classical_value, performs a controlled addition of
 * self[0..width-1] into ret[j..j+width-1] using toffoli_cQQ_add with ext_ctrl.
 *
 * No AND-ancilla needed: classical bit selection is compile-time; only the
 * runtime ext_ctrl gates each addition.
 *
 * For width 1: uses toffoli_cQQ_add(1) with ext_ctrl as control (CCX).
 * Ancilla: 1 carry (allocated before loop, freed after).
 *
 * @param circ            Active circuit
 * @param ret_qubits      Result register qubits (accumulator, starts at |0>)
 * @param ret_bits        Width of result register (n)
 * @param self_qubits     Multiplicand register qubits (preserved)
 * @param self_bits       Width of multiplicand
 * @param classical_value Classical integer to multiply by
 * @param ext_ctrl        External control qubit
 */
void toffoli_cmul_cq(circuit_t *circ, const unsigned int *ret_qubits, int ret_bits,
                     const unsigned int *self_qubits, int self_bits, int64_t classical_value,
                     unsigned int ext_ctrl);

/**
 * @brief Free a Toffoli addition sequence.
 *
 * Frees all internal arrays including large_control arrays for MCX gates
 * with 3+ controls (prevents memory leaks from controlled adder sequences).
 * CQ/cCQ sequences are value-dependent and cannot be cached, so they must
 * be freed after use.
 *
 * @param seq Sequence to free (can be NULL)
 */
void toffoli_sequence_free(sequence_t *seq);

// ============================================================================
// Toffoli Division (Phase 91)
// ============================================================================

/**
 * @brief Restoring divmod with classical divisor.
 *
 * Computes quotient = dividend / divisor, remainder = dividend % divisor.
 * Uses Bennett's trick for reversible comparison at each bit position.
 * All ancillae allocated internally and freed after use.
 *
 * @param circ             Active circuit
 * @param dividend_qubits  Dividend register (preserved), LSB-first
 * @param dividend_bits    Width of dividend (n)
 * @param divisor          Classical divisor value (> 0; 0 = sentinel)
 * @param quotient_qubits  Quotient output register (starts at |0>), LSB-first
 * @param remainder_qubits Remainder output register (starts at |0>), LSB-first
 */
void toffoli_divmod_cq(circuit_t *circ, const unsigned int *dividend_qubits, int dividend_bits,
                       int64_t divisor, const unsigned int *quotient_qubits,
                       const unsigned int *remainder_qubits);

/**
 * @brief Restoring divmod with quantum divisor.
 *
 * Computes quotient = dividend / divisor, remainder = dividend % divisor
 * using repeated subtraction (2^n iterations).
 *
 * @param circ             Active circuit
 * @param dividend_qubits  Dividend register (preserved), LSB-first
 * @param dividend_bits    Width of dividend (n)
 * @param divisor_qubits   Divisor register (preserved), LSB-first
 * @param divisor_bits     Width of divisor
 * @param quotient_qubits  Quotient output register (starts at |0>), LSB-first
 * @param remainder_qubits Remainder output register (starts at |0>), LSB-first
 */
void toffoli_divmod_qq(circuit_t *circ, const unsigned int *dividend_qubits, int dividend_bits,
                       const unsigned int *divisor_qubits, int divisor_bits,
                       const unsigned int *quotient_qubits, const unsigned int *remainder_qubits);

/**
 * @brief Controlled restoring divmod with classical divisor.
 *
 * Same as toffoli_divmod_cq but all operations controlled by ext_ctrl.
 *
 * @param circ             Active circuit
 * @param dividend_qubits  Dividend register (preserved), LSB-first
 * @param dividend_bits    Width of dividend (n)
 * @param divisor          Classical divisor value
 * @param quotient_qubits  Quotient output register (starts at |0>), LSB-first
 * @param remainder_qubits Remainder output register (starts at |0>), LSB-first
 * @param ext_ctrl         External control qubit
 */
void toffoli_cdivmod_cq(circuit_t *circ, const unsigned int *dividend_qubits, int dividend_bits,
                        int64_t divisor, const unsigned int *quotient_qubits,
                        const unsigned int *remainder_qubits, unsigned int ext_ctrl);

/**
 * @brief Controlled restoring divmod with quantum divisor.
 *
 * Same as toffoli_divmod_qq but all operations controlled by ext_ctrl.
 *
 * @param circ             Active circuit
 * @param dividend_qubits  Dividend register (preserved), LSB-first
 * @param dividend_bits    Width of dividend (n)
 * @param divisor_qubits   Divisor register (preserved), LSB-first
 * @param divisor_bits     Width of divisor
 * @param quotient_qubits  Quotient output register (starts at |0>), LSB-first
 * @param remainder_qubits Remainder output register (starts at |0>), LSB-first
 * @param ext_ctrl         External control qubit
 */
void toffoli_cdivmod_qq(circuit_t *circ, const unsigned int *dividend_qubits, int dividend_bits,
                        const unsigned int *divisor_qubits, int divisor_bits,
                        const unsigned int *quotient_qubits, const unsigned int *remainder_qubits,
                        unsigned int ext_ctrl);

// ============================================================================
// Toffoli Modular Reduction (Phase 91)
// ============================================================================

/**
 * @brief Modular reduction: value = value mod modulus.
 *
 * Reduces a quantum register in-place modulo a classical N.
 * Input assumed to be in range [0, 2N-2] (valid for modular addition results).
 * Uses single conditional subtraction of N.
 *
 * @param circ         Active circuit
 * @param value_qubits Register to reduce (modified in-place), LSB-first
 * @param value_bits   Width of register
 * @param modulus      Classical modulus N (> 0)
 */
void toffoli_mod_reduce(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                        int64_t modulus);

/**
 * @brief Controlled modular reduction.
 *
 * @param circ         Active circuit
 * @param value_qubits Register to reduce (modified in-place), LSB-first
 * @param value_bits   Width of register
 * @param modulus      Classical modulus N (> 0)
 * @param ext_ctrl     External control qubit
 */
void toffoli_cmod_reduce(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                         int64_t modulus, unsigned int ext_ctrl);

/**
 * @brief Modular CQ addition: value = (value + addend) mod modulus.
 *
 * @param circ         Active circuit
 * @param value_qubits Register (modified in-place), LSB-first
 * @param value_bits   Width of register
 * @param addend       Classical value to add
 * @param modulus      Classical modulus N (> 0)
 */
void toffoli_mod_add_cq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                        int64_t addend, int64_t modulus);

/**
 * @brief Controlled modular CQ addition.
 *
 * @param circ         Active circuit
 * @param value_qubits Register (modified in-place), LSB-first
 * @param value_bits   Width of register
 * @param addend       Classical value to add
 * @param modulus      Classical modulus N (> 0)
 * @param ext_ctrl     External control qubit
 */
void toffoli_cmod_add_cq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                         int64_t addend, int64_t modulus, unsigned int ext_ctrl);

// ============================================================================
// Toffoli Modular Subtraction (Phase 92)
// ============================================================================

/**
 * @brief Modular CQ subtraction: value = (value - subtrahend) mod modulus.
 *
 * Implemented as mod_add_cq(value, N - subtrahend, N).
 *
 * @param circ         Active circuit
 * @param value_qubits Register (modified in-place), LSB-first
 * @param value_bits   Width of register
 * @param subtrahend   Classical value to subtract
 * @param modulus      Classical modulus N (> 0)
 */
void toffoli_mod_sub_cq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                        int64_t subtrahend, int64_t modulus);

/**
 * @brief Controlled modular CQ subtraction.
 */
void toffoli_cmod_sub_cq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                         int64_t subtrahend, int64_t modulus, unsigned int ext_ctrl);

// ============================================================================
// Toffoli Modular QQ Addition/Subtraction (Phase 92)
// ============================================================================

/**
 * @brief Modular QQ addition: value = (value + other) mod modulus.
 *
 * Beauregard 8-step sequence with QQ adders. Source register (other) is preserved.
 *
 * @param circ          Active circuit
 * @param value_qubits  Register (modified in-place), LSB-first
 * @param value_bits    Width of value register
 * @param other_qubits  Source register (preserved), LSB-first
 * @param other_bits    Width of source register
 * @param modulus       Classical modulus N (> 0)
 */
void toffoli_mod_add_qq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                        const unsigned int *other_qubits, int other_bits, int64_t modulus);

/**
 * @brief Controlled modular QQ addition.
 */
void toffoli_cmod_add_qq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                         const unsigned int *other_qubits, int other_bits, int64_t modulus,
                         unsigned int ext_ctrl);

/**
 * @brief Modular QQ subtraction: value = (value - other) mod modulus.
 *
 * Computes (N - other) into temp register, does modular QQ add, then
 * uncomputes temp.
 *
 * @param circ          Active circuit
 * @param value_qubits  Register (modified in-place), LSB-first
 * @param value_bits    Width of value register
 * @param other_qubits  Source register (preserved), LSB-first
 * @param other_bits    Width of source register
 * @param modulus       Classical modulus N (> 0)
 */
void toffoli_mod_sub_qq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                        const unsigned int *other_qubits, int other_bits, int64_t modulus);

/**
 * @brief Controlled modular QQ subtraction.
 */
void toffoli_cmod_sub_qq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                         const unsigned int *other_qubits, int other_bits, int64_t modulus,
                         unsigned int ext_ctrl);

// ============================================================================
// Toffoli Modular Multiplication (Phase 92)
// ============================================================================

/**
 * @brief Modular CQ multiplication: result = value * multiplier mod modulus.
 *
 * Schoolbook approach: for each bit j of value (quantum), performs controlled
 * modular addition of (multiplier * 2^j mod N) to result, controlled by value[j].
 *
 * @param circ           Active circuit
 * @param value_qubits   Multiplicand register (preserved), LSB-first
 * @param value_bits     Width of multiplicand
 * @param result_qubits  Result register (starts at |0>), LSB-first
 * @param result_bits    Width of result register
 * @param multiplier     Classical multiplier
 * @param modulus        Classical modulus N (> 0)
 */
void toffoli_mod_mul_cq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                        const unsigned int *result_qubits, int result_bits, int64_t multiplier,
                        int64_t modulus);

/**
 * @brief Controlled modular CQ multiplication.
 */
void toffoli_cmod_mul_cq(circuit_t *circ, const unsigned int *value_qubits, int value_bits,
                         const unsigned int *result_qubits, int result_bits, int64_t multiplier,
                         int64_t modulus, unsigned int ext_ctrl);

/**
 * @brief Modular QQ multiplication: result = a * b mod modulus.
 *
 * Computes non-modular product, then reduces via bit-decomposition with
 * controlled modular additions of (2^k mod N).
 *
 * @param circ           Active circuit
 * @param a_qubits       First operand (preserved), LSB-first
 * @param a_bits         Width of first operand
 * @param b_qubits       Second operand (preserved), LSB-first
 * @param b_bits         Width of second operand
 * @param result_qubits  Result register (starts at |0>), LSB-first
 * @param result_bits    Width of result register
 * @param modulus        Classical modulus N (> 0)
 */
void toffoli_mod_mul_qq(circuit_t *circ, const unsigned int *a_qubits, int a_bits,
                        const unsigned int *b_qubits, int b_bits, const unsigned int *result_qubits,
                        int result_bits, int64_t modulus);

/**
 * @brief Controlled modular QQ multiplication.
 */
void toffoli_cmod_mul_qq(circuit_t *circ, const unsigned int *a_qubits, int a_bits,
                         const unsigned int *b_qubits, int b_bits,
                         const unsigned int *result_qubits, int result_bits, int64_t modulus,
                         unsigned int ext_ctrl);

#endif // TOFFOLI_ARITHMETIC_OPS_H
