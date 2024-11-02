//
// Created by Sören Wilkening on 27.10.24.
//

#include <stdio.h>
#include <time.h>
#include "../QPU.h"

hybrid_stack_t stack;
sequence_t *precompiled_QQ_add = NULL;
sequence_t *precompiled_cQQ_add = NULL;

int main(void) {
    FILE *file = fopen("../benchmark/CQ_inprov_qtf.csv", "a");
//    fprintf(file, ",n,t\n");
    // ._start
    double tot = 0;
    for (int i = 0; i < 40; ++i) {
        clock_t t1 = clock();
        QFT(NULL);
        tot += (double) (clock() - t1) / CLOCKS_PER_SEC / 40;
    }
    fprintf(file, "%d,%d,%.15f\n", (int)floor(log2(INTEGERSIZE)) - 2, INTEGERSIZE, tot);
    fclose(file);
    

    return 0;
}