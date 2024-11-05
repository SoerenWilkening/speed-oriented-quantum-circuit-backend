//
// Created by Sören Wilkening on 05.11.24.
//

#include "../include/LogicOperations.h"

sequence_t *not_seq() {
    int number = INTEGERSIZE;
    if (stack.GPR1[0].type == BOOL) number = 1;

    sequence_t *seq = malloc(sizeof(sequence_t));
    seq->gates_per_layer = malloc(sizeof(num_t));
    seq->gates_per_layer[0] = number;
    seq->seq = malloc(sizeof(gate_t *));
    seq->seq[0] = malloc(number * sizeof(gate_t));
    seq->used_layer = 1;
    seq->num_layer = 1;
    for (int i = 0; i < number; ++i) {
        x(&seq->seq[0][i], i);
    }
    return seq;
}