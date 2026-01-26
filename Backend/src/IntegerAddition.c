//
// Created by Sören Wilkening on 05.11.24.
//

#include "Integer.h"

sequence_t *precompiled_QQ_add = NULL;
sequence_t *precompiled_cQQ_add = NULL;
sequence_t *precompiled_CQ_add[64] = {NULL};
sequence_t *precompiled_cCQ_add[64] = {NULL};

sequence_t *CC_add() {
    // OWNERSHIP: No sequence returned (performs classical computation only)
    *(QPU_state->R0) += *(QPU_state->R1);
    return NULL;
}
sequence_t *CQ_add(int bits) {
    // OWNERSHIP: Returns cached sequence (precompiled_CQ_add[bits]) - DO NOT FREE
    // READS: QPU_state->R0 for classical value
    // The precompiled sequence is reused across calls for performance
    int offset = INTEGERSIZE - bits;

    // Compute rotation angles
    int NonZeroCount = 0;
    int *bin = two_complement(*(QPU_state->R0), bits);
    if (bin == NULL) {
        return NULL;
    }

    // Compute rotations for addition
    double *rotations = calloc(bits, sizeof(double));
    if (rotations == NULL) {
        free(bin);
        return NULL;
    }
    for (int i = 0; i < bits; ++i) {
        for (int j = 0; j < bits - i; ++j) {
            rotations[j + i] += bin[bits - i - 1] * 2 * M_PI / (pow(2, j + 1));
        }
        if (rotations[i] != 0)
            NonZeroCount++;
    }
    free(bin);
    int start_layer = bits;

    if (precompiled_CQ_add[bits] != NULL) {
        sequence_t *add = precompiled_CQ_add[bits];

        for (int i = 0; i < bits; ++i) {
            add->seq[start_layer + i][add->gates_per_layer[start_layer + i] - 1].GateValue =
                rotations[bits - i - 1];
        }
        free(rotations);
        return add;
    }

    sequence_t *add = malloc(sizeof(sequence_t));
    if (add == NULL) {
        free(rotations);
        return NULL;
    }
    // allocate exact number of layer and enough gates per layer
    add->used_layer = 0;
    add->num_layer = 4 * bits - 1;
    add->gates_per_layer = calloc(add->num_layer, sizeof(num_t));
    if (add->gates_per_layer == NULL) {
        free(rotations);
        free(add);
        return NULL;
    }
    memset(add->gates_per_layer, 0, add->num_layer * sizeof(num_t));
    add->seq = calloc(add->num_layer, sizeof(gate_t *));
    if (add->seq == NULL) {
        free(add->gates_per_layer);
        free(rotations);
        free(add);
        return NULL;
    }
    for (int i = 0; i < add->num_layer; ++i) {
        add->seq[i] = calloc(2 * bits, sizeof(gate_t));
        if (add->seq[i] == NULL) {
            for (int j = 0; j < i; ++j) {
                free(add->seq[j]);
            }
            free(add->seq);
            free(add->gates_per_layer);
            free(rotations);
            free(add);
            return NULL;
        }
    }
    QFT(add, bits);

    for (int i = 0; i < bits; ++i) {
        p(&add->seq[start_layer + i][add->gates_per_layer[start_layer + i]++], offset + i,
          rotations[bits - i - 1]);
    }
    free(rotations);
    add->used_layer++;

    QFT_inverse(add, bits);

    precompiled_CQ_add[bits] = add;
    return add;
}
sequence_t *QQ_add() {
    // OWNERSHIP: Returns cached sequence (precompiled_QQ_add) - DO NOT FREE
    // The precompiled sequence is reused across calls for performance
    //    printf("check\n");
    //    fflush(stdout);
    if (precompiled_QQ_add != NULL)
        return precompiled_QQ_add;
    //	printf("not precompiled\n");
    //    fflush(stdout);

    sequence_t *add = malloc(sizeof(sequence_t));
    if (add == NULL) {
        return NULL;
    }

    // allocate exact number of layer and enough gates per layer
    add->used_layer = 0;
    add->num_layer = 5 * INTEGERSIZE - 2;
    add->gates_per_layer = calloc(add->num_layer, sizeof(num_t));
    if (add->gates_per_layer == NULL) {
        free(add);
        return NULL;
    }
    memset(add->gates_per_layer, 0, add->num_layer * sizeof(num_t));
    add->seq = calloc(add->num_layer, sizeof(gate_t *));
    if (add->seq == NULL) {
        free(add->gates_per_layer);
        free(add);
        return NULL;
    }
    for (int i = 0; i < add->num_layer; ++i) {
        add->seq[i] = calloc(2 * INTEGERSIZE, sizeof(gate_t));
        if (add->seq[i] == NULL) {
            for (int j = 0; j < i; ++j) {
                free(add->seq[j]);
            }
            free(add->seq);
            free(add->gates_per_layer);
            free(add);
            return NULL;
        }
    }
    QFT(add, INTEGERSIZE);
    int rounds = 0;
    for (int bit = (int)INTEGERSIZE - 1; bit >= 0; --bit) {
        for (int i = 0; i < INTEGERSIZE - rounds; ++i) {
            num_t layer = 2 * INTEGERSIZE + i + 2 * rounds - 1;
            num_t target = INTEGERSIZE - i - 1 - rounds;
            num_t control = INTEGERSIZE + bit;
            double value = 2 * M_PI / (pow(2, i + 1));
            gate_t *g = &add->seq[layer][add->gates_per_layer[layer]++];
            cp(g, target, control, value);
        }
        rounds++;
    }
    //	printf("build\n");
    //    fflush(stdout);
    add->used_layer += INTEGERSIZE;
    QFT_inverse(add, INTEGERSIZE);
    //	printf("qft inverted\n");
    //    fflush(stdout);
    precompiled_QQ_add = add;
    //	printf("stored\n");
    //    fflush(stdout);

    return add;
}
sequence_t *cCQ_add(int bits) {
    // OWNERSHIP: Returns cached sequence (precompiled_cCQ_add[bits]) - DO NOT FREE
    // READS: QPU_state->R0 for classical value
    // The precompiled sequence is reused across calls for performance
    int offset = INTEGERSIZE - bits;
    // Compute rotation angles
    int NonZeroCount = 0;
    int *bin = two_complement(*(QPU_state->R0), bits);
    if (bin == NULL) {
        return NULL;
    }

    // Compute rotations for addition
    double *rotations = calloc(bits, sizeof(double));
    if (rotations == NULL) {
        free(bin);
        return NULL;
    }
    for (int i = 0; i < bits; ++i) {
        for (int j = 0; j < bits - i; ++j) {
            rotations[j + i] += bin[bits - i - 1] * 2 * M_PI / (pow(2, j + 1));
        }
        if (rotations[i] != 0)
            NonZeroCount++;
    }
    free(bin);
    int start_layer = bits;

    if (precompiled_cCQ_add[bits]) {
        sequence_t *add = precompiled_cCQ_add[bits];

        for (int i = 0; i < bits; ++i) {
            add->seq[start_layer + i][add->gates_per_layer[start_layer + i] - 1].GateValue =
                rotations[i];
        }
        free(rotations);
        return add;
    }

    sequence_t *add = malloc(sizeof(sequence_t));
    if (add == NULL) {
        free(rotations);
        return NULL;
    }
    // allocate exact number of layer and enough gates per layer
    add->used_layer = 0;
    add->num_layer = 4 * bits - 1;
    add->gates_per_layer = calloc(add->num_layer, sizeof(num_t));
    if (add->gates_per_layer == NULL) {
        free(rotations);
        free(add);
        return NULL;
    }
    memset(add->gates_per_layer, 0, add->num_layer * sizeof(num_t));
    add->seq = calloc(add->num_layer, sizeof(gate_t *));
    if (add->seq == NULL) {
        free(add->gates_per_layer);
        free(rotations);
        free(add);
        return NULL;
    }
    for (int i = 0; i < add->num_layer; ++i) {
        add->seq[i] = calloc(2 * bits, sizeof(gate_t));
        if (add->seq[i] == NULL) {
            for (int j = 0; j < i; ++j) {
                free(add->seq[j]);
            }
            free(add->seq);
            free(add->gates_per_layer);
            free(rotations);
            free(add);
            return NULL;
        }
    }
    QFT(add, bits);

    for (int i = 0; i < bits; ++i) {
        cp(&add->seq[start_layer + i][add->gates_per_layer[start_layer + i]++], offset + i,
           2 * INTEGERSIZE - 1, rotations[bits - i - 1]);
    }
    free(rotations);
    add->used_layer++;

    QFT_inverse(add, bits);

    precompiled_cCQ_add[bits] = add;
    return add;
}
sequence_t *cQQ_add() {
    // OWNERSHIP: Returns cached sequence (precompiled_cQQ_add) - DO NOT FREE
    // The precompiled sequence is reused across calls for performance
    if (precompiled_cQQ_add != NULL)
        return precompiled_cQQ_add;

    sequence_t *add = malloc(sizeof(sequence_t));
    if (add == NULL) {
        return NULL;
    }

    // allocate exact number of layer and enough gates per layer
    add->used_layer = 0;
    add->num_layer =
        INTEGERSIZE * (INTEGERSIZE + 1) / 2 * 4 + 4 * INTEGERSIZE - 2 - INTEGERSIZE / 4 * 4 + 3;
    add->gates_per_layer = calloc(add->num_layer, sizeof(num_t));
    if (add->gates_per_layer == NULL) {
        free(add);
        return NULL;
    }
    memset(add->gates_per_layer, 0, add->num_layer * sizeof(num_t));
    add->seq = calloc(add->num_layer, sizeof(gate_t *));
    if (add->seq == NULL) {
        free(add->gates_per_layer);
        free(add);
        return NULL;
    }
    for (int i = 0; i < add->num_layer; ++i) {
        add->seq[i] = calloc(2 * INTEGERSIZE, sizeof(gate_t));
        if (add->seq[i] == NULL) {
            for (int j = 0; j < i; ++j) {
                free(add->seq[j]);
            }
            free(add->seq);
            free(add->gates_per_layer);
            free(add);
            return NULL;
        }
    }

    QFT(add, INTEGERSIZE);

    int control = 3 * INTEGERSIZE - 1;
    int rounds;
    int layer = 2 * INTEGERSIZE - 1;

    // block 1
    for (int bit = (int)INTEGERSIZE - 1; bit >= 0; --bit) {
        double value = 0;
        for (int i = 0; i < INTEGERSIZE - bit; ++i) {
            value += 2 * M_PI / (pow(2, i + 1)) / 2;
        }
        gate_t *g = &add->seq[layer][add->gates_per_layer[layer]++];
        cp(g, bit, control, value);
        layer++;
    }

    // block 2
    rounds = 0;
    for (int bit = (int)INTEGERSIZE - 1; bit >= 0; --bit) {
        gate_t *g = &add->seq[layer][add->gates_per_layer[layer]++];
        cx(g, control, INTEGERSIZE + bit);
        layer++;
        for (int i = 0; i < INTEGERSIZE - rounds; ++i) {
            double value = 2 * M_PI / (pow(2, i + 1)) / 2;
            g = &add->seq[layer][add->gates_per_layer[layer]++];
            cp(g, bit - i, control, -value);
            layer++;
        }
        g = &add->seq[layer][add->gates_per_layer[layer]++];
        cx(g, control, INTEGERSIZE + bit);
        layer++;
        rounds++;
    }

    // block 3
    rounds = 0;
    for (int bit = (int)INTEGERSIZE - 1; bit >= 0; --bit) {
        for (int i = 0; i < INTEGERSIZE - rounds; ++i) {
            double value = 2 * M_PI / (pow(2, i + 1)) / 2;
            gate_t *g = &add->seq[layer][add->gates_per_layer[layer]++];
            cp(g, bit - i, INTEGERSIZE + bit, value);
            layer++;
        }
        layer -= INTEGERSIZE - rounds;
        rounds++;
    }
    add->used_layer = layer + 1;
    QFT_inverse(add, INTEGERSIZE);
    precompiled_cQQ_add = add;

    return add;
}

sequence_t *P_add() {
    // OWNERSHIP: Caller owns returned sequence_t*, must free gates_per_layer, seq arrays, and seq
    // READS: QPU_state->R0 for phase value
    sequence_t *seq = malloc(sizeof(sequence_t));
    if (seq == NULL) {
        return NULL;
    }

    seq->gates_per_layer[0] = 1;
    seq->used_layer = 1;
    seq->num_layer = 1;
    // implement correct phase multiplication
    p(&seq->seq[0][0], 0, *(QPU_state->R0));

    return seq;
}
sequence_t *cP_add() {
    // OWNERSHIP: Caller owns returned sequence_t*, must free gates_per_layer, seq arrays, and seq
    // READS: QPU_state->R0 for phase value
    sequence_t *seq = malloc(sizeof(sequence_t));
    if (seq == NULL) {
        return NULL;
    }

    seq->gates_per_layer[0] = 1;
    seq->used_layer = 1;
    seq->num_layer = 1;
    // implement correct phase multiplication
    cp(&seq->seq[0][0], 0, 1, *(QPU_state->R0));

    return seq;
}