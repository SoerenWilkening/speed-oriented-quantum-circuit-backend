//
// Created by Sören Wilkening on 26.10.24.
//
#include "../QPU.h"

element_t *quantum_bool(){
    element_t *integer = malloc(sizeof(element_t));
    integer->type = BOOL;
    integer->qualifier = Qu;
    integer->q_address = stack.circuit->used_qubit_indices;
    stack.circuit->ancilla += 1;
    stack.circuit->used_qubit_indices += 1;
    integer->c_address = NULL;
    return integer;
}

element_t *signed_quantum_integer(){
    element_t *integer = malloc(sizeof(element_t));
    integer->type = SIGNED;
    integer->qualifier = Qu;
    integer->q_address = stack.circuit->used_qubit_indices;
    stack.circuit->ancilla += INTEGERSIZE;
    stack.circuit->used_qubit_indices += INTEGERSIZE;
    integer->c_address = NULL;
    return integer;
}

element_t *unsigned_quantum_integer(){
    element_t *integer = malloc(sizeof(element_t));
    integer->type = UNSIGNED;
    integer->qualifier = Qu;
    integer->q_address = stack.circuit->used_qubit_indices + 1;
    stack.circuit->ancilla += INTEGERSIZE + 1;
    stack.circuit->used_qubit_indices += INTEGERSIZE + 1; // one more qubit for various operations like qq comparison
    integer->c_address = NULL;
    return integer;
}

element_t *classical_integer(int64_t intg){
    element_t *integer = malloc(sizeof(integer));
    integer->qualifier = Cl;
    integer->c_address = malloc(sizeof(int64_t));
    *integer->c_address = intg;
    return integer;
}