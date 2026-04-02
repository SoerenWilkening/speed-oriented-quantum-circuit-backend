# circuit-c-backend

## Overview

C11 shared library (`libquantum.so`) implementing quantum circuit construction, qubit allocation, arithmetic (QFT and Toffoli), comparisons, bitwise ops, optimization, and OpenQASM 3.0 export.

## Build

```bash
cd circuit-c-backend
cmake -B build
cmake --build build
```

Produces `build/libquantum.so` (Linux) or `build/libquantum.dylib` (macOS). The sole public header is `include/quantum_circuit.h`.

## Public API

All functions take `circuit_ctx_t*` as their first argument. No global state; each context is independent and thread-safe when not shared.

### Types

```c
typedef struct circuit_ctx circuit_ctx_t;  // opaque context

typedef enum { QC_GATE_X=0, QC_GATE_Y, QC_GATE_Z, QC_GATE_H,
               QC_GATE_S, QC_GATE_SDG, QC_GATE_T, QC_GATE_TDG,
               QC_GATE_P, QC_GATE_RX, QC_GATE_RY, QC_GATE_RZ,
               QC_GATE_CX, QC_GATE_CY, QC_GATE_CZ, QC_GATE_CH,
               QC_GATE_CP, QC_GATE_CRY, QC_GATE_CCX, QC_GATE_MCX,
               QC_GATE_MCZ, QC_GATE_SWAP, QC_GATE_M,
               QC_GATE_COUNT } qc_gate_type_t;

typedef enum { QC_OK=0, QC_ERR_NULL=-1, QC_ERR_ALLOC=-2,
               QC_ERR_QUBIT=-3, QC_ERR_GATE=-4, QC_ERR_WIDTH=-5,
               QC_ERR_OVERFLOW=-6, QC_ERR_DOUBLE_FREE=-7,
               QC_ERR_INVALID_OP=-8, QC_ERR_RANGE=-9,
               QC_ERR_DIVISOR=-10, QC_ERR_IO=-11 } qc_error_t;

typedef enum { QC_ARITH_QFT=0, QC_ARITH_TOFFOLI=1 } qc_arith_mode_t;

typedef struct { uint64_t x_gates, y_gates, z_gates, h_gates, p_gates,
                 t_gates, tdg_gates, cx_gates, ccx_gates, other_gates,
                 t_count; } qc_gate_counts_t;

typedef struct { uint32_t peak_allocated, total_allocations,
                 total_deallocations, current_in_use; } qc_alloc_stats_t;

typedef struct { uint32_t gate_type, target; double angle;
                 uint32_t num_controls; uint32_t controls[8]; } qc_exported_gate_t;
```

### Lifecycle

```c
circuit_ctx_t *qc_circuit_create(uint32_t initial_qubits);  // caller owns, 0 = default 128
void           qc_circuit_destroy(circuit_ctx_t *ctx);       // NULL-safe
qc_error_t     qc_circuit_reset(circuit_ctx_t *ctx);         // clear without realloc
```

### Single-Qubit Gates

```c
qc_error_t qc_circuit_x(ctx, uint32_t target);
qc_error_t qc_circuit_y(ctx, uint32_t target);
qc_error_t qc_circuit_z(ctx, uint32_t target);
qc_error_t qc_circuit_h(ctx, uint32_t target);
qc_error_t qc_circuit_t_gate(ctx, uint32_t target);
qc_error_t qc_circuit_tdg(ctx, uint32_t target);
qc_error_t qc_circuit_p(ctx, uint32_t target, double angle);
qc_error_t qc_circuit_rx(ctx, uint32_t target, double angle);
qc_error_t qc_circuit_ry(ctx, uint32_t target, double angle);
qc_error_t qc_circuit_rz(ctx, uint32_t target, double angle);
```

### Two-Qubit Controlled Gates

```c
qc_error_t qc_circuit_cx(ctx, uint32_t control, uint32_t target);
qc_error_t qc_circuit_cy(ctx, uint32_t control, uint32_t target);
qc_error_t qc_circuit_cz(ctx, uint32_t control, uint32_t target);
qc_error_t qc_circuit_ch(ctx, uint32_t control, uint32_t target);
qc_error_t qc_circuit_cp(ctx, uint32_t control, uint32_t target, double angle);
qc_error_t qc_circuit_cry(ctx, uint32_t control, uint32_t target, double angle);
```

### Multi-Qubit Gates

```c
qc_error_t qc_circuit_ccx(ctx, uint32_t ctrl1, uint32_t ctrl2, uint32_t target);
qc_error_t qc_circuit_mcx(ctx, const uint32_t *controls, uint32_t n_controls, uint32_t target);
qc_error_t qc_circuit_mcz(ctx, const uint32_t *controls, uint32_t n_controls, uint32_t target);
```

### Generic Gate Insertion

```c
qc_error_t qc_circuit_add_gate(ctx, qc_gate_type_t type, uint32_t target,
                                const uint32_t *controls, uint32_t n_controls, double angle);
```

### Qubit Management

```c
qc_error_t qc_qubit_alloc(ctx, uint32_t *qubit);
qc_error_t qc_qubit_alloc_n(ctx, uint32_t count, uint32_t *start);
qc_error_t qc_qubit_free(ctx, uint32_t qubit);
qc_error_t qc_qubit_free_n(ctx, uint32_t start, uint32_t count);
bool       qc_qubit_is_allocated(const ctx, uint32_t qubit);
uint32_t   qc_circuit_num_qubits(const ctx);
qc_alloc_stats_t qc_circuit_alloc_stats(const ctx);
```

### Circuit Information

```c
uint64_t        qc_circuit_gate_count(const ctx);
uint32_t        qc_circuit_depth(const ctx);
uint32_t        qc_circuit_width(const ctx);
qc_gate_counts_t qc_circuit_gate_counts(const ctx);
qc_gate_counts_t qc_circuit_gate_counts_range(const ctx, uint32_t start_layer, uint32_t end_layer);
```

### Configuration

```c
qc_error_t     qc_circuit_set_arith_mode(ctx, qc_arith_mode_t mode);
qc_arith_mode_t qc_circuit_get_arith_mode(const ctx);
qc_error_t     qc_circuit_set_toffoli_decompose(ctx, bool enable);
qc_error_t     qc_circuit_set_qubit_saving(ctx, bool enable);
qc_error_t     qc_circuit_set_cla_override(ctx, int override_val);
qc_error_t     qc_circuit_set_simulate(ctx, bool enable);   // true=store gates, false=count only
qc_error_t     qc_circuit_set_layer_floor(ctx, uint32_t layer);
```

### QFT-Based Arithmetic

```c
qc_error_t qc_arith_qq_add(ctx, const uint32_t *a, const uint32_t *b, uint32_t width);       // a += b
qc_error_t qc_arith_cq_add(ctx, const uint32_t *target, uint32_t width, int64_t value);       // target += value
qc_error_t qc_arith_cqq_add(ctx, const uint32_t *a, const uint32_t *b, uint32_t width, uint32_t control);
qc_error_t qc_arith_ccq_add(ctx, const uint32_t *target, uint32_t width, int64_t value, uint32_t control);
qc_error_t qc_arith_qq_mul(ctx, const uint32_t *result, const uint32_t *a, const uint32_t *b, uint32_t width);
qc_error_t qc_arith_cq_mul(ctx, const uint32_t *result, const uint32_t *target, uint32_t width, int64_t value);
```

### Toffoli-Based Arithmetic

```c
qc_error_t qc_toffoli_qq_add(ctx, const uint32_t *a, const uint32_t *b, uint32_t width);      // CDKM ripple-carry
qc_error_t qc_toffoli_cq_add(ctx, const uint32_t *target, uint32_t width, int64_t value);
qc_error_t qc_toffoli_cqq_add(ctx, const uint32_t *a, const uint32_t *b, uint32_t width, uint32_t control);
qc_error_t qc_toffoli_ccq_add(ctx, const uint32_t *target, uint32_t width, int64_t value, uint32_t control);
qc_error_t qc_toffoli_qq_add_bk(ctx, const uint32_t *a, const uint32_t *b, uint32_t width);   // Brent-Kung CLA
qc_error_t qc_toffoli_qq_mul(ctx, const uint32_t *result, uint32_t result_bits,
                              const uint32_t *a, uint32_t a_bits, const uint32_t *b, uint32_t b_bits);
qc_error_t qc_toffoli_cq_mul(ctx, const uint32_t *result, uint32_t result_bits,
                              const uint32_t *target, uint32_t target_bits, int64_t value);
qc_error_t qc_toffoli_cmul_qq(ctx, const uint32_t *result, uint32_t result_bits,
                               const uint32_t *a, uint32_t a_bits,
                               const uint32_t *b, uint32_t b_bits, uint32_t ext_ctrl);
qc_error_t qc_toffoli_cmul_cq(ctx, const uint32_t *result, uint32_t result_bits,
                               const uint32_t *target, uint32_t target_bits,
                               int64_t value, uint32_t ext_ctrl);
```

### Toffoli Division and Modular Arithmetic

```c
qc_error_t qc_toffoli_divmod_cq(ctx, const uint32_t *dividend, uint32_t dividend_bits,
                                 int64_t divisor, const uint32_t *quotient, const uint32_t *remainder);
qc_error_t qc_toffoli_divmod_qq(ctx, const uint32_t *dividend, uint32_t dividend_bits,
                                 const uint32_t *divisor, uint32_t divisor_bits,
                                 const uint32_t *quotient, const uint32_t *remainder);
qc_error_t qc_toffoli_cdivmod_cq(ctx, const uint32_t *dividend, uint32_t dividend_bits,
                                  int64_t divisor, const uint32_t *quotient,
                                  const uint32_t *remainder, uint32_t ext_ctrl);
qc_error_t qc_toffoli_cdivmod_qq(ctx, const uint32_t *dividend, uint32_t dividend_bits,
                                  const uint32_t *divisor, uint32_t divisor_bits,
                                  const uint32_t *quotient, const uint32_t *remainder,
                                  uint32_t ext_ctrl);
qc_error_t qc_toffoli_mod_reduce(ctx, const uint32_t *value, uint32_t value_bits, int64_t modulus);
qc_error_t qc_toffoli_mod_add_cq(ctx, const uint32_t *value, uint32_t value_bits, int64_t addend, int64_t modulus);
qc_error_t qc_toffoli_mod_add_qq(ctx, const uint32_t *value, uint32_t value_bits,
                                  const uint32_t *other, uint32_t other_bits, int64_t modulus);
qc_error_t qc_toffoli_mod_mul_cq(ctx, const uint32_t *value, uint32_t value_bits,
                                  const uint32_t *result, uint32_t result_bits, int64_t multiplier, int64_t modulus);
```

### Comparison Operations

```c
qc_error_t qc_cmp_qq_equal(ctx, const uint32_t *a, const uint32_t *b, uint32_t width, uint32_t result);
qc_error_t qc_cmp_cq_equal(ctx, const uint32_t *a, uint32_t width, int64_t value, uint32_t result);
qc_error_t qc_cmp_qq_less(ctx, const uint32_t *a, const uint32_t *b, uint32_t width, uint32_t result);
qc_error_t qc_cmp_cq_less(ctx, const uint32_t *a, uint32_t width, int64_t value, uint32_t result);
qc_error_t qc_cmp_cq_greater(ctx, const uint32_t *a, uint32_t width, int64_t value, uint32_t result);
```

### Bitwise Operations

```c
qc_error_t qc_bitwise_not(ctx, const uint32_t *target, uint32_t width);
qc_error_t qc_bitwise_xor(ctx, const uint32_t *a, const uint32_t *b, uint32_t width);          // a ^= b
qc_error_t qc_bitwise_and(ctx, const uint32_t *result, const uint32_t *a, const uint32_t *b, uint32_t width);
qc_error_t qc_bitwise_or(ctx, const uint32_t *result, const uint32_t *a, const uint32_t *b, uint32_t width);
```

### Gate Extraction

```c
qc_error_t qc_circuit_extract_gates(const ctx, uint32_t start_layer, uint32_t end_layer,
                                     qc_exported_gate_t **out_gates, uint32_t *out_count);  // caller must free()
uint32_t   qc_circuit_used_layer(const ctx);
```

### Circuit Manipulation

```c
qc_error_t qc_circuit_reverse_range(ctx, uint32_t start_layer, uint32_t end_layer);  // for uncomputation
```

### Optimization

```c
circuit_ctx_t *qc_circuit_optimize(ctx);                              // all passes, returns new ctx
circuit_ctx_t *qc_circuit_optimize_pass(ctx, qc_opt_pass_t pass);    // single pass
bool           qc_circuit_can_optimize(const ctx);
// qc_opt_pass_t: QC_OPT_MERGE=0, QC_OPT_CANCEL_INVERSE=1
```

### Output / Serialization

```c
char      *qc_circuit_to_qasm(const ctx);                    // caller must free()
qc_error_t qc_circuit_to_qasm_file(const ctx, const char *path);
void       qc_circuit_print_stats(const ctx);
void       qc_circuit_visualize(const ctx);                  // ASCII diagram
```

### Version

```c
const char *qc_version_string(void);   // e.g. "1.0.0", static -- do not free
int         qc_version_number(void);   // major*10000 + minor*100 + patch
```

## Dependencies (what this package imports from siblings)

None. This is a Layer 0 package with zero sibling dependencies.

## Downstream Consumers (what other packages expect from this one)

- **quantum-core** (`backend_bridge.py`): Loads `libquantum.so` via ctypes/cffi. Uses lifecycle functions, all gate functions, qubit management, arithmetic (both QFT and Toffoli), comparisons, bitwise ops, configuration, gate extraction, circuit info, and QASM export. Mirrors `qc_gate_type_t` enum values as Python integer constants.
- **quantum-cython-types** (`_c_backend.pxd`): Cython declarations that `cimport` directly from `quantum_circuit.h`. Uses lifecycle, single-qubit gates (x, h, p), controlled gates (cx, ccx, mcx), qubit management, QFT arithmetic (qq_add, cq_add, cqq_add, ccq_add, qq_mul, cq_mul), Toffoli arithmetic (qq_add, cq_add, cqq_add, ccq_add, qq_add_bk, qq_mul, cq_mul), division (divmod_cq, divmod_qq), bitwise ops, comparisons (qq_equal, cq_equal, cq_greater), generic gate insertion, gate extraction, circuit info, configuration, reverse_range, and QASM export.

The critical contract: The `qc_gate_type_t` enum values must remain stable (X=0 through M=22) as both downstream consumers hardcode these values. The `circuit_ctx_t*` must remain opaque with all access through the documented function API.

## Testing

```bash
cd circuit-c-backend
cmake -B build && cmake --build build
# C tests (if ctest configured):
cd build && ctest --output-on-failure
# Python integration tests (require libquantum.so built):
pytest tests/ -v
```

## Cross-Package Issues

When you discover a problem that belongs to another package, file it with:
`bd create --package {target-package} --title "..." --body "..."`
