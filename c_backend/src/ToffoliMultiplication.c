/**
 * @file ToffoliMultiplication.c
 * @brief Toffoli-based schoolbook multiplication (Phase 68).
 *
 * Implements shift-and-add multiplication using the CDKM ripple-carry adders
 * from ToffoliAddition.c as subroutines. Each multiplier bit controls whether
 * a shifted copy of the multiplicand is added to the result accumulator.
 *
 * Two variants:
 *   toffoli_mul_qq  -- quantum * quantum (controlled additions per multiplier bit)
 *   toffoli_mul_cq  -- quantum * classical (uncontrolled additions for set bits)
 *
 * Qubit arrays use LSB-first convention from the C hot path perspective:
 *   index 0 = LSB (weight 2^0), index n-1 = MSB (weight 2^(n-1)).
 * This matches the CDKM adder's internal convention.
 *
 * References:
 *   Schoolbook multiplication: sum_{j=0}^{n-1} a * b[j] * 2^j
 *   CDKM adder: Cuccaro et al., arXiv:quant-ph/0410184
 */

#include "Integer.h"
#include "QPU.h"
#include "execution.h"
#include "qubit_allocator.h"
#include "toffoli_arithmetic_ops.h"

#include <stdlib.h>

/**
 * @brief Toffoli schoolbook QQ multiplication: ret = self * other.
 *
 * Uses controlled CDKM adders: for each bit j of the multiplier (other),
 * performs a controlled addition of self[0..width-1] into ret[j..j+width-1],
 * controlled by other[j].
 *
 * The shift is implicit: adding into ret[j..] effectively multiplies by 2^j.
 * Each iteration uses progressively smaller addition width (n-j).
 *
 * LSB-first convention: index 0 = LSB, index n-1 = MSB.
 *
 * @param circ          Active circuit
 * @param ret_qubits    Result register (accumulator, starts at |0>)
 * @param ret_bits      Width of result register (n)
 * @param self_qubits   Multiplicand register (preserved)
 * @param self_bits     Width of multiplicand
 * @param other_qubits  Multiplier register (preserved, each bit used as control)
 * @param other_bits    Width of multiplier
 */
void toffoli_mul_qq(circuit_t *circ, const unsigned int *ret_qubits, int ret_bits,
                    const unsigned int *self_qubits, int self_bits,
                    const unsigned int *other_qubits, int other_bits) {
    int n = ret_bits;

    for (int j = 0; j < n; j++) {
        int width = n - j; /* Addition width for this iteration */
        if (width < 1)
            break;

        unsigned int tqa[256];
        sequence_t *toff_seq;

        if (width == 1) {
            /* 1-bit controlled addition: CCX
             * toffoli_cQQ_add(1) layout: [0]=target, [1]=source, [2]=control
             *
             * LSB-first: ret[n-1] = MSB of ret, self[0] = LSB of self,
             *            other[j] = multiplier bit with weight 2^j.
             * For j = n-1: adds self[0] (LSB) into ret[n-1] (MSB). */
            tqa[0] = ret_qubits[n - 1];
            tqa[1] = self_qubits[0];
            tqa[2] = other_qubits[j];

            toff_seq = toffoli_cQQ_add(1);
            if (toff_seq == NULL)
                return;
            run_instruction(toff_seq, tqa, 0, circ);
        } else {
            /* General case: controlled addition of width bits
             * Allocate 1 ancilla for carry */
            qubit_t ancilla = allocator_alloc(circ->allocator, 1, true);
            if (ancilla == (qubit_t)-1)
                return;

            /* CDKM convention (matching hot_path_add.c):
             *   a-register [0..width-1]       = source (preserved)
             *   b-register [width..2*width-1]  = target (gets sum)
             *   [2*width]                      = carry ancilla
             *   [2*width+1]                    = control qubit
             *
             * For multiplier bit j (weight 2^j):
             *   self slice: self[0..width-1] (the lower width=n-j bits of multiplicand)
             *   ret slice:  ret[j..j+width-1] (shifted by j positions in result)
             *   control:    other[j] (multiplier bit with weight 2^j)
             */

            /* a-register = self[0..width-1] (multiplicand lower bits, preserved) */
            for (int i = 0; i < width; i++) {
                tqa[i] = self_qubits[i];
            }
            /* b-register = ret[j..j+width-1] (accumulator, shifted, modified) */
            for (int i = 0; i < width; i++) {
                tqa[width + i] = ret_qubits[j + i];
            }
            /* carry ancilla */
            tqa[2 * width] = ancilla;
            /* control qubit = other[j] (multiplier bit with weight 2^j) */
            tqa[2 * width + 1] = other_qubits[j];

            toff_seq = toffoli_cQQ_add(width);
            if (toff_seq == NULL) {
                allocator_free(circ->allocator, ancilla, 1);
                return;
            }
            run_instruction(toff_seq, tqa, 0, circ);
            allocator_free(circ->allocator, ancilla, 1);
        }
    }
}

/**
 * @brief Toffoli schoolbook CQ multiplication: ret = self * classical_value.
 *
 * Decomposes classical_value into binary. For each set bit j (weight 2^j),
 * performs an uncontrolled addition of self[0..width-1] into ret[j..j+width-1].
 *
 * Only adds for set bits of the classical value (compile-time decision),
 * so uses cheaper uncontrolled CDKM adders (toffoli_QQ_add).
 *
 * LSB-first convention: index 0 = LSB, index n-1 = MSB.
 *
 * @param circ            Active circuit
 * @param ret_qubits      Result register (accumulator, starts at |0>)
 * @param ret_bits        Width of result register (n)
 * @param self_qubits     Multiplicand register (preserved)
 * @param self_bits       Width of multiplicand
 * @param classical_value Classical integer to multiply by
 */
void toffoli_mul_cq(circuit_t *circ, const unsigned int *ret_qubits, int ret_bits,
                    const unsigned int *self_qubits, int self_bits, int64_t classical_value) {
    int n = ret_bits;

    /* Convert classical value to binary (MSB-first: bin[0]=MSB, bin[n-1]=LSB) */
    int *bin = two_complement(classical_value, n);
    if (bin == NULL)
        return;

    for (int j = 0; j < n; j++) {
        /* bin is MSB-first: bin[0]=MSB, bin[n-1]=LSB
         * Bit j (weight 2^j) is at bin[n-1-j] */
        if (bin[n - 1 - j] == 0)
            continue; /* Skip zero bits */

        int width = n - j;
        if (width < 1)
            break;

        unsigned int tqa[256];
        sequence_t *toff_seq;

        if (width == 1) {
            /* 1-bit uncontrolled addition: CNOT
             * toffoli_QQ_add(1) layout: [0]=target, [1]=source
             *
             * LSB-first: ret[n-1] = MSB of ret, self[0] = LSB of self */
            tqa[0] = ret_qubits[n - 1];
            tqa[1] = self_qubits[0];

            toff_seq = toffoli_QQ_add(1);
            if (toff_seq == NULL) {
                free(bin);
                return;
            }
            run_instruction(toff_seq, tqa, 0, circ);
        } else {
            /* General case: uncontrolled addition of width bits */
            qubit_t ancilla = allocator_alloc(circ->allocator, 1, true);
            if (ancilla == (qubit_t)-1) {
                free(bin);
                return;
            }

            /* CDKM convention (matching hot_path_add.c):
             *   a-register [0..width-1]       = source (preserved)
             *   b-register [width..2*width-1]  = target (gets sum)
             *   [2*width]                      = carry ancilla
             *
             * For classical bit j (weight 2^j):
             *   self slice: self[0..width-1] (the lower width=n-j bits of multiplicand)
             *   ret slice:  ret[j..j+width-1] (shifted by j positions in result)
             */

            /* a-register = self[0..width-1] (multiplicand lower bits, preserved) */
            for (int i = 0; i < width; i++) {
                tqa[i] = self_qubits[i];
            }
            /* b-register = ret[j..j+width-1] (accumulator, shifted, modified) */
            for (int i = 0; i < width; i++) {
                tqa[width + i] = ret_qubits[j + i];
            }
            /* carry ancilla */
            tqa[2 * width] = ancilla;

            toff_seq = toffoli_QQ_add(width);
            if (toff_seq == NULL) {
                allocator_free(circ->allocator, ancilla, 1);
                free(bin);
                return;
            }
            run_instruction(toff_seq, tqa, 0, circ);
            allocator_free(circ->allocator, ancilla, 1);
        }
    }

    free(bin);
}
