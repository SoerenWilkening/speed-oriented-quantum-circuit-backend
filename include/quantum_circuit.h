/**
 * @file quantum_circuit.h
 * @brief Public API for the quantum circuit C backend (libquantum).
 *
 * This is the sole public header for the circuit-c-backend package.
 * All functions take a circuit_ctx_t* as their first argument, eliminating
 * global state and enabling multiple independent circuit contexts.
 *
 * Usage:
 *   #include <quantum_circuit.h>
 *
 *   circuit_ctx_t *ctx = qc_circuit_create(64);
 *   qc_circuit_x(ctx, 0);
 *   qc_circuit_cx(ctx, 0, 1);
 *   char *qasm = qc_circuit_to_qasm(ctx);
 *   free(qasm);
 *   qc_circuit_destroy(ctx);
 *
 * Thread safety: Each circuit_ctx_t is independent. No global state is used.
 * Concurrent access to the SAME context requires external synchronization.
 *
 * @copyright 2026 Quantum Assembly Project
 */

#ifndef QUANTUM_CIRCUIT_PUBLIC_H
#define QUANTUM_CIRCUIT_PUBLIC_H

/* ====================================================================== */
/* Version                                                                 */
/* ====================================================================== */

#define QC_VERSION_MAJOR 1
#define QC_VERSION_MINOR 0
#define QC_VERSION_PATCH 0

/**
 * @brief Encode version as a single integer: (major * 10000 + minor * 100 + patch).
 */
#define QC_VERSION_NUMBER \
    (QC_VERSION_MAJOR * 10000 + QC_VERSION_MINOR * 100 + QC_VERSION_PATCH)

/**
 * @brief Version string literal, e.g. "1.0.0".
 */
#define QC_VERSION_STRING "1.0.0"

/* ====================================================================== */
/* Platform / Visibility                                                   */
/* ====================================================================== */

#ifdef __cplusplus
#  define QC_EXTERN_C_BEGIN extern "C" {
#  define QC_EXTERN_C_END   }
#else
#  define QC_EXTERN_C_BEGIN
#  define QC_EXTERN_C_END
#endif

/* Shared-library export/import macros */
#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef QC_BUILDING_DLL
#    define QC_API __declspec(dllexport)
#  elif defined(QC_USING_DLL)
#    define QC_API __declspec(dllimport)
#  else
#    define QC_API
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define QC_API __attribute__((visibility("default")))
#else
#  define QC_API
#endif

/* ====================================================================== */
/* Standard includes                                                       */
/* ====================================================================== */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

QC_EXTERN_C_BEGIN

/* ====================================================================== */
/* Opaque context                                                          */
/* ====================================================================== */

/**
 * @brief Opaque circuit context.
 *
 * All public functions accept a pointer to this type. The internal layout
 * is private to the implementation and must not be accessed directly.
 * Create with qc_circuit_create(), destroy with qc_circuit_destroy().
 */
typedef struct circuit_ctx circuit_ctx_t;

/* ====================================================================== */
/* Gate types                                                              */
/* ====================================================================== */

/**
 * @brief Standard gate types supported by the circuit backend.
 *
 * These correspond to the fundamental gate set used for circuit construction.
 * Parameterized gates (P, Rx, Ry, Rz) require an angle argument.
 */
typedef enum {
    QC_GATE_X       = 0,   /**< Pauli-X (NOT) gate */
    QC_GATE_Y       = 1,   /**< Pauli-Y gate */
    QC_GATE_Z       = 2,   /**< Pauli-Z gate */
    QC_GATE_H       = 3,   /**< Hadamard gate */
    QC_GATE_S       = 4,   /**< S gate (phase pi/2) */
    QC_GATE_SDG     = 5,   /**< S-dagger gate (phase -pi/2) */
    QC_GATE_T       = 6,   /**< T gate (phase pi/4) */
    QC_GATE_TDG     = 7,   /**< T-dagger gate (phase -pi/4) */
    QC_GATE_P       = 8,   /**< Phase gate (arbitrary angle) */
    QC_GATE_RX      = 9,   /**< Rotation around X axis */
    QC_GATE_RY      = 10,  /**< Rotation around Y axis */
    QC_GATE_RZ      = 11,  /**< Rotation around Z axis */
    QC_GATE_CX      = 12,  /**< Controlled-X (CNOT) */
    QC_GATE_CY      = 13,  /**< Controlled-Y */
    QC_GATE_CZ      = 14,  /**< Controlled-Z */
    QC_GATE_CH      = 15,  /**< Controlled-Hadamard */
    QC_GATE_CP      = 16,  /**< Controlled-Phase */
    QC_GATE_CRY     = 17,  /**< Controlled-Ry */
    QC_GATE_CCX     = 18,  /**< Toffoli (double-controlled X) */
    QC_GATE_MCX     = 19,  /**< Multi-controlled X */
    QC_GATE_MCZ     = 20,  /**< Multi-controlled Z */
    QC_GATE_SWAP    = 21,  /**< SWAP gate */
    QC_GATE_M       = 22,  /**< Measurement */
    QC_GATE_COUNT          /**< Sentinel: number of gate types */
} qc_gate_type_t;

/* ====================================================================== */
/* Error codes                                                             */
/* ====================================================================== */

/**
 * @brief Error codes returned by circuit operations.
 *
 * Functions that can fail return a qc_error_t. QC_OK (0) indicates success.
 * All error values are negative.
 */
typedef enum {
    QC_OK               =  0,  /**< Success */
    QC_ERR_NULL         = -1,  /**< NULL pointer argument */
    QC_ERR_ALLOC        = -2,  /**< Memory allocation failure */
    QC_ERR_QUBIT        = -3,  /**< Invalid qubit index */
    QC_ERR_GATE         = -4,  /**< Invalid gate type or parameters */
    QC_ERR_WIDTH        = -5,  /**< Invalid bit width (must be 1-64) */
    QC_ERR_OVERFLOW     = -6,  /**< Capacity overflow (too many qubits/gates) */
    QC_ERR_DOUBLE_FREE  = -7,  /**< Qubit already freed */
    QC_ERR_INVALID_OP   = -8,  /**< Operation not valid in current state */
    QC_ERR_RANGE        = -9,  /**< Index out of range */
    QC_ERR_DIVISOR      = -10, /**< Invalid divisor (zero) */
    QC_ERR_IO           = -11  /**< I/O error (file write, etc.) */
} qc_error_t;

/* ====================================================================== */
/* Arithmetic mode                                                         */
/* ====================================================================== */

/**
 * @brief Arithmetic implementation mode.
 *
 * Controls whether arithmetic operations use QFT-based rotations
 * (fewer qubits, not fault-tolerant) or Toffoli-based gates
 * (more qubits, fault-tolerant with Clifford+T decomposition).
 */
typedef enum {
    QC_ARITH_QFT     = 0, /**< QFT-based (Draper adder) */
    QC_ARITH_TOFFOLI = 1  /**< Toffoli-based (CDKM / CLA) */
} qc_arith_mode_t;

/* ====================================================================== */
/* Gate count statistics                                                   */
/* ====================================================================== */

/**
 * @brief Breakdown of gate types in a circuit.
 */
typedef struct {
    uint64_t x_gates;     /**< Pauli-X gates */
    uint64_t y_gates;     /**< Pauli-Y gates */
    uint64_t z_gates;     /**< Pauli-Z gates */
    uint64_t h_gates;     /**< Hadamard gates */
    uint64_t p_gates;     /**< Phase gates (P, Rx, Ry, Rz) */
    uint64_t t_gates;     /**< T gates */
    uint64_t tdg_gates;   /**< T-dagger gates */
    uint64_t cx_gates;    /**< CNOT gates */
    uint64_t ccx_gates;   /**< Toffoli gates */
    uint64_t other_gates; /**< All other gate types */
    uint64_t t_count;     /**< Total T-cost estimate */
} qc_gate_counts_t;

/* ====================================================================== */
/* Allocator statistics                                                    */
/* ====================================================================== */

/**
 * @brief Qubit allocation statistics for debugging and analysis.
 */
typedef struct {
    uint32_t peak_allocated;      /**< Highest water mark */
    uint32_t total_allocations;   /**< Total alloc calls */
    uint32_t total_deallocations; /**< Total free calls */
    uint32_t current_in_use;      /**< Currently allocated */
} qc_alloc_stats_t;

/* ====================================================================== */
/* Circuit lifecycle                                                       */
/* ====================================================================== */

/**
 * @brief Create a new circuit context.
 *
 * Allocates and initializes a circuit context with empty gate sequences
 * and a qubit allocator pre-sized for @p initial_qubits.
 *
 * @param initial_qubits  Hint for initial qubit capacity (0 = default 128).
 * @return New context, or NULL on allocation failure.
 *
 * OWNERSHIP: Caller owns returned pointer; must call qc_circuit_destroy().
 */
QC_API circuit_ctx_t *qc_circuit_create(uint32_t initial_qubits);

/**
 * @brief Destroy a circuit context and free all resources.
 *
 * @param ctx  Context to destroy (NULL is a safe no-op).
 */
QC_API void qc_circuit_destroy(circuit_ctx_t *ctx);

/**
 * @brief Reset a circuit context to empty state without reallocating.
 *
 * Clears all gates and resets qubit allocator, but keeps allocated memory
 * for reuse. More efficient than destroy + create for repeated use.
 *
 * @param ctx  Context to reset.
 * @return QC_OK on success, QC_ERR_NULL if ctx is NULL.
 */
QC_API qc_error_t qc_circuit_reset(circuit_ctx_t *ctx);

/* ====================================================================== */
/* Single-qubit gates                                                      */
/* ====================================================================== */

/** @brief Apply Pauli-X (NOT) gate. */
QC_API qc_error_t qc_circuit_x(circuit_ctx_t *ctx, uint32_t target);

/** @brief Apply Pauli-Y gate. */
QC_API qc_error_t qc_circuit_y(circuit_ctx_t *ctx, uint32_t target);

/** @brief Apply Pauli-Z gate. */
QC_API qc_error_t qc_circuit_z(circuit_ctx_t *ctx, uint32_t target);

/** @brief Apply Hadamard gate. */
QC_API qc_error_t qc_circuit_h(circuit_ctx_t *ctx, uint32_t target);

/** @brief Apply T gate (pi/4 phase). */
QC_API qc_error_t qc_circuit_t_gate(circuit_ctx_t *ctx, uint32_t target);

/** @brief Apply T-dagger gate (-pi/4 phase). */
QC_API qc_error_t qc_circuit_tdg(circuit_ctx_t *ctx, uint32_t target);

/** @brief Apply phase gate with arbitrary angle. */
QC_API qc_error_t qc_circuit_p(circuit_ctx_t *ctx, uint32_t target, double angle);

/** @brief Apply Rx rotation gate. */
QC_API qc_error_t qc_circuit_rx(circuit_ctx_t *ctx, uint32_t target, double angle);

/** @brief Apply Ry rotation gate. */
QC_API qc_error_t qc_circuit_ry(circuit_ctx_t *ctx, uint32_t target, double angle);

/** @brief Apply Rz rotation gate. */
QC_API qc_error_t qc_circuit_rz(circuit_ctx_t *ctx, uint32_t target, double angle);

/* ====================================================================== */
/* Two-qubit controlled gates                                              */
/* ====================================================================== */

/** @brief Apply CNOT (controlled-X) gate. */
QC_API qc_error_t qc_circuit_cx(circuit_ctx_t *ctx, uint32_t control, uint32_t target);

/** @brief Apply controlled-Y gate. */
QC_API qc_error_t qc_circuit_cy(circuit_ctx_t *ctx, uint32_t control, uint32_t target);

/** @brief Apply controlled-Z gate. */
QC_API qc_error_t qc_circuit_cz(circuit_ctx_t *ctx, uint32_t control, uint32_t target);

/** @brief Apply controlled-Hadamard gate. */
QC_API qc_error_t qc_circuit_ch(circuit_ctx_t *ctx, uint32_t control, uint32_t target);

/** @brief Apply controlled-phase gate. */
QC_API qc_error_t qc_circuit_cp(circuit_ctx_t *ctx, uint32_t control, uint32_t target,
                                 double angle);

/** @brief Apply controlled-Ry gate. */
QC_API qc_error_t qc_circuit_cry(circuit_ctx_t *ctx, uint32_t control, uint32_t target,
                                  double angle);

/* ====================================================================== */
/* Multi-qubit gates                                                       */
/* ====================================================================== */

/** @brief Apply Toffoli (CCX) gate. */
QC_API qc_error_t qc_circuit_ccx(circuit_ctx_t *ctx, uint32_t ctrl1, uint32_t ctrl2,
                                  uint32_t target);

/**
 * @brief Apply multi-controlled X gate.
 *
 * @param ctx        Circuit context.
 * @param controls   Array of control qubit indices.
 * @param n_controls Number of control qubits (>= 1).
 * @param target     Target qubit index.
 */
QC_API qc_error_t qc_circuit_mcx(circuit_ctx_t *ctx, const uint32_t *controls,
                                  uint32_t n_controls, uint32_t target);

/**
 * @brief Apply multi-controlled Z gate.
 *
 * @param ctx        Circuit context.
 * @param controls   Array of control qubit indices.
 * @param n_controls Number of control qubits (>= 1).
 * @param target     Target qubit index.
 */
QC_API qc_error_t qc_circuit_mcz(circuit_ctx_t *ctx, const uint32_t *controls,
                                  uint32_t n_controls, uint32_t target);

/* ====================================================================== */
/* Generic gate insertion                                                  */
/* ====================================================================== */

/**
 * @brief Add a gate by type enum.
 *
 * Generic entry point for gate insertion. For parameterized gates (P, Rx,
 * Ry, Rz, CP, CRY), pass the angle; for others, angle is ignored.
 *
 * @param ctx        Circuit context.
 * @param type       Gate type from qc_gate_type_t.
 * @param target     Target qubit index.
 * @param controls   Array of control qubit indices (NULL if none).
 * @param n_controls Number of control qubits.
 * @param angle      Rotation angle (used only for parameterized gates).
 */
QC_API qc_error_t qc_circuit_add_gate(circuit_ctx_t *ctx, qc_gate_type_t type,
                                       uint32_t target, const uint32_t *controls,
                                       uint32_t n_controls, double angle);

/* ====================================================================== */
/* Qubit management                                                        */
/* ====================================================================== */

/**
 * @brief Allocate a single qubit.
 *
 * Returns a fresh qubit index. Freed qubits may be reused.
 *
 * @param ctx       Circuit context.
 * @param[out] qubit  Receives the allocated qubit index.
 * @return QC_OK on success, QC_ERR_NULL or QC_ERR_OVERFLOW on failure.
 */
QC_API qc_error_t qc_qubit_alloc(circuit_ctx_t *ctx, uint32_t *qubit);

/**
 * @brief Allocate a contiguous block of qubits.
 *
 * @param ctx         Circuit context.
 * @param count       Number of qubits to allocate.
 * @param[out] start  Receives the first qubit index of the block.
 * @return QC_OK on success, error code on failure.
 */
QC_API qc_error_t qc_qubit_alloc_n(circuit_ctx_t *ctx, uint32_t count, uint32_t *start);

/**
 * @brief Free a previously allocated qubit for reuse.
 *
 * @param ctx    Circuit context.
 * @param qubit  Qubit index to free.
 * @return QC_OK on success, QC_ERR_DOUBLE_FREE if already freed.
 */
QC_API qc_error_t qc_qubit_free(circuit_ctx_t *ctx, uint32_t qubit);

/**
 * @brief Free a contiguous block of qubits.
 *
 * @param ctx    Circuit context.
 * @param start  First qubit index of the block.
 * @param count  Number of qubits to free.
 * @return QC_OK on success, error code on failure.
 */
QC_API qc_error_t qc_qubit_free_n(circuit_ctx_t *ctx, uint32_t start, uint32_t count);

/**
 * @brief Check if a qubit is currently allocated.
 *
 * @param ctx    Circuit context.
 * @param qubit  Qubit index to check.
 * @return true if allocated, false if freed or never allocated.
 */
QC_API bool qc_qubit_is_allocated(const circuit_ctx_t *ctx, uint32_t qubit);

/**
 * @brief Get total number of qubits currently in use.
 */
QC_API uint32_t qc_circuit_num_qubits(const circuit_ctx_t *ctx);

/**
 * @brief Get qubit allocation statistics.
 */
QC_API qc_alloc_stats_t qc_circuit_alloc_stats(const circuit_ctx_t *ctx);

/* ====================================================================== */
/* Circuit information                                                     */
/* ====================================================================== */

/**
 * @brief Get total gate count in the circuit.
 */
QC_API uint64_t qc_circuit_gate_count(const circuit_ctx_t *ctx);

/**
 * @brief Get circuit depth (number of time-step layers).
 */
QC_API uint32_t qc_circuit_depth(const circuit_ctx_t *ctx);

/**
 * @brief Get circuit width (number of distinct qubits used).
 */
QC_API uint32_t qc_circuit_width(const circuit_ctx_t *ctx);

/**
 * @brief Get breakdown of gate types.
 */
QC_API qc_gate_counts_t qc_circuit_gate_counts(const circuit_ctx_t *ctx);

/**
 * @brief Get gate counts for a layer range [start, end).
 *
 * @param ctx         Circuit context.
 * @param start_layer First layer (inclusive).
 * @param end_layer   Last layer (exclusive).
 */
QC_API qc_gate_counts_t qc_circuit_gate_counts_range(const circuit_ctx_t *ctx,
                                                      uint32_t start_layer,
                                                      uint32_t end_layer);

/* ====================================================================== */
/* Configuration                                                           */
/* ====================================================================== */

/**
 * @brief Set arithmetic mode (QFT or Toffoli).
 */
QC_API qc_error_t qc_circuit_set_arith_mode(circuit_ctx_t *ctx, qc_arith_mode_t mode);

/**
 * @brief Get current arithmetic mode.
 */
QC_API qc_arith_mode_t qc_circuit_get_arith_mode(const circuit_ctx_t *ctx);

/**
 * @brief Enable/disable Toffoli decomposition to Clifford+T.
 *
 * When enabled, CCX gates are decomposed into H, T, Tdg, CX gates.
 */
QC_API qc_error_t qc_circuit_set_toffoli_decompose(circuit_ctx_t *ctx, bool enable);

/**
 * @brief Enable/disable qubit-saving mode (Brent-Kung vs Kogge-Stone CLA).
 */
QC_API qc_error_t qc_circuit_set_qubit_saving(circuit_ctx_t *ctx, bool enable);

/**
 * @brief Set CLA override: 0 = auto, 1 = force ripple-carry.
 */
QC_API qc_error_t qc_circuit_set_cla_override(circuit_ctx_t *ctx, int override_val);

/**
 * @brief Enable/disable simulation mode (gate storage vs count-only).
 *
 * @param ctx      Circuit context.
 * @param enable   true = store gates, false = count only.
 */
QC_API qc_error_t qc_circuit_set_simulate(circuit_ctx_t *ctx, bool enable);

/**
 * @brief Set layer floor for gate placement.
 *
 * Gates will not be placed before this layer index.
 */
QC_API qc_error_t qc_circuit_set_layer_floor(circuit_ctx_t *ctx, uint32_t layer);

/* ====================================================================== */
/* QFT-based arithmetic                                                    */
/* ====================================================================== */

/**
 * @brief Quantum-quantum addition: a += b (QFT Draper adder).
 *
 * @param ctx     Circuit context.
 * @param a       Target register qubit indices (modified in place).
 * @param b       Source register qubit indices (unchanged).
 * @param width   Bit width of registers (1-64).
 */
QC_API qc_error_t qc_arith_qq_add(circuit_ctx_t *ctx, const uint32_t *a,
                                    const uint32_t *b, uint32_t width);

/**
 * @brief Classical-quantum addition: target += value (QFT Draper adder).
 *
 * @param ctx     Circuit context.
 * @param target  Target register qubit indices (modified in place).
 * @param width   Bit width (1-64).
 * @param value   Classical value to add.
 */
QC_API qc_error_t qc_arith_cq_add(circuit_ctx_t *ctx, const uint32_t *target,
                                    uint32_t width, int64_t value);

/**
 * @brief Controlled quantum-quantum addition.
 */
QC_API qc_error_t qc_arith_cqq_add(circuit_ctx_t *ctx, const uint32_t *a,
                                     const uint32_t *b, uint32_t width,
                                     uint32_t control);

/**
 * @brief Controlled classical-quantum addition.
 */
QC_API qc_error_t qc_arith_ccq_add(circuit_ctx_t *ctx, const uint32_t *target,
                                     uint32_t width, int64_t value, uint32_t control);

/**
 * @brief Quantum-quantum multiplication: result = a * b.
 */
QC_API qc_error_t qc_arith_qq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                    const uint32_t *a, const uint32_t *b,
                                    uint32_t width);

/**
 * @brief Classical-quantum multiplication: result = target * value.
 */
QC_API qc_error_t qc_arith_cq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                    const uint32_t *target, uint32_t width,
                                    int64_t value);

/* ====================================================================== */
/* Toffoli-based arithmetic                                                */
/* ====================================================================== */

/**
 * @brief Toffoli QQ addition: a += b (CDKM ripple-carry).
 */
QC_API qc_error_t qc_toffoli_qq_add(circuit_ctx_t *ctx, const uint32_t *a,
                                      const uint32_t *b, uint32_t width);

/**
 * @brief Toffoli CQ addition: target += value.
 */
QC_API qc_error_t qc_toffoli_cq_add(circuit_ctx_t *ctx, const uint32_t *target,
                                      uint32_t width, int64_t value);

/**
 * @brief Toffoli controlled QQ addition.
 */
QC_API qc_error_t qc_toffoli_cqq_add(circuit_ctx_t *ctx, const uint32_t *a,
                                       const uint32_t *b, uint32_t width,
                                       uint32_t control);

/**
 * @brief Toffoli controlled CQ addition.
 */
QC_API qc_error_t qc_toffoli_ccq_add(circuit_ctx_t *ctx, const uint32_t *target,
                                       uint32_t width, int64_t value,
                                       uint32_t control);

/**
 * @brief Brent-Kung CLA QQ addition: b += a (O(log n) depth).
 */
QC_API qc_error_t qc_toffoli_qq_add_bk(circuit_ctx_t *ctx, const uint32_t *a,
                                         const uint32_t *b, uint32_t width);

/**
 * @brief Toffoli QQ multiplication: result = a * b.
 */
QC_API qc_error_t qc_toffoli_qq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                      uint32_t result_bits, const uint32_t *a,
                                      uint32_t a_bits, const uint32_t *b,
                                      uint32_t b_bits);

/**
 * @brief Toffoli CQ multiplication: result = target * value.
 */
QC_API qc_error_t qc_toffoli_cq_mul(circuit_ctx_t *ctx, const uint32_t *result,
                                      uint32_t result_bits, const uint32_t *target,
                                      uint32_t target_bits, int64_t value);

/* ====================================================================== */
/* Toffoli division and modular arithmetic                                 */
/* ====================================================================== */

/**
 * @brief Toffoli divmod with classical divisor.
 */
QC_API qc_error_t qc_toffoli_divmod_cq(circuit_ctx_t *ctx, const uint32_t *dividend,
                                         uint32_t dividend_bits, int64_t divisor,
                                         const uint32_t *quotient,
                                         const uint32_t *remainder);

/**
 * @brief Toffoli divmod with quantum divisor.
 */
QC_API qc_error_t qc_toffoli_divmod_qq(circuit_ctx_t *ctx, const uint32_t *dividend,
                                         uint32_t dividend_bits,
                                         const uint32_t *divisor,
                                         uint32_t divisor_bits,
                                         const uint32_t *quotient,
                                         const uint32_t *remainder);

/**
 * @brief Modular reduction: value = value mod N.
 */
QC_API qc_error_t qc_toffoli_mod_reduce(circuit_ctx_t *ctx, const uint32_t *value,
                                          uint32_t value_bits, int64_t modulus);

/**
 * @brief Modular CQ addition: value = (value + addend) mod N.
 */
QC_API qc_error_t qc_toffoli_mod_add_cq(circuit_ctx_t *ctx, const uint32_t *value,
                                          uint32_t value_bits, int64_t addend,
                                          int64_t modulus);

/**
 * @brief Modular QQ addition: value = (value + other) mod N.
 */
QC_API qc_error_t qc_toffoli_mod_add_qq(circuit_ctx_t *ctx, const uint32_t *value,
                                          uint32_t value_bits, const uint32_t *other,
                                          uint32_t other_bits, int64_t modulus);

/**
 * @brief Modular CQ multiplication: result = value * multiplier mod N.
 */
QC_API qc_error_t qc_toffoli_mod_mul_cq(circuit_ctx_t *ctx, const uint32_t *value,
                                          uint32_t value_bits, const uint32_t *result,
                                          uint32_t result_bits, int64_t multiplier,
                                          int64_t modulus);

/* ====================================================================== */
/* Comparison operations                                                   */
/* ====================================================================== */

/**
 * @brief Quantum-quantum equality: result = (A == B).
 *
 * @param ctx     Circuit context.
 * @param a       First operand qubit indices.
 * @param b       Second operand qubit indices.
 * @param width   Bit width (1-64).
 * @param result  Result qubit index (set to |1> if equal).
 */
QC_API qc_error_t qc_cmp_qq_equal(circuit_ctx_t *ctx, const uint32_t *a,
                                    const uint32_t *b, uint32_t width,
                                    uint32_t result);

/**
 * @brief Classical-quantum equality: result = (A == value).
 */
QC_API qc_error_t qc_cmp_cq_equal(circuit_ctx_t *ctx, const uint32_t *a,
                                    uint32_t width, int64_t value, uint32_t result);

/**
 * @brief Quantum-quantum less-than: result = (A < B).
 */
QC_API qc_error_t qc_cmp_qq_less(circuit_ctx_t *ctx, const uint32_t *a,
                                   const uint32_t *b, uint32_t width,
                                   uint32_t result);

/**
 * @brief Classical-quantum less-than: result = (A < value).
 */
QC_API qc_error_t qc_cmp_cq_less(circuit_ctx_t *ctx, const uint32_t *a,
                                   uint32_t width, int64_t value, uint32_t result);

/**
 * @brief Classical-quantum greater-than: result = (A > value).
 */
QC_API qc_error_t qc_cmp_cq_greater(circuit_ctx_t *ctx, const uint32_t *a,
                                      uint32_t width, int64_t value, uint32_t result);

/* ====================================================================== */
/* Bitwise operations                                                      */
/* ====================================================================== */

/**
 * @brief Bitwise NOT on a quantum register.
 */
QC_API qc_error_t qc_bitwise_not(circuit_ctx_t *ctx, const uint32_t *target,
                                   uint32_t width);

/**
 * @brief Bitwise XOR: a ^= b.
 */
QC_API qc_error_t qc_bitwise_xor(circuit_ctx_t *ctx, const uint32_t *a,
                                   const uint32_t *b, uint32_t width);

/**
 * @brief Bitwise AND: result = a & b.
 */
QC_API qc_error_t qc_bitwise_and(circuit_ctx_t *ctx, const uint32_t *result,
                                   const uint32_t *a, const uint32_t *b,
                                   uint32_t width);

/**
 * @brief Bitwise OR: result = a | b.
 */
QC_API qc_error_t qc_bitwise_or(circuit_ctx_t *ctx, const uint32_t *result,
                                  const uint32_t *a, const uint32_t *b,
                                  uint32_t width);

/* ====================================================================== */
/* Gate extraction / injection                                             */
/* ====================================================================== */

/**
 * @brief Exported gate representation for Python interop.
 */
typedef struct {
    uint32_t  gate_type;     /**< Gate type (qc_igate_t enum value) */
    uint32_t  target;        /**< Target qubit index */
    double    angle;         /**< Rotation angle (0.0 for non-rotation) */
    uint32_t  num_controls;  /**< Number of control qubits */
    uint32_t  controls[8];   /**< Control qubit indices (up to 8 inline) */
} qc_exported_gate_t;

/**
 * @brief Extract gates from layers [start_layer, end_layer).
 *
 * Caller must free the returned array with free().
 *
 * @param ctx          Circuit context.
 * @param start_layer  First layer (inclusive).
 * @param end_layer    Last layer (exclusive).
 * @param[out] out_gates   Pointer to receive allocated gate array.
 * @param[out] out_count   Pointer to receive number of gates.
 * @return QC_OK on success, QC_ERR_NULL if ctx is NULL.
 */
QC_API qc_error_t qc_circuit_extract_gates(const circuit_ctx_t *ctx,
                                             uint32_t start_layer,
                                             uint32_t end_layer,
                                             qc_exported_gate_t **out_gates,
                                             uint32_t *out_count);

/**
 * @brief Get the current used layer count (for layer tracking).
 */
QC_API uint32_t qc_circuit_used_layer(const circuit_ctx_t *ctx);

/* ====================================================================== */
/* Circuit manipulation                                                    */
/* ====================================================================== */

/**
 * @brief Reverse gates in a layer range [start, end).
 *
 * Used for uncomputation (inverse of a subcircuit).
 */
QC_API qc_error_t qc_circuit_reverse_range(circuit_ctx_t *ctx, uint32_t start_layer,
                                             uint32_t end_layer);

/* ====================================================================== */
/* Optimization                                                            */
/* ====================================================================== */

/**
 * @brief Available optimization passes.
 */
typedef enum {
    QC_OPT_MERGE          = 0, /**< Merge consecutive same-type gates */
    QC_OPT_CANCEL_INVERSE = 1  /**< Cancel inverse gate pairs (X-X, H-H) */
} qc_opt_pass_t;

/**
 * @brief Run all optimization passes, producing a new optimized circuit.
 *
 * @param ctx  Source circuit (not modified).
 * @return New optimized circuit (caller owns), or NULL on failure.
 */
QC_API circuit_ctx_t *qc_circuit_optimize(circuit_ctx_t *ctx);

/**
 * @brief Run a specific optimization pass.
 *
 * @param ctx   Source circuit (not modified).
 * @param pass  Which optimization pass to run.
 * @return New optimized circuit (caller owns), or NULL on failure.
 */
QC_API circuit_ctx_t *qc_circuit_optimize_pass(circuit_ctx_t *ctx, qc_opt_pass_t pass);

/**
 * @brief Check if optimization would have any effect.
 *
 * @param ctx  Circuit to check.
 * @return true if optimization would change the circuit.
 */
QC_API bool qc_circuit_can_optimize(const circuit_ctx_t *ctx);

/* ====================================================================== */
/* Output / Serialization                                                  */
/* ====================================================================== */

/**
 * @brief Export circuit to OpenQASM 3.0 string.
 *
 * @param ctx  Circuit context.
 * @return Heap-allocated QASM string (caller must free()), or NULL on error.
 */
QC_API char *qc_circuit_to_qasm(const circuit_ctx_t *ctx);

/**
 * @brief Export circuit to OpenQASM 3.0 file.
 *
 * @param ctx   Circuit context.
 * @param path  File path to write.
 * @return QC_OK on success, QC_ERR_IO on write failure.
 */
QC_API qc_error_t qc_circuit_to_qasm_file(const circuit_ctx_t *ctx, const char *path);

/**
 * @brief Print circuit statistics to stdout.
 *
 * Prints gate count, depth, width, and gate type breakdown.
 */
QC_API void qc_circuit_print_stats(const circuit_ctx_t *ctx);

/**
 * @brief Print ASCII circuit diagram to stdout.
 */
QC_API void qc_circuit_visualize(const circuit_ctx_t *ctx);

/* ====================================================================== */
/* Version query (runtime)                                                 */
/* ====================================================================== */

/**
 * @brief Get library version string at runtime.
 *
 * @return Static string, e.g. "1.0.0". Do not free.
 */
QC_API const char *qc_version_string(void);

/**
 * @brief Get library version as integer (major * 10000 + minor * 100 + patch).
 */
QC_API int qc_version_number(void);

QC_EXTERN_C_END

#endif /* QUANTUM_CIRCUIT_PUBLIC_H */
