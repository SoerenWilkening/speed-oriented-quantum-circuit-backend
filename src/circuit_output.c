/**
 * @file circuit_output.c
 * @brief Circuit visualization and OpenQASM 3.0 export — refactored for ctx.
 *
 * Refactored from monolith Quantum_Assembly/c_backend/src/circuit_output.c.
 * All functions take circuit_ctx_t* instead of circuit_t*.
 *
 * Public API implemented:
 *   - qc_circuit_to_qasm()
 *   - qc_circuit_to_qasm_file()
 *   - qc_circuit_print_stats()
 *   - qc_circuit_visualize()
 *
 * @see quantum_circuit.h for public declarations.
 * @see internal.h for circuit_ctx struct definition.
 */

#include "internal.h"
#include <inttypes.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====================================================================== */
/* Static helpers                                                          */
/* ====================================================================== */

/** @brief Normalize an angle to [0, 2*pi). */
static double normalize_angle(double theta) {
    double result = fmod(theta, 2.0 * M_PI);
    if (result < 0.0) {
        result += 2.0 * M_PI;
    }
    return result;
}

/** @brief Count measurement gates in the circuit. */
static int count_measurements(const circuit_ctx_t *ctx) {
    int count = 0;
    for (uint32_t layer = 0; layer < ctx->used_layer; layer++) {
        for (uint32_t gi = 0; gi < ctx->used_gates_per_layer[layer]; gi++) {
            if (ctx->sequence[layer][gi].Gate == QC_IGATE_M) {
                count++;
            }
        }
    }
    return count;
}

/**
 * @brief Get the QASM name string for a gate type (uncontrolled).
 * @return Static string or NULL if gate type has no simple name.
 */
static const char *gate_qasm_name(qc_igate_t gate) {
    switch (gate) {
    case QC_IGATE_X:   return "x";
    case QC_IGATE_Y:   return "y";
    case QC_IGATE_Z:   return "z";
    case QC_IGATE_H:   return "h";
    case QC_IGATE_P:   return "p";
    case QC_IGATE_RX:  return "rx";
    case QC_IGATE_RY:  return "ry";
    case QC_IGATE_RZ:  return "rz";
    case QC_IGATE_T:   return "t";
    case QC_IGATE_TDG: return "tdg";
    case QC_IGATE_R:   return NULL;
    case QC_IGATE_M:   return NULL;
    default:           return NULL;
    }
}

/** @brief Check whether a gate type has an angle parameter. */
static bool gate_has_angle(qc_igate_t gate) {
    return gate == QC_IGATE_P  || gate == QC_IGATE_RX ||
           gate == QC_IGATE_RY || gate == QC_IGATE_RZ;
}

/**
 * @brief Export a single gate to a text buffer in OpenQASM 3.0 format.
 *
 * @param g              Gate to export.
 * @param buffer         Output buffer.
 * @param buf_size       Total buffer capacity.
 * @param offset         Current write position.
 * @param measurement_idx  Pointer to running measurement counter.
 * @return New offset after writing, or (size_t)-1 if buffer is too small.
 */
static size_t export_gate(const qc_gate_internal_t *g, char *buffer,
                          size_t buf_size, size_t offset,
                          int *measurement_idx) {
    /* Ensure at least 256 bytes headroom */
    if (offset > buf_size - 256) {
        return (size_t)-1;
    }

    int written = 0;

    if (g->NumControls == 0) {
        /* No controls — simple gate */
        switch (g->Gate) {
        case QC_IGATE_X:
            written = sprintf(buffer + offset, "x q[%u];\n", g->Target);
            break;
        case QC_IGATE_Y:
            written = sprintf(buffer + offset, "y q[%u];\n", g->Target);
            break;
        case QC_IGATE_Z:
            written = sprintf(buffer + offset, "z q[%u];\n", g->Target);
            break;
        case QC_IGATE_H:
            written = sprintf(buffer + offset, "h q[%u];\n", g->Target);
            break;
        case QC_IGATE_P:
            written = sprintf(buffer + offset, "p(%.17g) q[%u];\n",
                              normalize_angle(g->GateValue), g->Target);
            break;
        case QC_IGATE_RX:
            written = sprintf(buffer + offset, "rx(%.17g) q[%u];\n",
                              normalize_angle(g->GateValue), g->Target);
            break;
        case QC_IGATE_RY:
            written = sprintf(buffer + offset, "ry(%.17g) q[%u];\n",
                              normalize_angle(g->GateValue), g->Target);
            break;
        case QC_IGATE_RZ:
            written = sprintf(buffer + offset, "rz(%.17g) q[%u];\n",
                              normalize_angle(g->GateValue), g->Target);
            break;
        case QC_IGATE_M:
            written = sprintf(buffer + offset, "c[%d] = measure q[%u];\n",
                              *measurement_idx, g->Target);
            (*measurement_idx)++;
            break;
        case QC_IGATE_R:
            written = sprintf(buffer + offset, "reset q[%u];\n", g->Target);
            break;
        case QC_IGATE_T:
            written = sprintf(buffer + offset, "t q[%u];\n", g->Target);
            break;
        case QC_IGATE_TDG:
            written = sprintf(buffer + offset, "tdg q[%u];\n", g->Target);
            break;
        default:
            written = sprintf(buffer + offset, "// unknown gate %d\n", g->Gate);
            break;
        }
    } else if (g->NumControls == 1) {
        /* Single control — c-prefix syntax */
        uint32_t ctrl = qc_get_control(g, 0);
        switch (g->Gate) {
        case QC_IGATE_X:
            written = sprintf(buffer + offset, "cx q[%u], q[%u];\n",
                              ctrl, g->Target);
            break;
        case QC_IGATE_Y:
            written = sprintf(buffer + offset, "cy q[%u], q[%u];\n",
                              ctrl, g->Target);
            break;
        case QC_IGATE_Z:
            written = sprintf(buffer + offset, "cz q[%u], q[%u];\n",
                              ctrl, g->Target);
            break;
        case QC_IGATE_H:
            written = sprintf(buffer + offset, "ch q[%u], q[%u];\n",
                              ctrl, g->Target);
            break;
        case QC_IGATE_P:
            written = sprintf(buffer + offset, "cp(%.17g) q[%u], q[%u];\n",
                              normalize_angle(g->GateValue), ctrl, g->Target);
            break;
        case QC_IGATE_RX:
            written = sprintf(buffer + offset, "crx(%.17g) q[%u], q[%u];\n",
                              normalize_angle(g->GateValue), ctrl, g->Target);
            break;
        case QC_IGATE_RY:
            written = sprintf(buffer + offset, "cry(%.17g) q[%u], q[%u];\n",
                              normalize_angle(g->GateValue), ctrl, g->Target);
            break;
        case QC_IGATE_RZ:
            written = sprintf(buffer + offset, "crz(%.17g) q[%u], q[%u];\n",
                              normalize_angle(g->GateValue), ctrl, g->Target);
            break;
        case QC_IGATE_M:
        case QC_IGATE_R:
            written = sprintf(buffer + offset, "// skipped controlled %s\n",
                              g->Gate == QC_IGATE_M ? "measure" : "reset");
            break;
        default:
            written = sprintf(buffer + offset,
                              "// unknown controlled gate %d\n", g->Gate);
            break;
        }
    } else if (g->NumControls == 2) {
        /* Two controls — ccx or ctrl(2) @ syntax */
        uint32_t ctrl0 = qc_get_control(g, 0);
        uint32_t ctrl1 = qc_get_control(g, 1);

        if (g->Gate == QC_IGATE_X) {
            written = sprintf(buffer + offset,
                              "ccx q[%u], q[%u], q[%u];\n",
                              ctrl0, ctrl1, g->Target);
        } else {
            const char *name = gate_qasm_name(g->Gate);
            if (name == NULL) name = "unknown";
            char param[80] = "";
            if (gate_has_angle(g->Gate)) {
                sprintf(param, "(%.17g)", normalize_angle(g->GateValue));
            }
            written = sprintf(buffer + offset,
                              "ctrl(2) @ %s%s q[%u], q[%u], q[%u];\n",
                              name, param, ctrl0, ctrl1, g->Target);
        }
    } else {
        /* 3+ controls — ctrl(n) @ syntax */
        const char *name = gate_qasm_name(g->Gate);
        if (name == NULL) name = "unknown";
        char param[80] = "";
        if (gate_has_angle(g->Gate)) {
            sprintf(param, "(%.17g)", normalize_angle(g->GateValue));
        }

        written = sprintf(buffer + offset, "ctrl(%u) @ %s%s ",
                          g->NumControls, name, param);
        offset += (size_t)written;

        for (uint32_t i = 0; i < g->NumControls; i++) {
            written = sprintf(buffer + offset, "q[%u], ",
                              qc_get_control(g, (int)i));
            offset += (size_t)written;
        }

        written = sprintf(buffer + offset, "q[%u];\n", g->Target);
    }

    return offset + (size_t)written;
}

/* ====================================================================== */
/* Public API — QASM export                                                */
/* ====================================================================== */

QC_API char *qc_circuit_to_qasm(const circuit_ctx_t *ctx) {
    if (ctx == NULL) {
        return NULL;
    }

    int num_measurements = count_measurements(ctx);

    /* Initial buffer: header + estimated 100 bytes per gate */
    size_t buf_size = 512 + (ctx->gate_count * 100);
    if (buf_size < 1024) buf_size = 1024;
    char *buffer = malloc(buf_size);
    if (buffer == NULL) {
        return NULL;
    }

    size_t offset = 0;

    /* Compute qubit count: max of tracked usage and allocator high-water mark */
    uint32_t qubit_count = ctx->used_qubits + 1;
    if (ctx->allocator != NULL && ctx->allocator->next_qubit > qubit_count) {
        qubit_count = ctx->allocator->next_qubit;
    }

    /* Header */
    int written = sprintf(buffer + offset,
                          "OPENQASM 3.0;\n"
                          "include \"stdgates.inc\";\n"
                          "\n"
                          "qubit[%u] q;\n",
                          qubit_count);
    offset += (size_t)written;

    /* Classical register for measurements */
    if (num_measurements > 0) {
        written = sprintf(buffer + offset, "bit[%d] c;\n", num_measurements);
        offset += (size_t)written;
    }

    /* Separator */
    written = sprintf(buffer + offset, "\n");
    offset += (size_t)written;

    /* Export all gates layer-by-layer */
    int measurement_idx = 0;
    for (uint32_t layer = 0; layer < ctx->used_layer; layer++) {
        for (uint32_t gi = 0; gi < ctx->used_gates_per_layer[layer]; gi++) {
            const qc_gate_internal_t *g = &ctx->sequence[layer][gi];
            size_t new_offset = export_gate(g, buffer, buf_size, offset,
                                            &measurement_idx);

            /* Grow buffer on overflow and retry */
            while (new_offset == (size_t)-1) {
                buf_size *= 2;
                char *tmp = realloc(buffer, buf_size);
                if (tmp == NULL) {
                    free(buffer);
                    return NULL;
                }
                buffer = tmp;
                new_offset = export_gate(g, buffer, buf_size, offset,
                                         &measurement_idx);
            }
            offset = new_offset;
        }
    }

    buffer[offset] = '\0';
    return buffer;
}

QC_API qc_error_t qc_circuit_to_qasm_file(const circuit_ctx_t *ctx,
                                            const char *path) {
    if (ctx == NULL || path == NULL) {
        return QC_ERR_NULL;
    }

    char *qasm = qc_circuit_to_qasm(ctx);
    if (qasm == NULL) {
        return QC_ERR_ALLOC;
    }

    FILE *f = fopen(path, "w");
    if (f == NULL) {
        free(qasm);
        return QC_ERR_IO;
    }

    fputs(qasm, f);
    fclose(f);
    free(qasm);
    return QC_OK;
}

/* ====================================================================== */
/* Public API — visualization                                              */
/* ====================================================================== */

/**
 * @brief Compute visual sub-column assignments for gates in a single layer.
 *
 * Gates whose qubit ranges [min_qubit, max_qubit] overlap are placed into
 * separate sub-columns.  Non-overlapping gates share a sub-column.
 * This is purely for visualization — no circuit data is modified.
 *
 * Uses greedy interval coloring: sort gates by min qubit, assign each to
 * the first sub-column whose highest qubit is below the gate's min qubit.
 *
 * @param ctx           Circuit context.
 * @param layer         Layer index.
 * @param subcol_of_gate Output array: subcol_of_gate[gi] = sub-column index.
 * @param max_gates     Capacity of subcol_of_gate array.
 * @return Number of sub-columns (>= 1).
 */
static uint32_t compute_visual_subcols(const circuit_ctx_t *ctx,
                                       uint32_t layer,
                                       uint32_t *subcol_of_gate,
                                       uint32_t max_gates) {
    uint32_t n = ctx->used_gates_per_layer[layer];
    if (n == 0) return 1;
    if (n > max_gates) n = max_gates;

    /* Build sort indices by min qubit (insertion sort — n is small) */
    uint32_t sorted[QC_GATES_PER_LAYER_BLOCK];
    for (uint32_t i = 0; i < n; i++) sorted[i] = i;

    for (uint32_t i = 1; i < n; i++) {
        uint32_t key = sorted[i];
        uint32_t key_min = qc_min_qubit(&ctx->sequence[layer][key]);
        int j = (int)i - 1;
        while (j >= 0 &&
               qc_min_qubit(&ctx->sequence[layer][sorted[j]]) > key_min) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    /* Greedy assign: subcol_max[sc] = highest qubit used by sub-column sc */
    uint32_t subcol_max[QC_GATES_PER_LAYER_BLOCK];
    uint32_t num_subcols = 0;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t gi = sorted[i];
        uint32_t gmin = qc_min_qubit(&ctx->sequence[layer][gi]);
        uint32_t gmax = qc_max_qubit(&ctx->sequence[layer][gi]);

        /* Find first sub-column where max qubit < this gate's min qubit */
        uint32_t assigned = UINT32_MAX;
        for (uint32_t sc = 0; sc < num_subcols; sc++) {
            if (subcol_max[sc] < gmin) {
                assigned = sc;
                break;
            }
        }

        if (assigned == UINT32_MAX) {
            /* New sub-column needed */
            assigned = num_subcols++;
        }

        subcol_of_gate[gi] = assigned;
        if (gmax > subcol_max[assigned] || assigned == num_subcols - 1)
            subcol_max[assigned] = gmax;
    }

    return num_subcols > 0 ? num_subcols : 1;
}

/** @brief Print a gate symbol for the visualization. */
static void vis_print_gate(const qc_gate_internal_t *g, uint32_t qubit) {
    if (g->Target == qubit) {
        switch (g->Gate) {
        case QC_IGATE_X:
            printf(g->NumControls > 0 ? " + " : " X ");
            break;
        case QC_IGATE_H:   printf(" H ");   break;
        case QC_IGATE_Z:   printf(" Z ");   break;
        case QC_IGATE_Y:   printf(" Y ");   break;
        case QC_IGATE_P:   printf(" P ");   break;
        case QC_IGATE_M:   printf(" M ");   break;
        case QC_IGATE_T:   printf(" T ");   break;
        case QC_IGATE_TDG: printf("Tdg");   break;
        default:           printf(" ? ");   break;
        }
    } else {
        printf(" @ ");
    }
}

QC_API void qc_circuit_visualize(const circuit_ctx_t *ctx) {
    if (ctx == NULL) {
        printf("Circuit is NULL\n");
        return;
    }

    printf("Circuit: %zu gates, %u layers, %u qubits\n\n",
           ctx->gate_count, ctx->used_layer, ctx->used_qubits + 1);

    if (ctx->gate_count == 0) {
        printf("(empty circuit)\n");
        return;
    }

    uint32_t max_display = ctx->used_layer > 60 ? 60 : ctx->used_layer;
    if (ctx->used_layer > 60) {
        printf("(showing first 60 of %u layers)\n\n", ctx->used_layer);
    }

    /* Pre-compute sub-column assignments for all displayed layers */
    uint32_t subcol_data[60][QC_GATES_PER_LAYER_BLOCK];
    uint32_t subcol_count[60];

    for (uint32_t layer = 0; layer < max_display; layer++) {
        subcol_count[layer] = compute_visual_subcols(
            ctx, layer, subcol_data[layer], QC_GATES_PER_LAYER_BLOCK);
    }

    /* Layer header — each layer occupies 3 * subcol_count characters */
    printf("     ");
    for (uint32_t layer = 0; layer < max_display; layer++) {
        uint32_t width = 3 * subcol_count[layer];
        if (layer % 5 == 0) {
            printf("%-*u", (int)width, layer);
        } else {
            printf("%*s", (int)width, "");
        }
    }
    printf("\n");

    /* Qubit rows */
    for (uint32_t qubit = 0; qubit <= ctx->used_qubits; qubit++) {
        if (ctx->used_occupation_indices_per_qubit == NULL ||
            ctx->used_occupation_indices_per_qubit[qubit] == 0) {
            continue;
        }

        printf("q%-3u ", qubit);

        for (uint32_t layer = 0; layer < max_display; layer++) {
            for (uint32_t sc = 0; sc < subcol_count[layer]; sc++) {
                /* Look up whether this qubit has a gate in this layer */
                int gate_idx = -1;
                if (ctx->gate_index_of_layer_and_qubits != NULL &&
                    ctx->gate_index_of_layer_and_qubits[layer] != NULL) {
                    gate_idx =
                        ctx->gate_index_of_layer_and_qubits[layer][qubit];
                }

                /* Check if this gate belongs to the current sub-column */
                if (gate_idx >= 0 &&
                    (uint32_t)gate_idx < ctx->used_gates_per_layer[layer] &&
                    subcol_data[layer][gate_idx] == sc) {
                    vis_print_gate(&ctx->sequence[layer][gate_idx], qubit);
                } else {
                    /* Check if wire passes between control and target
                     * for any gate in THIS sub-column only */
                    bool is_between = false;
                    for (uint32_t gi = 0;
                         gi < ctx->used_gates_per_layer[layer]; gi++) {
                        if (subcol_data[layer][gi] != sc) continue;
                        const qc_gate_internal_t *g =
                            &ctx->sequence[layer][gi];
                        uint32_t minq = qc_min_qubit(g);
                        uint32_t maxq = qc_max_qubit(g);
                        if (qubit > minq && qubit < maxq) {
                            is_between = true;
                            break;
                        }
                    }
                    printf(is_between ? " | " : "---");
                }
            }
        }
        printf("\n");
    }
    printf("\n");
}

/* ====================================================================== */
/* Public API — print stats                                                */
/* ====================================================================== */

QC_API void qc_circuit_print_stats(const circuit_ctx_t *ctx) {
    if (ctx == NULL) {
        printf("Circuit is NULL\n");
        return;
    }

    qc_gate_counts_t counts = qc_circuit_gate_counts(ctx);

    printf("=== Circuit Statistics ===\n");
    printf("Total gates:   %zu\n", ctx->gate_count);
    printf("Depth (layers): %u\n", ctx->used_layer);
    printf("Width (qubits): %u\n", ctx->used_qubits + 1);
    printf("\nGate breakdown:\n");
    printf("  X:     %" PRIu64 "\n", counts.x_gates);
    printf("  Y:     %" PRIu64 "\n", counts.y_gates);
    printf("  Z:     %" PRIu64 "\n", counts.z_gates);
    printf("  H:     %" PRIu64 "\n", counts.h_gates);
    printf("  P:     %" PRIu64 "\n", counts.p_gates);
    printf("  T:     %" PRIu64 "\n", counts.t_gates);
    printf("  Tdg:   %" PRIu64 "\n", counts.tdg_gates);
    printf("  CX:    %" PRIu64 "\n", counts.cx_gates);
    printf("  CCX:   %" PRIu64 "\n", counts.ccx_gates);
    printf("  Other: %" PRIu64 "\n", counts.other_gates);
    printf("  T-count: %" PRIu64 "\n", counts.t_count);
    printf("==========================\n");
}
