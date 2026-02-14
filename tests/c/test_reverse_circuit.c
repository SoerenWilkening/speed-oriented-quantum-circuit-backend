/**
 * @file test_reverse_circuit.c
 * @brief Unit tests for reverse_circuit_range gate value preservation (Phase 65, Plan 01).
 *
 * Tests that reverse_circuit_range() preserves GateValue for self-inverse gates
 * (X, Y, Z, H, M) and negates GateValue for rotation gates (P, R, Rx, Ry, Rz).
 *
 * Test strategy: With the Phase 65 fix, self-inverse gates (X, CX, CCX, H)
 * preserve GateValue during reversal, which means their reversed copies are
 * true inverses that the optimizer can cancel. We verify this by checking that
 * forward+reverse produces an empty circuit (all gates cancel).
 *
 * Without the fix (old bug), reversed X gates would have GateValue=-1 instead
 * of 1, causing gates_are_inverse() to return false and leaving uncancelled
 * gates in the circuit.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "circuit.h"
#include "execution.h"

/* Helper: count total gates in the circuit */
static int total_gate_count(circuit_t *circ) {
    int count = 0;
    for (int layer = 0; layer < (int)circ->used_layer; ++layer) {
        count += (int)circ->used_gates_per_layer[layer];
    }
    return count;
}

/* Helper: find the first gate matching a given Gate type and NumControls
 * in the layer range [start_layer, circ->used_layer).
 * Returns pointer to the gate, or NULL if not found. */
static gate_t *find_gate_in_range(circuit_t *circ, int start_layer, Standardgate_t gate_type,
                                  num_t num_controls) {
    for (int layer = start_layer; layer < (int)circ->used_layer; ++layer) {
        for (int g = 0; g < (int)circ->used_gates_per_layer[layer]; ++g) {
            gate_t *gate = &circ->sequence[layer][g];
            if (gate->Gate == gate_type && gate->NumControls == num_controls) {
                return gate;
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Test 1: X gate GateValue preserved -> forward+reverse cancels      */
/* ------------------------------------------------------------------ */
static void test_reverse_x_gate_preserves_value(void) {
    printf("test_reverse_x_gate_preserves_value... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    /* Add an X gate (GateValue=1) to qubit 0 */
    gate_t g;
    memset(&g, 0, sizeof(gate_t));
    x(&g, 0);
    add_gate(circ, &g);

    int end_layer = (int)circ->used_layer;
    assert(end_layer > 0);

    /* Reverse the circuit range -- reversed X should cancel with original X
     * because GateValue is preserved as 1.0 (not corrupted to -1.0) */
    reverse_circuit_range(circ, 0, end_layer);

    /* With the fix: X(1) + X(1) -> cancels (gates_are_inverse sees matching GateValues)
     * Without fix: X(1) + X(-1) -> doesn't cancel (GateValues differ) */
    int count = total_gate_count(circ);
    assert(count == 0 && "X gate should cancel with its reverse (GateValue preserved)");

    printf("PASS\n");
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 2: CCX (Toffoli) gate GateValue preserved -> cancels          */
/* ------------------------------------------------------------------ */
static void test_reverse_ccx_gate_preserves_value(void) {
    printf("test_reverse_ccx_gate_preserves_value... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    /* Add a CCX gate (Toffoli): target=0, controls=1,2 */
    gate_t g;
    memset(&g, 0, sizeof(gate_t));
    ccx(&g, 0, 1, 2);
    add_gate(circ, &g);

    int end_layer = (int)circ->used_layer;
    assert(end_layer > 0);

    reverse_circuit_range(circ, 0, end_layer);

    /* CCX should cancel with its reverse */
    int count = total_gate_count(circ);
    assert(count == 0 && "CCX gate should cancel with its reverse (GateValue preserved)");

    printf("PASS\n");
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 3: CX gate GateValue preserved -> cancels                     */
/* ------------------------------------------------------------------ */
static void test_reverse_cx_gate_preserves_value(void) {
    printf("test_reverse_cx_gate_preserves_value... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    /* Add a CX gate: target=0, control=1 */
    gate_t g;
    memset(&g, 0, sizeof(gate_t));
    cx(&g, 0, 1);
    add_gate(circ, &g);

    int end_layer = (int)circ->used_layer;
    assert(end_layer > 0);

    reverse_circuit_range(circ, 0, end_layer);

    /* CX should cancel with its reverse */
    int count = total_gate_count(circ);
    assert(count == 0 && "CX gate should cancel with its reverse (GateValue preserved)");

    printf("PASS\n");
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 4: P gate GateValue negated -> cancels                        */
/* ------------------------------------------------------------------ */
static void test_reverse_p_gate_negates_value(void) {
    printf("test_reverse_p_gate_negates_value... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    /* Add a P gate with GateValue=1.5 to qubit 0 */
    gate_t g;
    memset(&g, 0, sizeof(gate_t));
    p(&g, 0, 1.5);
    add_gate(circ, &g);

    int end_layer = (int)circ->used_layer;
    assert(end_layer > 0);

    reverse_circuit_range(circ, 0, end_layer);

    /* P(1.5) + P(-1.5) should cancel as inverses */
    int count = total_gate_count(circ);
    assert(count == 0 && "P gate should cancel with its negated reverse");

    printf("PASS\n");
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 5: H gate GateValue preserved -> cancels                      */
/* ------------------------------------------------------------------ */
static void test_reverse_h_gate_preserves_value(void) {
    printf("test_reverse_h_gate_preserves_value... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    /* Add an H gate (GateValue=0) to qubit 0 */
    gate_t g;
    memset(&g, 0, sizeof(gate_t));
    h(&g, 0);
    add_gate(circ, &g);

    int end_layer = (int)circ->used_layer;
    assert(end_layer > 0);

    reverse_circuit_range(circ, 0, end_layer);

    /* H(0) + H(0) should cancel (H is self-inverse) */
    int count = total_gate_count(circ);
    assert(count == 0 && "H gate should cancel with its reverse (GateValue preserved)");

    printf("PASS\n");
    free_circuit(circ);
}

/* ------------------------------------------------------------------ */
/* Test 6: Mixed circuit -> all gates cancel after forward+reverse    */
/* ------------------------------------------------------------------ */
static void test_reverse_mixed_circuit(void) {
    printf("test_reverse_mixed_circuit... ");
    fflush(stdout);

    circuit_t *circ = init_circuit();
    assert(circ != NULL);

    /* Add a mixed sequence of gates on different qubits:
     * X(q0), P(q1, 0.5), CCX(q2, q3, q4), H(q5)
     * These are on non-overlapping qubits so each is placed independently. */
    gate_t g;

    memset(&g, 0, sizeof(gate_t));
    x(&g, 0);
    add_gate(circ, &g);

    memset(&g, 0, sizeof(gate_t));
    p(&g, 1, 0.5);
    add_gate(circ, &g);

    memset(&g, 0, sizeof(gate_t));
    ccx(&g, 2, 3, 4);
    add_gate(circ, &g);

    memset(&g, 0, sizeof(gate_t));
    h(&g, 5);
    add_gate(circ, &g);

    int count_before = total_gate_count(circ);
    assert(count_before == 4 && "Should have 4 original gates");

    int end_layer = (int)circ->used_layer;

    /* Reverse the entire circuit range */
    reverse_circuit_range(circ, 0, end_layer);

    /* All gates should cancel with their reversed counterparts:
     * - X(q0, val=1) cancels with reversed X(q0, val=1) [self-inverse, value preserved]
     * - P(q1, val=0.5) cancels with reversed P(q1, val=-0.5) [rotation, value negated]
     * - CCX(q2,q3,q4, val=1) cancels with reversed CCX(q2,q3,q4, val=1) [self-inverse]
     * - H(q5, val=0) cancels with reversed H(q5, val=0) [self-inverse]
     */
    int count_after = total_gate_count(circ);
    assert(count_after == 0 && "All gates should cancel after forward+reverse");

    printf("PASS\n");
    free_circuit(circ);
}

int main(void) {
    printf("=== reverse_circuit_range C unit tests (Phase 65) ===\n\n");

    test_reverse_x_gate_preserves_value();
    test_reverse_ccx_gate_preserves_value();
    test_reverse_cx_gate_preserves_value();
    test_reverse_p_gate_negates_value();
    test_reverse_h_gate_preserves_value();
    test_reverse_mixed_circuit();

    printf("\n=== ALL 6 TESTS PASSED ===\n");
    return 0;
}
