//
// add_seq_1_4.c - Hardcoded QQ_add and cQQ_add sequences for 1-4 bit widths
//
// This file contains statically-defined gate sequences that match the
// dynamically-generated sequences from QQ_add() and cQQ_add() in IntegerAddition.c
//

#include "sequences.h"

// SEQ_PI as compile-time constant for static initializers
// (math.h SEQ_PI is not a constant expression in standard C)
#ifndef SEQ_PI
#define SEQ_PI 3.14159265358979323846
#endif

// ============================================================================
// QQ_ADD WIDTH 1 (3 layers)
// Qubit layout: [0] = target, [1] = control
// ============================================================================

static const gate_t QQ_ADD_1_L0[] = {{.Gate = H,
                                      .Target = 0,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_1_L1[] = {{.Gate = P,
                                      .Target = 0,
                                      .NumControls = 1,
                                      .Control = {1},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_1_L2[] = {{.Gate = H,
                                      .Target = 0,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t *QQ_ADD_1_LAYERS[] = {QQ_ADD_1_L0, QQ_ADD_1_L1, QQ_ADD_1_L2};
static const num_t QQ_ADD_1_GPL[] = {1, 1, 1};

static const sequence_t HARDCODED_QQ_ADD_1 = {.seq = (gate_t **)QQ_ADD_1_LAYERS,
                                              .num_layer = 3,
                                              .used_layer = 3,
                                              .gates_per_layer = (num_t *)QQ_ADD_1_GPL};

// ============================================================================
// QQ_ADD WIDTH 2 (8 layers)
// Qubit layout: [0,1] = target, [2,3] = control
// ============================================================================

static const gate_t QQ_ADD_2_L0[] = {{.Gate = H,
                                      .Target = 1,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_2_L1[] = {{.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_2_L2[] = {{.Gate = H,
                                      .Target = 0,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_2_L3[] = {{.Gate = P,
                                      .Target = 0,
                                      .NumControls = 1,
                                      .Control = {2},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_2_L4[] = {{.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {2},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_2_L5[] = {{.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {3},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI,
                                      .NumBasisGates = 0},
                                     {.Gate = H,
                                      .Target = 0,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_2_L6[] = {{.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = -SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_2_L7[] = {{.Gate = H,
                                      .Target = 1,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t *QQ_ADD_2_LAYERS[] = {QQ_ADD_2_L0, QQ_ADD_2_L1, QQ_ADD_2_L2, QQ_ADD_2_L3,
                                          QQ_ADD_2_L4, QQ_ADD_2_L5, QQ_ADD_2_L6, QQ_ADD_2_L7};
static const num_t QQ_ADD_2_GPL[] = {1, 1, 1, 1, 1, 2, 1, 1};

static const sequence_t HARDCODED_QQ_ADD_2 = {.seq = (gate_t **)QQ_ADD_2_LAYERS,
                                              .num_layer = 8,
                                              .used_layer = 8,
                                              .gates_per_layer = (num_t *)QQ_ADD_2_GPL};

// ============================================================================
// QQ_ADD WIDTH 3 (13 layers)
// Qubit layout: [0,1,2] = target, [3,4,5] = control
// ============================================================================

static const gate_t QQ_ADD_3_L0[] = {{.Gate = H,
                                      .Target = 2,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L1[] = {{.Gate = P,
                                      .Target = 2,
                                      .NumControls = 1,
                                      .Control = {1},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L2[] = {{.Gate = P,
                                      .Target = 2,
                                      .NumControls = 1,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 4,
                                      .NumBasisGates = 0},
                                     {.Gate = H,
                                      .Target = 1,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L3[] = {{.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L4[] = {{.Gate = H,
                                      .Target = 0,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L5[] = {{.Gate = P,
                                      .Target = 0,
                                      .NumControls = 1,
                                      .Control = {3},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L6[] = {{.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {3},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L7[] = {{.Gate = P,
                                      .Target = 2,
                                      .NumControls = 1,
                                      .Control = {3},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 4,
                                      .NumBasisGates = 0},
                                     {.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {4},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L8[] = {{.Gate = P,
                                      .Target = 2,
                                      .NumControls = 1,
                                      .Control = {4},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0},
                                     {.Gate = H,
                                      .Target = 0,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L9[] = {{.Gate = P,
                                      .Target = 2,
                                      .NumControls = 1,
                                      .Control = {5},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI,
                                      .NumBasisGates = 0},
                                     {.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = -SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L10[] = {{.Gate = P,
                                       .Target = 2,
                                       .NumControls = 1,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = -SEQ_PI / 4,
                                       .NumBasisGates = 0},
                                      {.Gate = H,
                                       .Target = 1,
                                       .NumControls = 0,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = 0,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L11[] = {{.Gate = P,
                                       .Target = 2,
                                       .NumControls = 1,
                                       .Control = {1},
                                       .large_control = NULL,
                                       .GateValue = -SEQ_PI / 2,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_3_L12[] = {{.Gate = H,
                                       .Target = 2,
                                       .NumControls = 0,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = 0,
                                       .NumBasisGates = 0}};

static const gate_t *QQ_ADD_3_LAYERS[] = {
    QQ_ADD_3_L0, QQ_ADD_3_L1, QQ_ADD_3_L2, QQ_ADD_3_L3,  QQ_ADD_3_L4,  QQ_ADD_3_L5, QQ_ADD_3_L6,
    QQ_ADD_3_L7, QQ_ADD_3_L8, QQ_ADD_3_L9, QQ_ADD_3_L10, QQ_ADD_3_L11, QQ_ADD_3_L12};
static const num_t QQ_ADD_3_GPL[] = {1, 1, 2, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1};

static const sequence_t HARDCODED_QQ_ADD_3 = {.seq = (gate_t **)QQ_ADD_3_LAYERS,
                                              .num_layer = 13,
                                              .used_layer = 13,
                                              .gates_per_layer = (num_t *)QQ_ADD_3_GPL};

// ============================================================================
// QQ_ADD WIDTH 4 (18 layers)
// Qubit layout: [0,1,2,3] = target, [4,5,6,7] = control
// ============================================================================

static const gate_t QQ_ADD_4_L0[] = {{.Gate = H,
                                      .Target = 3,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L1[] = {{.Gate = P,
                                      .Target = 3,
                                      .NumControls = 1,
                                      .Control = {2},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L2[] = {{.Gate = P,
                                      .Target = 3,
                                      .NumControls = 1,
                                      .Control = {1},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 4,
                                      .NumBasisGates = 0},
                                     {.Gate = H,
                                      .Target = 2,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L3[] = {{.Gate = P,
                                      .Target = 3,
                                      .NumControls = 1,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 8,
                                      .NumBasisGates = 0},
                                     {.Gate = P,
                                      .Target = 2,
                                      .NumControls = 1,
                                      .Control = {1},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L4[] = {{.Gate = P,
                                      .Target = 2,
                                      .NumControls = 1,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 4,
                                      .NumBasisGates = 0},
                                     {.Gate = H,
                                      .Target = 1,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L5[] = {{.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L6[] = {{.Gate = H,
                                      .Target = 0,
                                      .NumControls = 0,
                                      .Control = {0},
                                      .large_control = NULL,
                                      .GateValue = 0,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L7[] = {{.Gate = P,
                                      .Target = 0,
                                      .NumControls = 1,
                                      .Control = {4},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L8[] = {{.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {4},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 2,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L9[] = {{.Gate = P,
                                      .Target = 2,
                                      .NumControls = 1,
                                      .Control = {4},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI / 4,
                                      .NumBasisGates = 0},
                                     {.Gate = P,
                                      .Target = 1,
                                      .NumControls = 1,
                                      .Control = {5},
                                      .large_control = NULL,
                                      .GateValue = SEQ_PI,
                                      .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L10[] = {{.Gate = P,
                                       .Target = 3,
                                       .NumControls = 1,
                                       .Control = {4},
                                       .large_control = NULL,
                                       .GateValue = SEQ_PI / 8,
                                       .NumBasisGates = 0},
                                      {.Gate = P,
                                       .Target = 2,
                                       .NumControls = 1,
                                       .Control = {5},
                                       .large_control = NULL,
                                       .GateValue = SEQ_PI / 2,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L11[] = {{.Gate = P,
                                       .Target = 3,
                                       .NumControls = 1,
                                       .Control = {5},
                                       .large_control = NULL,
                                       .GateValue = SEQ_PI / 4,
                                       .NumBasisGates = 0},
                                      {.Gate = P,
                                       .Target = 2,
                                       .NumControls = 1,
                                       .Control = {6},
                                       .large_control = NULL,
                                       .GateValue = SEQ_PI,
                                       .NumBasisGates = 0},
                                      {.Gate = H,
                                       .Target = 0,
                                       .NumControls = 0,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = 0,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L12[] = {{.Gate = P,
                                       .Target = 3,
                                       .NumControls = 1,
                                       .Control = {6},
                                       .large_control = NULL,
                                       .GateValue = SEQ_PI / 2,
                                       .NumBasisGates = 0},
                                      {.Gate = P,
                                       .Target = 1,
                                       .NumControls = 1,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = -SEQ_PI / 2,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L13[] = {{.Gate = P,
                                       .Target = 3,
                                       .NumControls = 1,
                                       .Control = {7},
                                       .large_control = NULL,
                                       .GateValue = SEQ_PI,
                                       .NumBasisGates = 0},
                                      {.Gate = P,
                                       .Target = 2,
                                       .NumControls = 1,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = -SEQ_PI / 4,
                                       .NumBasisGates = 0},
                                      {.Gate = H,
                                       .Target = 1,
                                       .NumControls = 0,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = 0,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L14[] = {{.Gate = P,
                                       .Target = 3,
                                       .NumControls = 1,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = -SEQ_PI / 8,
                                       .NumBasisGates = 0},
                                      {.Gate = P,
                                       .Target = 2,
                                       .NumControls = 1,
                                       .Control = {1},
                                       .large_control = NULL,
                                       .GateValue = -SEQ_PI / 2,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L15[] = {{.Gate = P,
                                       .Target = 3,
                                       .NumControls = 1,
                                       .Control = {1},
                                       .large_control = NULL,
                                       .GateValue = -SEQ_PI / 4,
                                       .NumBasisGates = 0},
                                      {.Gate = H,
                                       .Target = 2,
                                       .NumControls = 0,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = 0,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L16[] = {{.Gate = P,
                                       .Target = 3,
                                       .NumControls = 1,
                                       .Control = {2},
                                       .large_control = NULL,
                                       .GateValue = -SEQ_PI / 2,
                                       .NumBasisGates = 0}};

static const gate_t QQ_ADD_4_L17[] = {{.Gate = H,
                                       .Target = 3,
                                       .NumControls = 0,
                                       .Control = {0},
                                       .large_control = NULL,
                                       .GateValue = 0,
                                       .NumBasisGates = 0}};

static const gate_t *QQ_ADD_4_LAYERS[] = {
    QQ_ADD_4_L0,  QQ_ADD_4_L1,  QQ_ADD_4_L2,  QQ_ADD_4_L3,  QQ_ADD_4_L4,  QQ_ADD_4_L5,
    QQ_ADD_4_L6,  QQ_ADD_4_L7,  QQ_ADD_4_L8,  QQ_ADD_4_L9,  QQ_ADD_4_L10, QQ_ADD_4_L11,
    QQ_ADD_4_L12, QQ_ADD_4_L13, QQ_ADD_4_L14, QQ_ADD_4_L15, QQ_ADD_4_L16, QQ_ADD_4_L17};
static const num_t QQ_ADD_4_GPL[] = {1, 1, 2, 2, 2, 1, 1, 1, 1, 2, 2, 3, 2, 3, 2, 2, 1, 1};

static const sequence_t HARDCODED_QQ_ADD_4 = {.seq = (gate_t **)QQ_ADD_4_LAYERS,
                                              .num_layer = 18,
                                              .used_layer = 18,
                                              .gates_per_layer = (num_t *)QQ_ADD_4_GPL};

// ============================================================================
// QQ_ADD DISPATCH HELPER
// ============================================================================

const sequence_t *get_hardcoded_QQ_add_1_4(int bits) {
    switch (bits) {
    case 1:
        return &HARDCODED_QQ_ADD_1;
    case 2:
        return &HARDCODED_QQ_ADD_2;
    case 3:
        return &HARDCODED_QQ_ADD_3;
    case 4:
        return &HARDCODED_QQ_ADD_4;
    default:
        return NULL;
    }
}
