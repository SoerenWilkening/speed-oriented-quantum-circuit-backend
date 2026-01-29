//
// circuit_output.h - Circuit visualization and export
// Dependencies: types.h
//
// Provides functions for:
// - Text-based circuit visualization
// - OpenQASM 3.0 export
//

#ifndef QUANTUM_CIRCUIT_OUTPUT_H
#define QUANTUM_CIRCUIT_OUTPUT_H

#include "types.h"

// Forward declaration
struct circuit_s;
typedef struct circuit_s circuit_t;

// Print circuit to stdout in text format
// Shows gate count, layer count, qubit count, and ASCII diagram
void print_circuit(circuit_t *circ);

// Enhanced circuit visualization with horizontal layout
// Shows:
// - Qubit indices on left
// - Layer numbers on top
// - Compact gate symbols (H, X, Z, P, etc.)
// - Control connections with vertical lines (| and @)
// - Time flows left-to-right
void circuit_visualize(circuit_t *circ);

// Export circuit to OpenQASM 3.0 format
// Creates file at: {path}/circuit.qasm
void circuit_to_opanqasm(circuit_t *circ, char *path);

#endif // QUANTUM_CIRCUIT_OUTPUT_H
