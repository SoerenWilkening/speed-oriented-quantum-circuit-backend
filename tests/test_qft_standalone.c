/**
 * @file test_qft_standalone.c
 * @brief Functional + structural tests for the bare QFT/IQFT sequence builders.
 *
 * Issue: refactor-6s9
 *
 * Covers PLAN_qft_standalone.md Step 4 test cases:
 *   1. Literal per-layer (target, control, angle) tuples for n=3 and n=4.
 *   2. n=1 forward: single H, no CP rotations.
 *   3. n=1 inverse: single H, no CP rotations.
 *   4. n=0 forward and inverse: zero used layers.
 *   5. Symmetry: qc_qft_seq + qc_iqft_seq compose to identity (per-layer
 *      gate-multiset reversal check).
 *   6. n>64 error path on the wrappers.
 *   7. Replay through qc_run_instruction on a non-contiguous qmap honors
 *      the qubit mapping.
 *
 * Round-trip identity against a real simulator lives in
 * tests/test_qasm_sim_qft_standalone.py — running unitary simulation in
 * pure C is out of scope for this package.
 */

#include "quantum_circuit.h"
#include "internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====================================================================== */
/* Test infrastructure                                                     */
/* ====================================================================== */

static int g_run = 0, g_pass = 0, g_fail = 0;

#define TEST(name) do { g_run++; printf("  %-60s ", name); } while (0)
#define PASS()     do { g_pass++; printf("PASS\n"); } while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { g_fail++; printf("FAIL: %s (line %d)\n", msg, __LINE__); return; } \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((long long)(a) != (long long)(b)) { \
        g_fail++; printf("FAIL: %s -- exp %lld got %lld (line %d)\n", \
            msg, (long long)(b), (long long)(a), __LINE__); return; } \
} while (0)

#define ASSERT_NEAR(a, b, msg) do { \
    double _da = (double)(a), _db = (double)(b); \
    if (fabs(_da - _db) > 1e-12) { \
        g_fail++; printf("FAIL: %s -- exp %.17g got %.17g (line %d)\n", \
            msg, _db, _da, __LINE__); return; } \
} while (0)

/* ====================================================================== */
/* Expected-tuple table representation                                     */
/* ====================================================================== */

typedef struct {
    int is_h;          /* 1 = H, 0 = CP */
    uint32_t target;
    uint32_t control;  /* unused if is_h */
    double angle;      /* unused if is_h */
} expected_gate_t;

static int gate_matches(const qc_gate_internal_t *g,
                        const expected_gate_t *e) {
    if (e->is_h) {
        return g->Gate == QC_IGATE_H && g->NumControls == 0
            && g->Target == e->target;
    } else {
        if (g->Gate != QC_IGATE_P) return 0;
        if (g->NumControls != 1) return 0;
        if (g->Target != e->target) return 0;
        if (qc_get_control(g, 0) != e->control) return 0;
        if (fabs(g->GateValue - e->angle) > 1e-12) return 0;
        return 1;
    }
}

static int check_layer(qc_sequence_t *seq, uint32_t layer,
                       const expected_gate_t *exp, uint32_t n_exp,
                       const char *label) {
    if (seq->gates_per_layer[layer] != n_exp) {
        printf("FAIL: %s layer %u count exp %u got %u\n",
               label, layer, n_exp, seq->gates_per_layer[layer]);
        return 0;
    }
    for (uint32_t i = 0; i < n_exp; ++i) {
        if (!gate_matches(&seq->seq[layer][i], &exp[i])) {
            printf("FAIL: %s layer %u gate %u mismatch "
                   "(got Gate=%d Tgt=%u NCtrl=%u Val=%.17g)\n",
                   label, layer, i,
                   (int)seq->seq[layer][i].Gate,
                   seq->seq[layer][i].Target,
                   seq->seq[layer][i].NumControls,
                   seq->seq[layer][i].GateValue);
            return 0;
        }
    }
    return 1;
}

/* ====================================================================== */
/* 1a. Literal per-layer test for n = 3                                    */
/* ====================================================================== */

static void test_qft_seq_n3_literal(void) {
    TEST("qft_seq n=3: literal (target,control,angle) per layer");

    qc_sequence_t *seq = qc_qft_seq(3);
    ASSERT(seq != NULL, "qc_qft_seq(3) returned NULL");
    ASSERT_EQ(seq->used_layer, 5, "used_layer mismatch");
    ASSERT_EQ(seq->total_qubits, 3, "total_qubits mismatch");
    ASSERT_EQ(seq->total_gate_count, 6, "total_gate_count mismatch");

    /* Hand-derived from the reference loop in PRD §6:
     *  j=0: q=2, L0 H(2); L1 CP(2,1,π/2); L2 CP(2,0,π/4)
     *  j=1: q=1, L2 H(1); L3 CP(1,0,π/2)
     *  j=2: q=0, L4 H(0)
     */
    expected_gate_t L0[] = {{1, 2, 0, 0.0}};
    expected_gate_t L1[] = {{0, 2, 1, M_PI / 2.0}};
    expected_gate_t L2[] = {{0, 2, 0, M_PI / 4.0}, {1, 1, 0, 0.0}};
    expected_gate_t L3[] = {{0, 1, 0, M_PI / 2.0}};
    expected_gate_t L4[] = {{1, 0, 0, 0.0}};

    int ok = 1;
    ok &= check_layer(seq, 0, L0, 1, "qft n=3");
    ok &= check_layer(seq, 1, L1, 1, "qft n=3");
    ok &= check_layer(seq, 2, L2, 2, "qft n=3");
    ok &= check_layer(seq, 3, L3, 1, "qft n=3");
    ok &= check_layer(seq, 4, L4, 1, "qft n=3");
    ASSERT(ok, "literal per-layer mismatch (n=3)");

    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* 1b. Literal per-layer test for n = 4                                    */
/* ====================================================================== */

static void test_qft_seq_n4_literal(void) {
    TEST("qft_seq n=4: literal (target,control,angle) per layer");

    qc_sequence_t *seq = qc_qft_seq(4);
    ASSERT(seq != NULL, "qc_qft_seq(4) returned NULL");
    ASSERT_EQ(seq->used_layer, 7, "used_layer mismatch");
    ASSERT_EQ(seq->total_qubits, 4, "total_qubits mismatch");
    ASSERT_EQ(seq->total_gate_count, 10, "total_gate_count mismatch");

    /* Hand-derived:
     *  j=0: q=3, L0 H(3); L1 CP(3,2,π/2); L2 CP(3,1,π/4); L3 CP(3,0,π/8)
     *  j=1: q=2, L2 H(2);            L3 CP(2,1,π/2); L4 CP(2,0,π/4)
     *  j=2: q=1, L4 H(1);            L5 CP(1,0,π/2)
     *  j=3: q=0, L6 H(0)
     */
    expected_gate_t L0[] = {{1, 3, 0, 0.0}};
    expected_gate_t L1[] = {{0, 3, 2, M_PI / 2.0}};
    expected_gate_t L2[] = {{0, 3, 1, M_PI / 4.0}, {1, 2, 0, 0.0}};
    expected_gate_t L3[] = {{0, 3, 0, M_PI / 8.0}, {0, 2, 1, M_PI / 2.0}};
    expected_gate_t L4[] = {{0, 2, 0, M_PI / 4.0}, {1, 1, 0, 0.0}};
    expected_gate_t L5[] = {{0, 1, 0, M_PI / 2.0}};
    expected_gate_t L6[] = {{1, 0, 0, 0.0}};

    int ok = 1;
    ok &= check_layer(seq, 0, L0, 1, "qft n=4");
    ok &= check_layer(seq, 1, L1, 1, "qft n=4");
    ok &= check_layer(seq, 2, L2, 2, "qft n=4");
    ok &= check_layer(seq, 3, L3, 2, "qft n=4");
    ok &= check_layer(seq, 4, L4, 2, "qft n=4");
    ok &= check_layer(seq, 5, L5, 1, "qft n=4");
    ok &= check_layer(seq, 6, L6, 1, "qft n=4");
    ASSERT(ok, "literal per-layer mismatch (n=4)");

    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* 2. n=1 forward                                                          */
/* ====================================================================== */

static void test_qft_seq_n1(void) {
    TEST("qft_seq n=1: single H, no CP rotations");

    qc_sequence_t *seq = qc_qft_seq(1);
    ASSERT(seq != NULL, "returned NULL");
    ASSERT_EQ(seq->used_layer, 1, "used_layer should be 1");
    ASSERT_EQ(seq->total_qubits, 1, "total_qubits should be 1");
    ASSERT_EQ(seq->total_gate_count, 1, "should have exactly 1 gate");
    ASSERT_EQ(seq->gates_per_layer[0], 1, "layer 0 should have 1 gate");
    ASSERT_EQ(seq->seq[0][0].Gate, QC_IGATE_H, "expected H");
    ASSERT_EQ(seq->seq[0][0].Target, 0, "H target should be 0");
    ASSERT_EQ(seq->seq[0][0].NumControls, 0, "H must have no controls");
    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* 3. n=1 inverse                                                          */
/* ====================================================================== */

static void test_iqft_seq_n1(void) {
    TEST("iqft_seq n=1: single H, no CP rotations");

    qc_sequence_t *seq = qc_iqft_seq(1);
    ASSERT(seq != NULL, "returned NULL");
    ASSERT_EQ(seq->used_layer, 1, "used_layer should be 1");
    ASSERT_EQ(seq->total_qubits, 1, "total_qubits should be 1");
    ASSERT_EQ(seq->total_gate_count, 1, "should have exactly 1 gate");
    ASSERT_EQ(seq->gates_per_layer[0], 1, "layer 0 should have 1 gate");
    ASSERT_EQ(seq->seq[0][0].Gate, QC_IGATE_H, "expected H");
    ASSERT_EQ(seq->seq[0][0].Target, 0, "H target should be 0");
    ASSERT_EQ(seq->seq[0][0].NumControls, 0, "H must have no controls");
    qc_sequence_free(seq);
    PASS();
}

/* ====================================================================== */
/* 4. n=0 forward + inverse                                                */
/* ====================================================================== */

static void test_qft_seq_n0(void) {
    TEST("qft_seq/iqft_seq n=0: empty valid sequences");

    qc_sequence_t *fwd = qc_qft_seq(0);
    ASSERT(fwd != NULL, "qft_seq(0) returned NULL");
    ASSERT_EQ(fwd->used_layer, 0, "fwd used_layer must be 0");
    ASSERT_EQ(fwd->total_gate_count, 0, "fwd total_gate_count must be 0");
    qc_sequence_free(fwd);

    qc_sequence_t *inv = qc_iqft_seq(0);
    ASSERT(inv != NULL, "iqft_seq(0) returned NULL");
    ASSERT_EQ(inv->used_layer, 0, "inv used_layer must be 0");
    ASSERT_EQ(inv->total_gate_count, 0, "inv total_gate_count must be 0");
    qc_sequence_free(inv);

    PASS();
}

/* ====================================================================== */
/* 5. Symmetry: forward then inverse cancels structurally for n=3          */
/*                                                                         */
/* Strategy: applying qc_qft followed by qc_iqft on a basis state via the  */
/* count-only mode just stacks gates. For deterministic recovery we rely   */
/* on the Python QASM-simulation test. Here we verify the structural       */
/* invariant required by the PLAN: the inverse sequence is exactly the    */
/* reverse-layer image of the forward sequence with negated CP angles.    */
/* ====================================================================== */

static void test_qft_iqft_symmetry_n3(void) {
    TEST("qft/iqft symmetry n=3: inverse is reversed forward with -angles");

    int n = 3;
    qc_sequence_t *fwd = qc_qft_seq(n);
    qc_sequence_t *inv = qc_iqft_seq(n);
    ASSERT(fwd != NULL && inv != NULL, "build returned NULL");
    ASSERT_EQ(fwd->used_layer, inv->used_layer, "layer count mismatch");
    ASSERT_EQ(fwd->total_gate_count, inv->total_gate_count,
              "total gate count mismatch");

    uint32_t L = fwd->used_layer;
    int ok = 1;
    for (uint32_t k = 0; k < L; ++k) {
        uint32_t mirror = L - 1 - k;
        if (fwd->gates_per_layer[k] != inv->gates_per_layer[mirror]) {
            printf("FAIL: layer %u gate count %u vs mirror %u gate count %u\n",
                   k, fwd->gates_per_layer[k],
                   mirror, inv->gates_per_layer[mirror]);
            ok = 0;
            break;
        }

        /* For each gate in fwd layer k, find a matching gate in
         * inv layer mirror with opposite-sign angle (or matching H).
         * The within-layer order is not part of the contract, so do
         * a multiset match. */
        uint32_t m = fwd->gates_per_layer[k];
        int matched[8] = {0};
        for (uint32_t i = 0; i < m; ++i) {
            const qc_gate_internal_t *gf = &fwd->seq[k][i];
            int found = 0;
            for (uint32_t j = 0; j < m; ++j) {
                if (matched[j]) continue;
                const qc_gate_internal_t *gi = &inv->seq[mirror][j];
                if (gf->Gate != gi->Gate) continue;
                if (gf->Target != gi->Target) continue;
                if (gf->NumControls != gi->NumControls) continue;
                if (gf->NumControls == 1
                    && qc_get_control(gf, 0) != qc_get_control(gi, 0))
                    continue;
                if (gf->Gate == QC_IGATE_P) {
                    if (fabs(gf->GateValue + gi->GateValue) > 1e-12)
                        continue;
                }
                matched[j] = 1;
                found = 1;
                break;
            }
            if (!found) { ok = 0; break; }
        }
        if (!ok) break;
    }
    ASSERT(ok, "fwd/inv multiset mismatch");

    qc_sequence_free(fwd);
    qc_sequence_free(inv);
    PASS();
}

/* ====================================================================== */
/* 6. n>64 error path on wrappers                                          */
/* ====================================================================== */

static void test_wrapper_width_error(void) {
    TEST("qc_qft / qc_iqft: n>64 returns QC_ERR_WIDTH");

    circuit_ctx_t *ctx = qc_circuit_create(8);
    ASSERT(ctx != NULL, "ctx create");

    uint32_t dummy[1] = {0};
    qc_error_t e1 = qc_qft(ctx, dummy, 65);
    qc_error_t e2 = qc_iqft(ctx, dummy, 65);
    ASSERT_EQ(e1, QC_ERR_WIDTH, "qc_qft(65) should return QC_ERR_WIDTH");
    ASSERT_EQ(e2, QC_ERR_WIDTH, "qc_iqft(65) should return QC_ERR_WIDTH");

    /* Builders return NULL for n > 64. */
    ASSERT(qc_qft_seq(65) == NULL, "qc_qft_seq(65) should return NULL");
    ASSERT(qc_iqft_seq(65) == NULL, "qc_iqft_seq(65) should return NULL");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* 7. Round-trip on non-contiguous qmap: gates land on the chosen          */
/*    hardware qubits and only those qubits.                               */
/* ====================================================================== */

static void test_non_contiguous_qmap(void) {
    TEST("qc_qft + qc_iqft on non-contiguous qmap {0,2,4}");

    int n = 3;
    uint32_t qmap[3] = {0, 2, 4};

    circuit_ctx_t *ctx = qc_circuit_create(16);
    ASSERT(ctx != NULL, "ctx create");
    qc_circuit_set_simulate(ctx, true);

    /* Allocate enough qubits to cover index 4. */
    uint32_t start = 0;
    qc_qubit_alloc_n(ctx, 5, &start);

    qc_error_t e1 = qc_qft(ctx, qmap, (uint32_t)n);
    qc_error_t e2 = qc_iqft(ctx, qmap, (uint32_t)n);
    ASSERT_EQ(e1, QC_OK, "qc_qft failed");
    ASSERT_EQ(e2, QC_OK, "qc_iqft failed");

    uint64_t gc = qc_circuit_gate_count(ctx);
    /* 6 gates per QFT (n=3), 2x for fwd+inv = 12. */
    ASSERT_EQ(gc, 12, "expected 12 gates total");

    /* Inspect every emitted gate: it must touch only qubits {0,2,4}. */
    qc_exported_gate_t *gates = NULL;
    uint32_t count = 0;
    qc_error_t ee = qc_circuit_extract_gates(ctx, 0, qc_circuit_used_layer(ctx),
                                              &gates, &count);
    ASSERT_EQ(ee, QC_OK, "extract_gates failed");
    int ok = 1;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t t = gates[i].target;
        if (t != 0 && t != 2 && t != 4) { ok = 0; break; }
        for (uint32_t c = 0; c < gates[i].num_controls; ++c) {
            uint32_t cq = gates[i].controls[c];
            if (cq != 0 && cq != 2 && cq != 4) { ok = 0; break; }
        }
        if (!ok) break;
    }
    free(gates);
    ASSERT(ok, "gate touches a qubit outside qmap");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* 8. Wrapper error: n=0 is no-op QC_OK                                    */
/* ====================================================================== */

static void test_wrapper_n0_noop(void) {
    TEST("qc_qft / qc_iqft: n=0 is QC_OK no-op");

    circuit_ctx_t *ctx = qc_circuit_create(4);
    ASSERT(ctx != NULL, "ctx create");
    qc_circuit_set_simulate(ctx, true);

    qc_error_t e1 = qc_qft(ctx, NULL, 0);
    qc_error_t e2 = qc_iqft(ctx, NULL, 0);
    ASSERT_EQ(e1, QC_OK, "qc_qft(0) must be QC_OK");
    ASSERT_EQ(e2, QC_OK, "qc_iqft(0) must be QC_OK");
    ASSERT_EQ(qc_circuit_gate_count(ctx), 0, "no gates expected");

    qc_circuit_destroy(ctx);
    PASS();
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

int main(void) {
    printf("=== Standalone QFT/IQFT tests ===\n");

    test_qft_seq_n3_literal();
    test_qft_seq_n4_literal();
    test_qft_seq_n1();
    test_iqft_seq_n1();
    test_qft_seq_n0();
    test_qft_iqft_symmetry_n3();
    test_wrapper_width_error();
    test_wrapper_n0_noop();
    test_non_contiguous_qmap();

    printf("\n  Results: %d run, %d passed, %d failed\n",
           g_run, g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
