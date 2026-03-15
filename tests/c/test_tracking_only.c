/**
 * @file test_tracking_only.c
 * @brief Unit tests for simulate flag in circuit struct and run_instruction().
 *
 * Tests:
 * 1. gate_count initialized to 0, simulate defaults to 0 in init_circuit
 * 2. simulate=0 increments gate_count without building the circuit
 * 3. simulate=1 builds the circuit and increments gate_count
 * 4. simulate=0 does not modify circuit structure
 * 5. gate_count is cumulative across multiple run_instruction calls
 * 6. NULL sequence is a no-op regardless of simulate flag
 * 7. validated_run_instruction respects circ->simulate
 * 8. simulate=0 with invert=1 counts gates without building circuit
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "circuit.h"
#include "execution.h"
#include "gate.h"

/* Helper: count total gates in the circuit structure */
static int total_gate_count(circuit_t *circ) {
    int count = 0;
    for (int layer = 0; layer < (int)circ->used_layer; ++layer) {
        count += (int)circ->used_gates_per_layer[layer];
    }
    return count;
}

/* Helper: create a simple 2-layer CX sequence for testing */
static sequence_t *make_cx_sequence(void) {
    sequence_t *seq = malloc(sizeof(sequence_t));
    if (seq == NULL)
        return NULL;

    seq->num_layer = 2;
    seq->used_layer = 2;
    seq->gates_per_layer = calloc(2, sizeof(num_t));
    seq->seq = malloc(2 * sizeof(gate_t *));

    for (int i = 0; i < 2; i++) {
        seq->seq[i] = malloc(1 * sizeof(gate_t));
        seq->gates_per_layer[i] = 1;
    }

    /* Layer 0: X gate on virtual qubit 0 */
    memset(&seq->seq[0][0], 0, sizeof(gate_t));
    x(&seq->seq[0][0], 0);

    /* Layer 1: CX gate on virtual qubit 1, controlled by virtual qubit 0 */
    memset(&seq->seq[1][0], 0, sizeof(gate_t));
    cx(&seq->seq[1][0], 1, 0);

    return seq;
}

static void free_test_sequence(sequence_t *seq) {
    if (seq == NULL)
        return;
    for (int i = 0; i < (int)seq->num_layer; i++) {
        free(seq->seq[i]);
    }
    free(seq->seq);
    free(seq->gates_per_layer);
    free(seq);
}

/* ------------------------------------------------------------------ */
/* Test 1: gate_count and simulate initialized correctly              */
/* ------------------------------------------------------------------ */
static void test_gate_count_initialized_to_zero(void) {
    printf("test_gate_count_initialized_to_zero... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);
    assert(circ->gate_count == 0 && "gate_count should be 0 after init_circuit");
    assert(circ->simulate == 0 && "simulate should default to 0 after init_circuit");

    printf("PASS\n");
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 2: simulate=0 counts gates without building circuit           */
/* ------------------------------------------------------------------ */
static void test_tracking_only_counts_without_building(void) {
    printf("test_tracking_only_counts_without_building... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    sequence_t *seq = make_cx_sequence();
    assert(seq != NULL);

    /* Map virtual qubits to physical qubits */
    qubit_t qa[2] = {0, 1};

    /* Run with simulate=0 (tracking-only) */
    circ->simulate = 0;
    run_instruction(seq, qa, 0, circ);

    /* gate_count should reflect the 2 gates in the sequence */
    assert(circ->gate_count == 2 && "simulate=0 should count 2 gates");

    /* Circuit structure should be empty */
    int actual_gates = total_gate_count(circ);
    assert(actual_gates == 0 && "simulate=0 should not add gates to circuit");
    assert(circ->used_layer == 0 && "simulate=0 should not create layers");

    printf("PASS\n");
    free_test_sequence(seq);
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 3: simulate=1 builds circuit and increments gate_count        */
/* ------------------------------------------------------------------ */
static void test_normal_mode_builds_and_counts(void) {
    printf("test_normal_mode_builds_and_counts... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    sequence_t *seq = make_cx_sequence();
    assert(seq != NULL);

    qubit_t qa[2] = {0, 1};

    /* Run with simulate=1 (full simulation) */
    circ->simulate = 1;
    run_instruction(seq, qa, 0, circ);

    /* gate_count should reflect the 2 gates */
    assert(circ->gate_count == 2 && "simulate=1 should count 2 gates");

    /* Circuit structure should contain the gates */
    int actual_gates = total_gate_count(circ);
    assert(actual_gates == 2 && "simulate=1 should add 2 gates to circuit");

    printf("PASS\n");
    free_test_sequence(seq);
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 4: simulate=0 does not modify circuit structure               */
/* ------------------------------------------------------------------ */
static void test_tracking_only_preserves_circuit_state(void) {
    printf("test_tracking_only_preserves_circuit_state... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    /* First, add a gate normally */
    circ->simulate = 1;
    gate_t g;
    memset(&g, 0, sizeof(gate_t));
    x(&g, 0);
    add_gate(circ, &g);

    size_t used_before = circ->used;
    num_t used_layer_before = circ->used_layer;
    num_t used_qubits_before = circ->used_qubits;

    /* Now run a sequence with simulate=0 */
    circ->simulate = 0;
    sequence_t *seq = make_cx_sequence();
    assert(seq != NULL);

    qubit_t qa[2] = {2, 3};
    run_instruction(seq, qa, 0, circ);

    /* Circuit structure should be unchanged */
    assert(circ->used == used_before && "simulate=0 should not change circ->used");
    assert(circ->used_layer == used_layer_before &&
           "simulate=0 should not change circ->used_layer");
    assert(circ->used_qubits == used_qubits_before &&
           "simulate=0 should not change circ->used_qubits");

    /* But gate_count should have increased */
    assert(circ->gate_count == 2 && "simulate=0 should still increment gate_count");

    printf("PASS\n");
    free_test_sequence(seq);
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 5: gate_count is cumulative across multiple calls             */
/* ------------------------------------------------------------------ */
static void test_gate_count_cumulative(void) {
    printf("test_gate_count_cumulative... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    sequence_t *seq = make_cx_sequence();
    assert(seq != NULL);

    qubit_t qa[2] = {0, 1};

    /* Run simulate=0 three times */
    circ->simulate = 0;
    run_instruction(seq, qa, 0, circ);
    run_instruction(seq, qa, 0, circ);
    run_instruction(seq, qa, 0, circ);

    assert(circ->gate_count == 6 && "3 runs * 2 gates = 6 total");

    /* Mix in a simulate=1 run */
    circ->simulate = 1;
    run_instruction(seq, qa, 0, circ);

    assert(circ->gate_count == 8 && "6 + 2 from simulate=1 run = 8 total");

    printf("PASS\n");
    free_test_sequence(seq);
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 6: NULL sequence is a no-op regardless of simulate flag       */
/* ------------------------------------------------------------------ */
static void test_tracking_only_null_sequence(void) {
    printf("test_tracking_only_null_sequence... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    qubit_t qa[2] = {0, 1};

    /* NULL sequence should be a no-op for both modes */
    circ->simulate = 0;
    run_instruction(NULL, qa, 0, circ);
    assert(circ->gate_count == 0 && "NULL sequence should not increment gate_count");

    circ->simulate = 1;
    run_instruction(NULL, qa, 0, circ);
    assert(circ->gate_count == 0 && "NULL sequence should not increment gate_count");

    printf("PASS\n");
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 7: validated_run_instruction respects circ->simulate          */
/* ------------------------------------------------------------------ */
static void test_validated_run_instruction_tracking(void) {
    printf("test_validated_run_instruction_tracking... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    sequence_t *seq = make_cx_sequence();
    assert(seq != NULL);

    qubit_t qa[2] = {0, 1};

    /* simulate=0 via validated entry point */
    circ->simulate = 0;
    int rc = validated_run_instruction(seq, qa, 0, circ);
    assert(rc == QV_OK);
    assert(circ->gate_count == 2 && "validated simulate=0 should count gates");
    assert(total_gate_count(circ) == 0 && "validated simulate=0 should not build circuit");

    /* simulate=1 via validated entry point */
    circ->simulate = 1;
    rc = validated_run_instruction(seq, qa, 0, circ);
    assert(rc == QV_OK);
    assert(circ->gate_count == 4 && "validated simulate=1 should also count gates");
    assert(total_gate_count(circ) == 2 && "validated simulate=1 should build circuit");

    /* NULL circuit check */
    rc = validated_run_instruction(seq, qa, 0, NULL);
    assert(rc == QV_NULL_CIRC);

    printf("PASS\n");
    free_test_sequence(seq);
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 8: simulate=0 with invert=1 counts without building           */
/* ------------------------------------------------------------------ */
static void test_tracking_only_with_invert(void) {
    printf("test_tracking_only_with_invert... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    sequence_t *seq = make_cx_sequence();
    assert(seq != NULL);

    qubit_t qa[2] = {0, 1};

    /* Run with simulate=0 and invert=1 */
    circ->simulate = 0;
    run_instruction(seq, qa, 1, circ);

    /* gate_count should match the non-inverted case */
    assert(circ->gate_count == 2 &&
           "simulate=0 with invert should count same number of gates");

    /* Circuit structure should remain empty */
    int actual_gates = total_gate_count(circ);
    assert(actual_gates == 0 &&
           "simulate=0 with invert should not add gates to circuit");
    assert(circ->used_layer == 0 &&
           "simulate=0 with invert should not create layers");

    printf("PASS\n");
    free_test_sequence(seq);
    free_circuit(circ);
}

int main(void) {
    printf("=== simulate flag C unit tests ===\n\n");

    test_gate_count_initialized_to_zero();
    test_tracking_only_counts_without_building();
    test_normal_mode_builds_and_counts();
    test_tracking_only_preserves_circuit_state();
    test_gate_count_cumulative();
    test_tracking_only_null_sequence();
    test_validated_run_instruction_tracking();
    test_tracking_only_with_invert();

    printf("\n=== ALL 8 TESTS PASSED ===\n");
    return 0;
}
