//
// Created by Sören Wilkening on 26.10.24.
//

#include "../QPU.h"

void push(element_t *element){
    stack.element[stack.stack_pointer + 1] = element;
    stack.stack_pointer++;
}

void pop(element_t *el1){
    if (stack.stack_pointer == -1){
        printf("Stack empty!\n");
        exit(3);
    }
    stack.stack_pointer--;
}

void mov(element_t *el1, element_t *el2, int pov){
    // 3 possible cased:
    //  - C <- C copy classical data
    //  - Q <- C create sequence to initialize classical integer
    //  - Q <- Q create sequence to copy quantum integer to quantum integer

    if (el1->qualifier == Cl){
        // Not allowed operation
        if (el2->qualifier == Qu){
            printf("Not allowed copy of quantum Integer to classical integer!\n");
            exit(1);
        }
        if (pov == POINTER) { // copy adress
            free(el1->c_address);
            el1->c_address = el2->c_address;
        }
        else *el1->c_address = *el2->c_address; // copy value
        return;
    }
    if (el2->qualifier == Cl) {
        // Circuit creation for Q <- C to be included
        if (pov == POINTER){
            printf("Illegal classical pointer copy to qunatum int!\n");
            exit(4);
        }
        return;
    }
    if (pov == POINTER){
        el1->q_address = el1->q_address;
    }
    el1->type = el1->type;
    // Circuit creation for Q <- Q to be included
}