//
// Created by Sören Wilkening on 05.11.24.
//

#ifndef CQ_BACKEND_IMPROVED_LOGICOPERATIONS_H
#define CQ_BACKEND_IMPROVED_LOGICOPERATIONS_H

#include "definition.h"
#include "gate.h"
#include "QPU.h"
#include "Integer.h"

sequence_t *void_seq();

sequence_t *not_seq();
sequence_t *ctrl_not_seq();

sequence_t *and_seq();
sequence_t *q_and_seq();
sequence_t *qq_and_seq();
sequence_t *ctrl_q_and_seq();
sequence_t *ctrl_qq_and_seq();

sequence_t *branch_seq();

#endif //CQ_BACKEND_IMPROVED_LOGICOPERATIONS_H
