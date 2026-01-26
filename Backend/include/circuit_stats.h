//
// circuit_stats.h - Circuit statistics and metrics
// Dependencies: types.h
//
// Provides functions to compute:
// - Total gate count
// - Circuit depth (number of layers)
// - Qubit count
// - Gate type breakdown
//

#ifndef QUANTUM_CIRCUIT_STATS_H
#define QUANTUM_CIRCUIT_STATS_H

#include "types.h"

// Forward declaration
struct circuit_s;
typedef struct circuit_s circuit_t;

// Total number of gates in circuit
size_t circuit_gate_count(circuit_t *circ);

// Circuit depth (number of layers used)
num_t circuit_depth(circuit_t *circ);

// Number of qubits used
num_t circuit_qubit_count(circuit_t *circ);

// Gate type counts structure
typedef struct {
    size_t x_gates;     // X gates
    size_t y_gates;     // Y gates
    size_t z_gates;     // Z gates
    size_t h_gates;     // Hadamard gates
    size_t p_gates;     // Phase gates
    size_t cx_gates;    // Controlled-X (CNOT)
    size_t ccx_gates;   // Toffoli (CCX)
    size_t other_gates; // Other gate types
} gate_counts_t;

// Get breakdown of gate types in circuit
gate_counts_t circuit_gate_counts(circuit_t *circ);

#endif // QUANTUM_CIRCUIT_STATS_H
