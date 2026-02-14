/**
 * @file ToffoliAddition.c
 * @brief CDKM ripple-carry adder implementation (Phase 66).
 *
 * Implements the Cuccaro-Draper-Kutin-Moulton (CDKM) ripple-carry adder
 * using MAJ (Majority) and UMA (UnMajority-and-Add) gate chains.
 *
 * The CDKM adder uses only Toffoli (CCX) and CNOT (CX) gates, making it
 * suitable for fault-tolerant quantum computation where T-gate count matters.
 *
 * References:
 *   Cuccaro et al., "A new quantum ripple-carry addition circuit" (2004)
 *   arXiv:quant-ph/0410184
 */

#include "Integer.h"
#include "gate.h"
#include "toffoli_arithmetic_ops.h"
#include <stdint.h>
#include <stdlib.h>

// ============================================================================
// Precompiled cache for QQ Toffoli addition (separate from QFT cache)
// ============================================================================
static sequence_t *precompiled_toffoli_QQ_add[65] = {NULL};

// No cache for CQ: value-dependent sequences, generated fresh each call.

// ============================================================================
// MAJ / UMA helper functions
// ============================================================================

/**
 * @brief Emit MAJ (Majority) gate triplet.
 *
 * MAJ(a, b, c):
 *   1. CNOT(target=b, control=c)     -- b ^= c
 *   2. CNOT(target=a, control=c)     -- a ^= c
 *   3. Toffoli(target=c, ctrl=a, ctrl=b) -- c ^= (a AND b)
 *
 * After MAJ: c holds the carry, a and b are in superposition.
 *
 * @param seq   Sequence to emit gates into
 * @param layer Pointer to current layer index (incremented by 3)
 * @param a     Qubit index for 'a' (carry-in / previous carry)
 * @param b     Qubit index for 'b' (source bit)
 * @param c     Qubit index for 'c' (target bit, becomes carry-out)
 */
static void emit_MAJ(sequence_t *seq, int *layer, int a, int b, int c) {
    // Step 1: CNOT(target=b, control=c)
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], b, c);
    (*layer)++;

    // Step 2: CNOT(target=a, control=c)
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], a, c);
    (*layer)++;

    // Step 3: Toffoli(target=c, ctrl1=a, ctrl2=b)
    ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], c, a, b);
    (*layer)++;
}

/**
 * @brief Emit UMA (UnMajority-and-Add) gate triplet.
 *
 * UMA(a, b, c):
 *   1. Toffoli(target=c, ctrl=a, ctrl=b) -- undoes MAJ's Toffoli
 *   2. CNOT(target=a, control=c)          -- restores a
 *   3. CNOT(target=b, control=a)          -- b = sum bit
 *
 * After UMA: a is restored, b holds the sum bit, c is restored.
 *
 * @param seq   Sequence to emit gates into
 * @param layer Pointer to current layer index (incremented by 3)
 * @param a     Qubit index for 'a'
 * @param b     Qubit index for 'b' (becomes sum bit)
 * @param c     Qubit index for 'c'
 */
static void emit_UMA(sequence_t *seq, int *layer, int a, int b, int c) {
    // Step 1: Toffoli(target=c, ctrl1=a, ctrl2=b)
    ccx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], c, a, b);
    (*layer)++;

    // Step 2: CNOT(target=a, control=c)
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], a, c);
    (*layer)++;

    // Step 3: CNOT(target=b, control=a)
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], b, a);
    (*layer)++;
}

// ============================================================================
// CQ MAJ/UMA helpers (classical bit simplification)
// ============================================================================

/**
 * @brief Emit MAJ for CQ when classical bit = 0.
 *
 * MAJ(carry, 0, a[i]) simplifies to:
 *   CNOT(target=carry, control=a[i])  -- carry ^= a[i]
 * (CNOT(b,c) is identity since b is classical 0, and Toffoli with b=0 is identity)
 */
static void emit_MAJ_CQ_zero(sequence_t *seq, int *layer, int carry, int a_i) {
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], carry, a_i);
    (*layer)++;
}

/**
 * @brief Emit MAJ for CQ when classical bit = 1.
 *
 * MAJ(carry, 1, a[i]) simplifies to:
 *   X(a[i])                              -- a[i] ^= 1 (CNOT with b=1)
 *   CNOT(target=carry, control=a[i])     -- carry ^= a[i]
 *   CNOT(target=a[i], control=carry)     -- a[i] ^= carry (Toffoli with b=1)
 */
static void emit_MAJ_CQ_one(sequence_t *seq, int *layer, int carry, int a_i) {
    // X(a[i])
    x(&seq->seq[*layer][seq->gates_per_layer[*layer]++], a_i);
    (*layer)++;

    // CNOT(target=carry, control=a[i])
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], carry, a_i);
    (*layer)++;

    // CNOT(target=a[i], control=carry)
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], a_i, carry);
    (*layer)++;
}

/**
 * @brief Emit UMA for CQ when classical bit = 0.
 *
 * UMA(carry, 0, a[i]) simplifies to:
 *   CNOT(target=carry, control=a[i])  -- carry ^= a[i]
 * (Toffoli with b=0 is identity, and CNOT(b,a) with b=0 is identity)
 */
static void emit_UMA_CQ_zero(sequence_t *seq, int *layer, int carry, int a_i) {
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], carry, a_i);
    (*layer)++;
}

/**
 * @brief Emit UMA for CQ when classical bit = 1.
 *
 * UMA(carry, 1, a[i]) simplifies to:
 *   CNOT(target=a[i], control=carry)     -- Toffoli with b=1 = CNOT
 *   CNOT(target=carry, control=a[i])     -- carry ^= a[i]
 *   X(carry)                              -- carry ^= 1 (CNOT with b=1)
 */
static void emit_UMA_CQ_one(sequence_t *seq, int *layer, int carry, int a_i) {
    // CNOT(target=a[i], control=carry)
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], a_i, carry);
    (*layer)++;

    // CNOT(target=carry, control=a[i])
    cx(&seq->seq[*layer][seq->gates_per_layer[*layer]++], carry, a_i);
    (*layer)++;

    // X(carry)
    x(&seq->seq[*layer][seq->gates_per_layer[*layer]++], carry);
    (*layer)++;
}

// ============================================================================
// Sequence allocation helper
// ============================================================================

/**
 * @brief Allocate a sequence with given number of layers, 1 gate per layer.
 */
static sequence_t *alloc_sequence(int num_layers) {
    sequence_t *seq = malloc(sizeof(sequence_t));
    if (seq == NULL)
        return NULL;

    seq->num_layer = num_layers;
    seq->used_layer = 0;
    seq->gates_per_layer = calloc(num_layers, sizeof(num_t));
    if (seq->gates_per_layer == NULL) {
        free(seq);
        return NULL;
    }

    seq->seq = calloc(num_layers, sizeof(gate_t *));
    if (seq->seq == NULL) {
        free(seq->gates_per_layer);
        free(seq);
        return NULL;
    }

    for (int i = 0; i < num_layers; i++) {
        seq->seq[i] = calloc(1, sizeof(gate_t));
        if (seq->seq[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(seq->seq[j]);
            }
            free(seq->seq);
            free(seq->gates_per_layer);
            free(seq);
            return NULL;
        }
    }

    return seq;
}

// ============================================================================
// Public API
// ============================================================================

sequence_t *toffoli_QQ_add(int bits) {
    // OWNERSHIP: Returns cached sequence - DO NOT FREE
    //
    // Qubit layout for toffoli_QQ_add(bits):
    //   [0..bits-1]       = register a (target, modified in place: a += b)
    //   [bits..2*bits-1]   = register b (source, unchanged)
    //   [2*bits]           = ancilla carry (bits >= 2 only)

    // Bounds check
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    // Check cache
    if (precompiled_toffoli_QQ_add[bits] != NULL) {
        return precompiled_toffoli_QQ_add[bits];
    }

    // 1-bit special case: single CNOT, no ancilla
    if (bits == 1) {
        sequence_t *seq = alloc_sequence(1);
        if (seq == NULL)
            return NULL;

        // a[0] ^= b[0]: CNOT(target=0, control=1)
        cx(&seq->seq[0][seq->gates_per_layer[0]++], 0, 1);
        seq->used_layer = 1;

        precompiled_toffoli_QQ_add[bits] = seq;
        return seq;
    }

    // General case (bits >= 2): CDKM ripple-carry adder
    // Forward sweep: n MAJ calls (3n layers)
    // Reverse sweep: n UMA calls (3n layers)
    // Total: 6n layers
    int num_layers = 6 * bits;

    sequence_t *seq = alloc_sequence(num_layers);
    if (seq == NULL)
        return NULL;

    int layer = 0;
    int ancilla = 2 * bits; // ancilla carry qubit index

    // Forward MAJ sweep
    // First: MAJ(ancilla, b[0], a[0])
    emit_MAJ(seq, &layer, ancilla, bits + 0, 0);

    // Remaining: MAJ(a[i-1], b[i], a[i]) for i = 1..bits-1
    for (int i = 1; i < bits; i++) {
        emit_MAJ(seq, &layer, i - 1, bits + i, i);
    }

    // Reverse UMA sweep
    // First (innermost): UMA(a[bits-2], b[bits-1], a[bits-1])
    for (int i = bits - 1; i >= 1; i--) {
        emit_UMA(seq, &layer, i - 1, bits + i, i);
    }

    // Last: UMA(ancilla, b[0], a[0])
    emit_UMA(seq, &layer, ancilla, bits + 0, 0);

    seq->used_layer = layer;

    // Cache and return
    precompiled_toffoli_QQ_add[bits] = seq;
    return seq;
}

sequence_t *toffoli_CQ_add(int bits, int64_t value) {
    // OWNERSHIP: Caller owns returned sequence, must free via toffoli_sequence_free()
    //
    // Qubit layout for toffoli_CQ_add(bits, value):
    //   [0..bits-1]  = register a (target, modified in place: a += value)
    //   [bits]       = ancilla carry (bits >= 2 only)

    // Bounds check
    if (bits < 1 || bits > 64) {
        return NULL;
    }

    // Convert value to binary (MSB-first: bin[0]=MSB, bin[bits-1]=LSB)
    int *bin = two_complement(value, bits);
    if (bin == NULL) {
        return NULL;
    }

    // 1-bit special case
    if (bits == 1) {
        if (bin[0] == 1) {
            // X(target=0)
            sequence_t *seq = alloc_sequence(1);
            if (seq == NULL) {
                free(bin);
                return NULL;
            }
            x(&seq->seq[0][seq->gates_per_layer[0]++], 0);
            seq->used_layer = 1;
            free(bin);
            return seq;
        } else {
            // Identity: 0-layer sequence
            sequence_t *seq = malloc(sizeof(sequence_t));
            if (seq == NULL) {
                free(bin);
                return NULL;
            }
            seq->num_layer = 0;
            seq->used_layer = 0;
            seq->gates_per_layer = NULL;
            seq->seq = NULL;
            free(bin);
            return seq;
        }
    }

    // General case (bits >= 2): CQ CDKM adder
    // Count layers: for each bit position, +6 if classical bit=1, +2 if bit=0
    // (3 MAJ gates + 3 UMA gates for bit=1; 1 MAJ gate + 1 UMA gate for bit=0)
    int num_layers = 0;
    for (int i = 0; i < bits; i++) {
        int classical_bit = bin[bits - 1 - i]; // i=0 is LSB
        if (classical_bit == 1) {
            num_layers += 6;
        } else {
            num_layers += 2;
        }
    }

    if (num_layers == 0) {
        // All classical bits are 0 -- but wait, each 0 bit contributes 2 layers
        // This can't happen for bits >= 2. Safety fallback.
        sequence_t *seq = malloc(sizeof(sequence_t));
        if (seq == NULL) {
            free(bin);
            return NULL;
        }
        seq->num_layer = 0;
        seq->used_layer = 0;
        seq->gates_per_layer = NULL;
        seq->seq = NULL;
        free(bin);
        return seq;
    }

    sequence_t *seq = alloc_sequence(num_layers);
    if (seq == NULL) {
        free(bin);
        return NULL;
    }

    int layer = 0;
    int ancilla = bits; // ancilla carry qubit index

    // Forward MAJ sweep (CQ variant)
    // For i=0 (LSB): carry = ancilla, a[i] = index i
    // For i=1..bits-1: carry = a[i-1], a[i] = index i
    for (int i = 0; i < bits; i++) {
        int carry = (i == 0) ? ancilla : (i - 1);
        int a_i = i;
        int classical_bit = bin[bits - 1 - i]; // i=0 is LSB

        if (classical_bit == 1) {
            emit_MAJ_CQ_one(seq, &layer, carry, a_i);
        } else {
            emit_MAJ_CQ_zero(seq, &layer, carry, a_i);
        }
    }

    // Reverse UMA sweep (CQ variant)
    for (int i = bits - 1; i >= 0; i--) {
        int carry = (i == 0) ? ancilla : (i - 1);
        int a_i = i;
        int classical_bit = bin[bits - 1 - i]; // i=0 is LSB

        if (classical_bit == 1) {
            emit_UMA_CQ_one(seq, &layer, carry, a_i);
        } else {
            emit_UMA_CQ_zero(seq, &layer, carry, a_i);
        }
    }

    seq->used_layer = layer;

    free(bin);
    return seq;
}

void toffoli_sequence_free(sequence_t *seq) {
    if (seq == NULL)
        return;

    if (seq->seq != NULL) {
        for (num_t i = 0; i < seq->num_layer; i++) {
            free(seq->seq[i]);
        }
        free(seq->seq);
    }

    free(seq->gates_per_layer);
    free(seq);
}
