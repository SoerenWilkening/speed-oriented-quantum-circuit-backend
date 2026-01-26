//
// Created by Sören Wilkening on 05.11.24.
//

#ifndef CQ_BACKEND_IMPROVED_INTEGER_H
#define CQ_BACKEND_IMPROVED_INTEGER_H

#include "QPU.h"
#include "definition.h"
#include "gate.h"
#include <math.h>
#include <stdint.h>

int *two_complement(int64_t x, int n);

// Quantum integer allocation/deallocation (uses circuit's qubit allocator)
// OWNERSHIP: Returned quantum_int_t* must be freed with free_element()
// Qubits are borrowed from circuit and tracked through allocator
quantum_int_t *QINT(circuit_t *circ, int width); // width: 1-64 bits
quantum_int_t *QBOOL(circuit_t *circ);           // Convenience: calls QINT(circ, 1)
quantum_int_t *INT(int64_t intg);
quantum_int_t *BOOL(bool intg);

// Backward compatibility macro for C code that calls QINT without width
#define QINT_DEFAULT(circ) QINT(circ, INTEGERSIZE)
quantum_int_t *bit_of_int(quantum_int_t *el1, int bit);
sequence_t *setting_seq();
void free_element(circuit_t *circ, quantum_int_t *el1);

// implementation of Integer operations gate sequences
// Multiplication functions (width-parameterized, cached by width)
// bits: Bit width of operands (1-64)
// value: Classical value for classical-quantum operations
// Returns: Cached sequence, DO NOT FREE
//
// Qubit layout:
// - QQ_mul(bits): [0:bits] = result, [bits:2*bits] = a, [2*bits:3*bits] = b
// - CQ_mul(bits, value): [0:bits] = result/target, classical value from parameter
// - cQQ_mul(bits): [0:bits] = result, [bits:2*bits] = a, [2*bits:3*bits] = b, [4*bits-1] = control
// - cCQ_mul(bits, value): [0:bits] = result/target, [3*bits-1] = control, classical value from
// parameter
sequence_t *CC_mul();
sequence_t *CQ_mul(int bits, int64_t value);
sequence_t *QQ_mul(int bits);
sequence_t *cCQ_mul(int bits, int64_t value);
sequence_t *cQQ_mul(int bits);

sequence_t *CC_add();
sequence_t *CQ_add(int bits, int64_t value);
sequence_t *QQ_add(int bits);
sequence_t *cCQ_add(int bits, int64_t value);
sequence_t *cQQ_add(int bits);

sequence_t *P_add();

// Legacy globals for backward compatibility (point to INTEGERSIZE versions)
extern sequence_t *precompiled_QQ_add;
extern sequence_t *precompiled_cQQ_add;
extern sequence_t *precompiled_CQ_add[64];
extern sequence_t *precompiled_cCQ_add[64];

// Width-parameterized precompiled caches (index 0 unused, 1-64 valid)
extern sequence_t *precompiled_QQ_add_width[65];
extern sequence_t *precompiled_cQQ_add_width[65];

// Legacy globals for backward compatibility (point to INTEGERSIZE versions)
extern sequence_t *precompiled_QQ_mul;
extern sequence_t *precompiled_cQQ_mul;

// Width-parameterized precompiled caches (index 0 unused, 1-64 valid)
extern sequence_t *precompiled_QQ_mul_width[65];
extern sequence_t *precompiled_cQQ_mul_width[65];

#endif // CQ_BACKEND_IMPROVED_INTEGER_H
