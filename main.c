#include <stdio.h>
#include <time.h>
#include "QPU.h"

hybrid_stack_t stack = {
        .stack_pointer = -1,
};

sequence_t *precompiled_QQ_add = NULL;

int main(void) {

    // initialize the rest of the stack
    stack.circuit = init_circuit();

    // ._data
    element_t *Aq = signed_quantum_integer();
    element_t *Bq = signed_quantum_integer();
    element_t *Cc = classical_integer(12);
    element_t *Dc = classical_integer(24);

    // ._start
    clock_t t1 = clock();
    instruction_t *ins = ADD(Cc, Dc);

    // execute instruction
    push(ins->el2);
    push(ins->el1);
    sequence_t *res = ins->routine();
    pop(Cc);
    pop(Cc);

    printf("%d\n", *Cc->c_address);
    printf("%d\n", *Dc->c_address);
    print_sequence(res);
    printf("%p\n", res);
    printf("%f\n", (double) (clock() - t1) / CLOCKS_PER_SEC);
    return 0;
}
