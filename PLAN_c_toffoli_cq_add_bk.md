# PLAN: Controlled Toffoli CQ BK CLA Add Sequence Builder

**PRD**: `circuit-c-backend/PRD_c_toffoli_cq_add_bk.md`
**Estimated size**: ~45 lines of new code across 5 files

## Module 1: C Sequence Builder (circuit-c-backend)

### 1a. Add `qc_toffoli_ccq_add_bk_seq` to `toffoli_cla_seq.c`

Insert after `qc_toffoli_cqq_add_bk_seq` (line 234), following the exact
same capture pattern used by `qc_toffoli_cq_add_bk_seq` but with an extra
control qubit:

```c
/* qc_toffoli_ccq_add_bk_seq -- controlled CQ BK CLA add sequence          */

qc_sequence_t *qc_toffoli_ccq_add_bk_seq(int bits, int64_t value) {
    if (bits < 1 || bits > 64)
        return NULL;

    uint32_t n = (uint32_t)bits;
    uint32_t total = 4 * n + 64;

    circuit_ctx_t *ctx = cmp_create_capture_ctx(total);
    if (!ctx)
        return NULL;
    qc_circuit_set_arith_mode(ctx, QC_ARITH_TOFFOLI);

    /* Pre-allocate: n target qubits + 1 control qubit */
    uint32_t start;
    if (qc_qubit_alloc_n(ctx, n + 1, &start) != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    uint32_t target[64];
    for (uint32_t i = 0; i < n; i++)
        target[i] = i;
    uint32_t control = n;  /* control qubit is last pre-allocated */

    qc_error_t err = qc_toffoli_ccq_add(ctx, target, n, value, control);
    if (err != QC_OK) {
        qc_circuit_destroy(ctx);
        return NULL;
    }

    qc_sequence_t *seq = cmp_capture_circuit_to_sequence(ctx);
    if (seq)
        seq->total_qubits = ctx->allocator->next_qubit;
    qc_circuit_destroy(ctx);
    return seq;
}
```

**Qubit layout**: `[0..n-1]` = target register, `[n]` = control qubit.
This matches the dispatch convention `takes_value=True` where the caller
passes `target_qubits + [ctrl_idx]`.

### 1b. Add header declaration to `include/quantum_circuit.h`

After line 1001 (`qc_toffoli_cqq_add_bk_seq`), add:

```c
QC_API qc_sequence_t *qc_toffoli_ccq_add_bk_seq(int bits, int64_t value);
```

## Module 2: Cython Layer (quantum-cython-types)

### 2a. Add declaration to `_c_backend.pxd`

After line 305 (`qc_toffoli_cqq_add_bk_seq`), add:

```cython
    qc_sequence_t *qc_toffoli_ccq_add_bk_seq(int bits, int64_t value)
```

### 2b. Add Python wrapper to `_c_backend.pyx`

After `toffoli_cqq_add_bk_seq` (line 728), add:

```cython
def toffoli_ccq_add_bk_seq(int bits, int64_t value):
    cdef qc_sequence_t *seq = qc_toffoli_ccq_add_bk_seq(bits, value)
    if seq is NULL:
        return 0
    return <uintptr_t>seq
```

## Module 3: Dispatch Table (quantum-core)

### 3a. Add entry to `_SEQ_DISPATCH` in `sequences.py`

After the existing `"c_toffoli_qq_add_bk"` entry (line 85), add:

```python
"c_toffoli_cq_add_bk":  ("toffoli_ccq_add_bk_seq",   True),
```

## Execution Order

1. Module 1a + 1b (C layer) -- can be done together
2. Module 2a + 2b (Cython layer) -- depends on Module 1
3. Module 3a (dispatch) -- independent of Cython compilation
4. Rebuild: `cmake --build build && pip install -e quantum-cython-types`

## Verification

After implementation, the following should work without warnings:

```python
from quantum_core import get_sequence, free_sequence
ptr = get_sequence("c_toffoli_cq_add_bk", 4, 3)
assert ptr != 0
free_sequence(ptr)
```

And in a full integration test:
```python
@compile
def f(a: qint[4]):
    with qbool(True):
        a += 3
```
should produce a compiled function that includes the controlled toffoli CQ
addition without any `get_sequence: unknown op_type` warnings.
