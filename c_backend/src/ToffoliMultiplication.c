/**
 * @file ToffoliMultiplication.c
 * @brief Toffoli-based schoolbook multiplication (Phase 68, 69).
 *
 * Implements shift-and-add multiplication using the CDKM ripple-carry adders
 * from ToffoliAddition.c as subroutines. Each multiplier bit controls whether
 * a shifted copy of the multiplicand is added to the result accumulator.
 *
 * Four variants:
 *   toffoli_mul_qq   -- quantum * quantum (controlled additions per multiplier bit)
 *   toffoli_mul_cq   -- quantum * classical (uncontrolled additions for set bits)
 *   toffoli_cmul_qq  -- controlled quantum * quantum (AND-ancilla + cQQ adder)
 *   toffoli_cmul_cq  -- controlled quantum * classical (cQQ adder with ext_ctrl)
 *
 * Qubit arrays use LSB-first convention from the C hot path perspective:
 *   index 0 = LSB (weight 2^0), index n-1 = MSB (weight 2^(n-1)).
 * This matches the CDKM adder's internal convention.
 *
 * References:
 *   Schoolbook multiplication: sum_{j=0}^{n-1} a * b[j] * 2^j
 *   CDKM adder: Cuccaro et al., arXiv:quant-ph/0410184
 *   AND-ancilla pattern: Beauregard (2003), Haner et al. (2018)
 */

#include "Integer.h"
#include "QPU.h"
#include "execution.h"
#include "gate.h"
#include "optimizer.h"
#include "qubit_allocator.h"
#include "toffoli_arithmetic_ops.h"

#include <stdlib.h>
#include <string.h>

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

// ============================================================================
// Controlled Toffoli Multiplication (Phase 69)
// ============================================================================

/**
 * @brief Controlled Toffoli QQ multiplication: ret = self * other, controlled by ext_ctrl.
 *
 * Uses the AND-ancilla pattern: for each multiplier bit other[j], computes
 * and_ancilla = other[j] AND ext_ctrl via CCX, then uses and_ancilla as the
 * single control for toffoli_cQQ_add. Uncomputes AND after each adder call
 * (CCX is self-inverse; other[j] and ext_ctrl are preserved by the adder).
 *
 * For width 1 (j == n-1): emits a single MCX with 3 controls
 * (self[0], other[j], ext_ctrl) targeting ret[n-1], avoiding the AND-ancilla
 * overhead for the trivial case.
 *
 * Ancilla budget: 1 carry + 1 AND = 2 ancilla allocated before loop, freed after.
 *
 * @param circ          Active circuit
 * @param ret_qubits    Result register (accumulator, starts at |0>)
 * @param ret_bits      Width of result register (n)
 * @param self_qubits   Multiplicand register (preserved)
 * @param self_bits     Width of multiplicand
 * @param other_qubits  Multiplier register (preserved)
 * @param other_bits    Width of multiplier
 * @param ext_ctrl      External control qubit
 */
void toffoli_cmul_qq(circuit_t *circ, const unsigned int *ret_qubits, int ret_bits,
                     const unsigned int *self_qubits, int self_bits,
                     const unsigned int *other_qubits, int other_bits, unsigned int ext_ctrl) {
    int n = ret_bits;

    /* Allocate 1 carry ancilla (reused per iteration, CDKM returns to |0>) */
    qubit_t carry = allocator_alloc(circ->allocator, 1, true);
    if (carry == (qubit_t)-1)
        return;

    /* Allocate 1 AND ancilla (reused per iteration, uncomputed after each adder) */
    qubit_t and_anc = allocator_alloc(circ->allocator, 1, true);
    if (and_anc == (qubit_t)-1) {
        allocator_free(circ->allocator, carry, 1);
        return;
    }

    for (int j = 0; j < n; j++) {
        int width = n - j; /* Addition width for this iteration */
        if (width < 1)
            break;

        if (width == 1) {
            /* 1-bit controlled multiplication: MCX with 3 controls.
             * ret[n-1] ^= self[0] AND other[j] AND ext_ctrl.
             *
             * Emit a single MCX gate directly via add_gate.
             * mcx() allocates large_control; circuit takes ownership. */
            gate_t g;
            memset(&g, 0, sizeof(gate_t));
            qubit_t *large_control = malloc(3 * sizeof(qubit_t));
            if (large_control == NULL) {
                allocator_free(circ->allocator, and_anc, 1);
                allocator_free(circ->allocator, carry, 1);
                return;
            }
            large_control[0] = self_qubits[0];
            large_control[1] = other_qubits[j];
            large_control[2] = ext_ctrl;
            mcx(&g, ret_qubits[n - 1], large_control, 3);
            add_gate(circ, &g);
            /* Do NOT free large_control -- circuit takes ownership (Phase 67 pattern) */
        } else {
            /* General case: AND-ancilla pattern for controlled addition.
             *
             * Step 1: Compute AND: and_anc = other[j] AND ext_ctrl
             *         CCX(target=and_anc, ctrl1=other_qubits[j], ctrl2=ext_ctrl) */
            gate_t g;
            memset(&g, 0, sizeof(gate_t));
            ccx(&g, and_anc, other_qubits[j], ext_ctrl);
            add_gate(circ, &g);

            /* Step 2: Build tqa for toffoli_cQQ_add(width) with and_anc as control.
             *   a-register [0..width-1]       = self[0..width-1] (source, preserved)
             *   b-register [width..2*width-1]  = ret[j..j+width-1] (accumulator)
             *   [2*width]                      = carry ancilla
             *   [2*width+1]                    = and_anc (control for cQQ_add) */
            unsigned int tqa[256];
            for (int i = 0; i < width; i++) {
                tqa[i] = self_qubits[i];
            }
            for (int i = 0; i < width; i++) {
                tqa[width + i] = ret_qubits[j + i];
            }
            tqa[2 * width] = carry;
            tqa[2 * width + 1] = and_anc;

            sequence_t *toff_seq = toffoli_cQQ_add(width);
            if (toff_seq == NULL) {
                allocator_free(circ->allocator, and_anc, 1);
                allocator_free(circ->allocator, carry, 1);
                return;
            }
            run_instruction(toff_seq, tqa, 0, circ);

            /* Step 3: Uncompute AND: and_anc = 0 (CCX is self-inverse).
             * Safe because other[j] and ext_ctrl are preserved by cQQ_add. */
            memset(&g, 0, sizeof(gate_t));
            ccx(&g, and_anc, other_qubits[j], ext_ctrl);
            add_gate(circ, &g);
        }
    }

    allocator_free(circ->allocator, and_anc, 1);
    allocator_free(circ->allocator, carry, 1);
}

/**
 * @brief Controlled Toffoli CQ multiplication: ret = self * classical_value, controlled.
 *
 * For each set bit j of classical_value (weight 2^j), performs a controlled
 * addition of self[0..width-1] into ret[j..j+width-1], controlled by ext_ctrl.
 *
 * Uses toffoli_cQQ_add with ext_ctrl mapped to the control slot. No AND-ancilla
 * needed because the classical bit selection is compile-time; only the runtime
 * external control needs to gate each addition.
 *
 * For width 1: uses toffoli_cQQ_add(1) layout [target, source, ext_ctrl] which
 * emits a CCX.
 *
 * @param circ            Active circuit
 * @param ret_qubits      Result register (accumulator, starts at |0>)
 * @param ret_bits        Width of result register (n)
 * @param self_qubits     Multiplicand register (preserved)
 * @param self_bits       Width of multiplicand
 * @param classical_value Classical integer to multiply by
 * @param ext_ctrl        External control qubit
 */
void toffoli_cmul_cq(circuit_t *circ, const unsigned int *ret_qubits, int ret_bits,
                     const unsigned int *self_qubits, int self_bits, int64_t classical_value,
                     unsigned int ext_ctrl) {
    int n = ret_bits;

    /* Convert classical value to binary (MSB-first: bin[0]=MSB, bin[n-1]=LSB) */
    int *bin = two_complement(classical_value, n);
    if (bin == NULL)
        return;

    /* Allocate 1 carry ancilla before the loop (reused, CDKM returns to |0>) */
    qubit_t carry = allocator_alloc(circ->allocator, 1, true);
    if (carry == (qubit_t)-1) {
        free(bin);
        return;
    }

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
            /* 1-bit controlled addition: CCX via toffoli_cQQ_add(1).
             * toffoli_cQQ_add(1) layout: [0]=target, [1]=source, [2]=control
             * target=ret[n-1], source=self[0], control=ext_ctrl */
            tqa[0] = ret_qubits[n - 1];
            tqa[1] = self_qubits[0];
            tqa[2] = ext_ctrl;

            toff_seq = toffoli_cQQ_add(1);
            if (toff_seq == NULL) {
                allocator_free(circ->allocator, carry, 1);
                free(bin);
                return;
            }
            run_instruction(toff_seq, tqa, 0, circ);
        } else {
            /* General case: controlled addition using toffoli_cQQ_add.
             *   a-register [0..width-1]       = self[0..width-1] (source, preserved)
             *   b-register [width..2*width-1]  = ret[j..j+width-1] (accumulator)
             *   [2*width]                      = carry ancilla
             *   [2*width+1]                    = ext_ctrl (external control) */
            for (int i = 0; i < width; i++) {
                tqa[i] = self_qubits[i];
            }
            for (int i = 0; i < width; i++) {
                tqa[width + i] = ret_qubits[j + i];
            }
            tqa[2 * width] = carry;
            tqa[2 * width + 1] = ext_ctrl;

            toff_seq = toffoli_cQQ_add(width);
            if (toff_seq == NULL) {
                allocator_free(circ->allocator, carry, 1);
                free(bin);
                return;
            }
            run_instruction(toff_seq, tqa, 0, circ);
        }
    }

    allocator_free(circ->allocator, carry, 1);
    free(bin);
}
