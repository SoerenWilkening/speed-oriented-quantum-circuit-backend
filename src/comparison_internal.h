/**
 * @file comparison_internal.h
 * @brief Internal header for integer comparison helpers shared across
 *        integer_comparison.c and integer_comparison_ctrl.c.
 *
 * Issue: refactor-4xn
 *
 * This header is NOT part of the public API.
 */

#ifndef QC_COMPARISON_INTERNAL_H
#define QC_COMPARISON_INTERNAL_H

#include "internal.h"
#include <stdint.h>

/* ====================================================================== */
/* Gate initializers for sequence building                                 */
/* ====================================================================== */

void qc_cmp_seq_gate_init(qc_gate_internal_t *g);
void qc_cmp_seq_gate_x(qc_gate_internal_t *g, uint32_t target);
void qc_cmp_seq_gate_cx(qc_gate_internal_t *g, uint32_t target, uint32_t control);
void qc_cmp_seq_gate_ccx(qc_gate_internal_t *g, uint32_t target,
                          uint32_t ctrl1, uint32_t ctrl2);

/* ====================================================================== */
/* Sequence allocation                                                     */
/* ====================================================================== */

qc_sequence_t *qc_cmp_alloc_sequence(int num_layers, int max_gates_per_layer);

/* ====================================================================== */
/* MCX decomposition                                                       */
/* ====================================================================== */

int  qc_cmp_mcx_decomp_layers(int num_controls);
void qc_cmp_emit_mcx_decomp(qc_sequence_t *seq, int *layer, uint32_t target,
                             const uint32_t *controls, int num_controls,
                             int anc_start);

/* ====================================================================== */
/* External: from integer.c                                                */
/* ====================================================================== */

extern int *qc_two_complement(int64_t x, int n);

/* ====================================================================== */
/* External: from hot_path_add.c                                           */
/* ====================================================================== */

extern qc_sequence_t *qc_split_cq_add_seq(int bits, int64_t value);
extern qc_sequence_t *qc_split_cq_sub_seq(int bits, int64_t value);

#endif /* QC_COMPARISON_INTERNAL_H */
