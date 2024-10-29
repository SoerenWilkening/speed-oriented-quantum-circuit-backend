//
// Created by Sören Wilkening on 27.10.24.
//
#include "../QPU.h"

circuit_t *init_circuit() {
    circuit_t *circ = malloc(sizeof(circuit_t));
    circ->used = 0;
    circ->used_layer = 0;
    circ->toff_decomp = DONTDECOMPOSETOFFOLI;

    // allocate sequence structures
    circ->allocated_layer = LAYERBLOCK;
    circ->used_gates_per_layer = calloc(LAYERBLOCK, sizeof(num_t));
    circ->allocated_gates_per_layer = malloc(LAYERBLOCK * sizeof(num_t));
    for (int i = 0; i < LAYERBLOCK; ++i) {
        circ->allocated_gates_per_layer[i] = GATESPERLAYERBLOCK;
    }
    circ->sequence = malloc(LAYERBLOCK * sizeof(gate_t *));
    for (int i = 0; i < LAYERBLOCK; ++i) {
        circ->sequence[i] = malloc(GATESPERLAYERBLOCK * sizeof(gate_t));
    }

    // allocate layer informations
    circ->used_layer_per_qubit = calloc(QUBITBLOCK, sizeof(num_t));
    circ->allocated_layer_per_qubit = malloc(LAYERBLOCK * sizeof(num_t));

    circ->layer_on_qubit = malloc(QUBITBLOCK * sizeof(layer_t *));
    for (int i = 0; i < QUBITBLOCK; ++i) {
        circ->layer_on_qubit[i] = malloc(LAYERQUBITBLOCK * sizeof(layer_t));
        circ->allocated_layer_per_qubit[i] = LAYERQUBITBLOCK;
    }
    memset(circ->used_gates_per_layer, 0, LAYERBLOCK * sizeof(num_t));

    circ->allocated_qubits = QUBITBLOCK;
    circ->used_qubits = 0;

    // initialize all the qubit indices and the ancilla register
    for (int i = 0; i < MAXQUBITS; ++i) {
        circ->qubit_indices[i] = i;
    }
    circ->used_qubit_indices = 0;
    circ->ancilla = &circ->qubit_indices[0];

    return circ;
}