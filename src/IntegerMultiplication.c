//
// Created by Sören Wilkening on 05.11.24.
//
#include "../include/Integer.h"

sequence_t *QQ_mul() {
    sequence_t *mul = malloc(sizeof(sequence_t));

    mul->seq = malloc(
            (INTEGERSIZE * (2 * INTEGERSIZE + 6) - 1) * sizeof(gate_t));
    mul->used_layer = 0;
    mul->num_layer = INTEGERSIZE * (2 * INTEGERSIZE + 6) - 1;
    mul->gates_per_layer = calloc(mul->num_layer, sizeof(num_t));

    for (int i = 0; i < mul->num_layer; ++i) mul->seq[i] = malloc(2 * INTEGERSIZE * sizeof(gate_t));

    QFT(mul);
    num_t layer = INTEGERSIZE;
    int rounds = 0;

    // First blocks of CCP decompositions
    // all the CP block of the first decomp step can be merged
    for (int bit = INTEGERSIZE - 1; bit >= 0; --bit) {
        layer = INTEGERSIZE + 2 * rounds;
        num_t control = INTEGERSIZE + bit;
        for (int i = 0; i < INTEGERSIZE - rounds; ++i) {
            num_t target = i + rounds;
            double value = M_PI / (pow(2, i + 1)) * (pow(2, INTEGERSIZE) - 1);
            gate_t *g = &mul->seq[layer][mul->gates_per_layer[layer]++];
            cp(g, target, control, value);
            layer++;
        }
        rounds++;
    }
    layer -= INTEGERSIZE - 1;

    // intermediate step, C1X0 C0P_2(value/2)C1X0
    for (int bit_int2 = 0; bit_int2 < INTEGERSIZE; ++bit_int2) {
        for (int bit = INTEGERSIZE - 1; bit >= 0; --bit) {
            num_t control = INTEGERSIZE + bit;
            gate_t *g = &mul->seq[layer][mul->gates_per_layer[layer]++];
            cx(g, control, 2 * INTEGERSIZE + bit_int2);
            layer++;
        }
        int rounds = 0;
        for (int bit = INTEGERSIZE - 1; bit >= 0; --bit) {
            num_t control = INTEGERSIZE + bit;
            for (int i = 0; i < INTEGERSIZE - rounds; ++i) {
                num_t target = i + rounds;
                double value = -M_PI / (pow(2, i + 1)) * pow(2, bit_int2);
                gate_t *g = &mul->seq[layer][mul->gates_per_layer[layer]++];
                cp(g, target, control, value);
                layer++;
            }
            layer -= INTEGERSIZE - rounds;
            rounds++;
        }
        layer += INTEGERSIZE;


        for (int bit = INTEGERSIZE - 1; bit >= 0; --bit) {
            num_t control = INTEGERSIZE + bit;
            gate_t *g = &mul->seq[layer][mul->gates_per_layer[layer]++];
            cx(g, control, 2 * INTEGERSIZE + bit_int2);
            layer++;
        }
        layer -= INTEGERSIZE - 1;
    }

    // the final step, C1P(value/2)
    rounds = INTEGERSIZE - 1;
    for (int bit = 0; bit < INTEGERSIZE; ++bit) {
        num_t control = INTEGERSIZE + bit;
        for (int i = INTEGERSIZE - rounds - 1; i >= 0; --i) {
            num_t target = i + rounds;
            double value = M_PI / (pow(2, i + 1)) * (pow(2, INTEGERSIZE) - 1);
            gate_t *g = &mul->seq[layer][mul->gates_per_layer[layer]++];
            cp(g, target, control, value);
            layer++;
        }
        rounds--;
        layer -= bit;
    }

    mul->used_layer = layer;
    QFT_inverse(mul);

    return mul;
}
