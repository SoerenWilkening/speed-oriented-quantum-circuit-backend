//
// Created by Sören Wilkening on 26.10.24.
//
#include "QPU.h"
#include <stdint.h>

quantum_int_t *QBOOL() {
    quantum_int_t *integer = malloc(sizeof(quantum_int_t));
    if (integer == NULL) {
        return NULL;
    }

    integer->MSB = INTEGERSIZE - 1;
    integer->q_address[INTEGERSIZE - 1] = circuit->used_qubit_indices;

    circuit->ancilla += 1;
    circuit->used_qubit_indices += 1;
    return integer;
}

quantum_int_t *QINT() {
    quantum_int_t *integer = malloc(sizeof(quantum_int_t));
    if (integer == NULL) {
        return NULL;
    }

    integer->MSB = 0;
    circuit->ancilla += INTEGERSIZE;

    for (int i = 0; i < INTEGERSIZE; ++i) {
        integer->q_address[i] = circuit->used_qubit_indices;
        circuit->used_qubit_indices++;
    }
    return integer;
}

quantum_int_t *INT(int64_t intg) {
    quantum_int_t *integer = malloc(sizeof(quantum_int_t));
    if (integer == NULL) {
        return NULL;
    }
    //    integer->c_address = malloc(sizeof(int64_t));
    //    *integer->c_address = intg;
    return integer;
}
quantum_int_t *BOOL(bool intg) {
    quantum_int_t *integer = malloc(sizeof(quantum_int_t));
    if (integer == NULL) {
        return NULL;
    }
    //    integer->c_address = malloc(sizeof(int64_t));
    //    *integer->c_address = intg;
    return integer;
}
quantum_int_t *bit_of_int(quantum_int_t *el1, int bit) {
    quantum_int_t *b = malloc(sizeof(quantum_int_t));
    if (b == NULL) {
        return NULL;
    }
    b->MSB = INTEGERSIZE - 1;
    b->q_address[INTEGERSIZE - 1] = el1->q_address[bit];
    return b;
}

// Function to compute n-bit two's complement bit representation of x
int *two_complement(int64_t x, int n) {
    // Print the n-bit two's complement bit representation
    uint64_t mask = 1ULL << (n - 1); // Start from the highest bit
    int *bin = calloc(n, sizeof(int));
    if (bin == NULL) {
        return NULL;
    }

    for (int i = 0; i < n; ++i) {
        // Check each bit using mask and print '1' or '0'
        bin[i] = (x & mask) ? 1 : 0;
        mask >>= 1; // Shift the mask right
    }
    return bin;
}

void free_element(quantum_int_t *el1) {
    circuit->ancilla--;
    circuit->used_qubit_indices--;
}

sequence_t *setting_seq() {
    int *bin = two_complement(*QPU_state->R0, INTEGERSIZE);
    if (bin == NULL) {
        return NULL;
    }
    sequence_t *seq;
    seq = malloc(sizeof(sequence_t));
    if (seq == NULL) {
        free(bin);
        return NULL;
    }
    seq->used_layer = 1;
    seq->num_layer = 1;
    seq->gates_per_layer = calloc(1, sizeof(num_t));
    if (seq->gates_per_layer == NULL) {
        free(bin);
        free(seq);
        return NULL;
    }
    seq->seq = calloc(1, sizeof(gate_t *));
    if (seq->seq == NULL) {
        free(seq->gates_per_layer);
        free(bin);
        free(seq);
        return NULL;
    }
    seq->seq[0] = calloc(INTEGERSIZE, sizeof(gate_t));
    if (seq->seq[0] == NULL) {
        free(seq->seq);
        free(seq->gates_per_layer);
        free(bin);
        free(seq);
        return NULL;
    }
    seq->gates_per_layer[0] = 0;
    for (int i = QPU_state->Q0->MSB; i < INTEGERSIZE; ++i) {
        if (bin[i])
            x(&seq->seq[0][seq->gates_per_layer[0]++], i);
    }

    free(bin);
    return seq;
}