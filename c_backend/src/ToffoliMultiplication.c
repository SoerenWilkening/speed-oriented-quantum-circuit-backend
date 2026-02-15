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
 * performs a controlled addition of self[0..n-1-j] into ret[j..n-1],
 * controlled by other[j].
 *
 * The shift is implicit: adding into ret[j..] effectively multiplies by 2^j.
 * Each iteration uses progressively smaller addition width (n-j).
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
            /* 1-bit controlled addition: CCX(target=ret[n-1], ctrl1=self[0], ctrl2=other[j])
             * toffoli_cQQ_add(1) layout: [0]=target, [1]=source, [2]=control
             * target = ret[n-1] (accumulator bit), source = self[0], control = other[j] */
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
             *   a-register [0..width-1]       = self (multiplicand, preserved)
             *   b-register [width..2*width-1]  = ret slice (accumulator, gets sum)
             *   [2*width]                      = carry ancilla
             *   [2*width+1]                    = control qubit (other[j])
             *
             * toffoli_cQQ_add modifies the b-register (positions [width..2*width-1]).
             * The a-register (positions [0..width-1]) is preserved.
             */

            /* a-register = self[0..width-1] (multiplicand, preserved) */
            for (int i = 0; i < width; i++) {
                tqa[i] = self_qubits[i];
            }
            /* b-register = ret[j..j+width-1] (accumulator, modified) */
            for (int i = 0; i < width; i++) {
                tqa[width + i] = ret_qubits[j + i];
            }
            /* carry ancilla */
            tqa[2 * width] = ancilla;
            /* control qubit = other[j] (multiplier bit j) */
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
 * Decomposes classical_value into binary. For each set bit j, performs an
 * uncontrolled addition of self[0..n-1-j] into ret[j..n-1].
 *
 * Only adds for set bits of the classical value (compile-time decision),
 * so uses cheaper uncontrolled CDKM adders (toffoli_QQ_add).
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
            /* 1-bit uncontrolled addition: CNOT(target=ret[n-1], control=self[0])
             * toffoli_QQ_add(1) layout: [0]=target, [1]=source */
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
             *   a-register [0..width-1]       = self (multiplicand, preserved)
             *   b-register [width..2*width-1]  = ret slice (accumulator, gets sum)
             *   [2*width]                      = carry ancilla
             */

            /* a-register = self[0..width-1] (multiplicand, preserved) */
            for (int i = 0; i < width; i++) {
                tqa[i] = self_qubits[i];
            }
            /* b-register = ret[j..j+width-1] (accumulator, modified) */
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
