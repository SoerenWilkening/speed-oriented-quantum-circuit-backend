//
// Created by Sören Wilkening on 26.10.24.
//

#ifndef CQ_BACKEND_IMPROVED_QPU_H
#define CQ_BACKEND_IMPROVED_QPU_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

//typedef enum {
//    BOOL,
//    SIGNED,
//    UNSIGNED,
//    UNINITIALIZED
//} type_t;
//
//typedef struct {
//    bool_t qualifier;
//    union {
//        qubit_t q_address[INTEGERSIZE];
//        int64_t *c_address;
//    };
//    type_t type;
//} element_t;

//typedef struct {
//    element_t *el1;
//    element_t *el2;
//    element_t *el3;
//    element_t *control;
//    sequence_t *(*routine)();
//    bool_t invert;
//} instruction_t;
//
//typedef struct {
//    element_t GPR1[1];
//    element_t GPR2[1];
//    element_t GPR3[1];
//    element_t GPC[1];
//
//    instruction_t instruction_list[10000];
//    int instruction_counter;
//
//    circuit_t *circuit;
//} hybrid_stack_t;
//
//extern hybrid_stack_t stack;

//#define POINTER 1
//#define VALUE 0

// circuit functions ===================================================================================================
//circuit_t *init_circuit();

// integer generation and stack operation functions ====================================================================

//int *two_complement(int64_t x, int n);
//
//element_t *quantum_bool();
//
//element_t *signed_quantum_integer();
//
//element_t *unsigned_quantum_integer();
//
//element_t *classical_integer(int64_t intg);
//
//element_t *bit_of_int(element_t *el1, int bit);

//
//void push(element_t *element); // push qubit reference to stack
//
//void pop(element_t *element); // remove first entry from stack

//void MOV(element_t *el1, element_t *el2, int pov);
//
//void ADD(element_t *el1, element_t *el2);
//
//void SUB(element_t *el1, element_t *el2);
//
//void IMUL(element_t *el1, element_t *el2, element_t *res);
//
//void IDIV(element_t *el1, element_t *el2, element_t *remainder);
//
//void NOT(element_t *el1);
//
//void IF(element_t *el1);
//
//void ELSE(element_t *el1);
//
//void TSTBIT(element_t *el1, element_t *el2, int bit);
//
//void init_instruction(instruction_t *instr);
//
//void execute(instruction_t *instr);

//extern sequence_t *precompiled_QQ_add;
//extern sequence_t *precompiled_cQQ_add;
//extern sequence_t *precompiled_CQ_add;
//extern sequence_t *precompiled_cCQ_add;

// INTEGERSIZE * (2 * INTEGERSIZE + 6) - 1

#endif //CQ_BACKEND_IMPROVED_QPU_H
