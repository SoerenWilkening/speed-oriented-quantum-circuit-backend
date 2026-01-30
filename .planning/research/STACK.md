# Technology Stack

**Project:** Quantum Assembly - OpenQASM Export & Qiskit Verification
**Researched:** 2026-01-30
**Confidence:** HIGH

## Executive Summary

This stack addition enables production-quality OpenQASM 3.0 export and Qiskit-based circuit verification for the existing Quantum Assembly framework. The additions are minimal and surgical: two Python packages for testing/verification only, with no changes to core dependencies. The C backend requires no new dependencies, only implementation of existing gate types and proper multi-control handling.

## New Dependencies (Test/Verification Layer Only)

### Qiskit Ecosystem

| Package | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| qiskit | >=2.3.0 | Core quantum SDK for circuit verification | Required for all verification tasks |
| qiskit-aer | >=0.17.1 | High-performance quantum circuit simulator | Required for measurement-based verification |
| qiskit[qasm3-import] | >=2.3.0 | OpenQASM 3.0 string import capability | Required for loading exported QASM |

**Rationale:** These are TEST dependencies only, not runtime requirements. The framework generates OpenQASM 3.0 strings that can be consumed by any QASM 3.0 compliant tool. Qiskit provides the gold standard for verification during development and testing.

**Version justification:**
- Qiskit 2.3.0 (released Jan 8, 2026) is the latest stable version with mature QASM3 support
- qiskit-aer 0.17.1+ includes Python 3.13 support, GPU acceleration, and stable simulation APIs
- qiskit[qasm3-import] pulls in qiskit-qasm3-import >=0.6.0, which supports the stable `loads()` API

**Installation:**
```bash
# Test/verification dependencies (add to dev dependencies)
pip install qiskit>=2.3.0 qiskit-aer>=0.17.1
pip install "qiskit[qasm3-import]>=2.3.0"
```

## Existing Stack (No Changes Required)

| Technology | Current Version | Purpose | Status |
|------------|----------------|---------|--------|
| Python | >=3.11 | Frontend language | Maintained |
| Cython | >=3.0.11,<4.0 | C/Python bridge | Maintained |
| numpy | >=1.24 | Array operations | Maintained |
| C (gcc/clang) | - | Backend implementation | Maintained |

**No changes needed** to the core stack. The OpenQASM export is pure C string generation, and verification is test-only.

## Integration Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Python Test Suite (NEW)                                    │
│  ├─ Qiskit imports (qiskit, qiskit_aer, qiskit.qasm3)      │
│  └─ Verification workflow                                   │
└──────────────────┬──────────────────────────────────────────┘
                   │ calls
┌──────────────────▼──────────────────────────────────────────┐
│  Quantum Assembly Python Frontend (EXISTING)                │
│  ├─ qint/qbool/qarray classes                              │
│  └─ Operator overloading                                    │
└──────────────────┬──────────────────────────────────────────┘
                   │ calls via Cython
┌──────────────────▼──────────────────────────────────────────┐
│  C Backend (EXISTING + ENHANCEMENTS)                        │
│  ├─ circuit_to_openqasm3_string() [NEW FUNCTION]           │
│  ├─ circuit_to_opanqasm() [EXISTING - ENHANCE]             │
│  └─ Gate implementations (Y, R, Rx, Ry, Rz) [ENHANCE]      │
└─────────────────────────────────────────────────────────────┘
                   │ outputs
┌──────────────────▼──────────────────────────────────────────┐
│  OpenQASM 3.0 String                                        │
│  ├─ Consumed by Qiskit (verification)                       │
│  └─ Consumed by any QASM 3.0 tool (portability)            │
└─────────────────────────────────────────────────────────────┘
```

## API Usage Patterns

### 1. OpenQASM 3.0 Export (C Backend Enhancement)

**New function signature:**
```c
// Returns malloc'd string - caller must free()
char* circuit_to_openqasm3_string(circuit_t *circ);
```

**Enhancement to existing:**
```c
// Existing - writes to file, needs fix for file handle closure
void circuit_to_opanqasm(circuit_t *circ, char *path);
```

**Implementation requirements:**
- Use `sprintf()` or `snprintf()` with dynamic buffer for string generation
- Handle all gate types: X, Y, Z, H, P, R, Rx, Ry, Rz, M
- Handle multi-controlled gates via Control[0..1] and large_control array
- Use OpenQASM 3.0 `ctrl(n) @` modifier for n>2 controls

### 2. Qiskit QASM Import (CURRENT API - Qiskit 2.3.0)

**Recommended API (Stable):**
```python
from qiskit import qasm3

# Load from string (production use)
qasm_string = """
OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;
h q[0];
cx q[0], q[1];
"""
circuit = qasm3.loads(qasm_string)

# Load from file (legacy/testing)
circuit = qasm3.load("circuit.qasm")
```

**Status:** STABLE (but marked "exploratory release period")
- Requires `qiskit[qasm3-import]` extra to be installed
- `qiskit_qasm3_import` package must be >=0.6.0
- Issues no deprecation warnings (as of Qiskit 2.3.0)

**Experimental alternative (NOT RECOMMENDED):**
```python
# Faster Rust-based parser, but experimental and incomplete feature support
circuit = qasm3.loads_experimental(qasm_string)  # Issues ExperimentalWarning
```

**Rationale:** Use `loads()` over `loads_experimental()` because:
- Stable API surface (no breaking changes expected in Qiskit 2.x)
- Complete OpenQASM 3.0 feature support
- Experimental version lacks features we may need later

### 3. Qiskit Aer Simulation (CURRENT API - Qiskit Aer 0.17.1)

**DEPRECATED pattern (found in tests/run_qisk.py):**
```python
from qiskit import Aer, execute, transpile  # OLD - DO NOT USE

simulator = Aer.get_backend("aer_simulator")  # Removed in Qiskit 1.0
job = execute(circuit, backend=simulator)     # Deprecated in 0.46, removed in 1.0
```

**CURRENT pattern (Qiskit 2.3.0 + Aer 0.17.1):**
```python
from qiskit_aer import AerSimulator
from qiskit import transpile

# Create simulator instance
simulator = AerSimulator()

# Transpile circuit for simulator
transpiled = transpile(circuit, backend=simulator)

# Run simulation
job = simulator.run(transpiled, shots=1024)
result = job.result()
counts = result.get_counts()
```

**Key changes from old API:**
- Import from `qiskit_aer`, not `qiskit.Aer` (namespace changed in Qiskit 1.0)
- Use `AerSimulator()` constructor, not `Aer.get_backend()` (removed in 1.0)
- Use `transpile()` + `backend.run()`, not `execute()` (removed in 1.0)
- Explicit transpilation gives better control and visibility

**Simulation methods:**
The simulator auto-selects method based on circuit, but can be explicit:
```python
# Auto-select (recommended)
sim = AerSimulator()

# Or specify method explicitly
sim = AerSimulator(method='statevector')  # Dense statevector
sim = AerSimulator(method='density_matrix')  # With noise
sim = AerSimulator(method='matrix_product_state')  # For certain circuits
```

**Rationale:** Use automatic method selection. AerSimulator intelligently chooses based on circuit structure and noise model.

### 4. Verification Workflow Pattern

**Complete workflow for measurement-based verification:**
```python
from qiskit import qasm3, transpile
from qiskit_aer import AerSimulator

# 1. Export from Quantum Assembly (via Cython binding)
qasm_string = quantum_assembly_circuit.to_openqasm3()  # NEW method

# 2. Load into Qiskit
circuit = qasm3.loads(qasm_string)

# 3. Add measurements (if not already present)
if not circuit.num_clbits:
    circuit.measure_all()

# 4. Transpile for simulator
simulator = AerSimulator()
transpiled = transpile(circuit, backend=simulator)

# 5. Execute
job = simulator.run(transpiled, shots=1024)
result = job.result()
counts = result.get_counts()

# 6. Verify expected distribution
assert '000' in counts or '111' in counts  # Bell state example
```

## OpenQASM 3.0 Specification Compliance

### Multi-Controlled Gate Syntax

**Standard gates in stdgates.inc:**
- Single-qubit: `p, x, y, z, h, s, sdg, t, tdg, sx, rx, ry, rz`
- Two-qubit: `cx, cy, cz, cp, crx, cry, crz, ch, cu, swap`
- Three-qubit: `ccx, cswap`

**Multi-control handling:**

For ≤2 controls, use explicit gate names:
```openqasm
// 0 controls
x q[0];

// 1 control (CNOT)
cx q[0], q[1];

// 2 controls (Toffoli)
ccx q[0], q[1], q[2];
```

For >2 controls, use `ctrl(n) @` modifier:
```openqasm
// 3 controls
ctrl(3) @ x q[0], q[1], q[2], q[3];

// 4 controls
ctrl(4) @ x q[0], q[1], q[2], q[3], q[4];
```

**Implementation mapping:**
```c
gate_t g;  // From circuit

if (g.NumControls == 0) {
    fprintf(f, "x q[%d];\n", g.Target);
} else if (g.NumControls == 1) {
    fprintf(f, "cx q[%d], q[%d];\n", g.Control[0], g.Target);
} else if (g.NumControls == 2) {
    fprintf(f, "ccx q[%d], q[%d], q[%d];\n",
            g.Control[0], g.Control[1], g.Target);
} else {
    // >2 controls: use large_control array and ctrl modifier
    fprintf(f, "ctrl(%d) @ x ", g.NumControls);
    for (int i = 0; i < g.NumControls; i++) {
        fprintf(f, "q[%d]", g.large_control[i]);
        if (i < g.NumControls - 1) fprintf(f, ", ");
    }
    fprintf(f, ", q[%d];\n", g.Target);
}
```

**Why this approach:**
- Explicit gates (cx, ccx) are more widely supported than ctrl modifier
- ctrl modifier is required for >2 controls (c3x, c4x not in standard library)
- Qiskit's qasm3.loads() handles both syntaxes correctly

### Rotation Gate Syntax

**Parameterized gates:**
```openqasm
// Phase gate
p(0.785398163) q[0];  // π/4 rotation

// Pauli rotations
rx(1.57079633) q[0];  // π/2 X rotation
ry(3.14159265) q[0];  // π Y rotation
rz(0.785398163) q[0]; // π/4 Z rotation

// Generic rotation (if implemented)
r(1.57, 0.0) q[0];  // r(θ, φ) - NOT in stdgates.inc
```

**Current C backend status:**
```c
// In circuit_output.c, lines 310-319:
case Y:   // break; (no-op)
case R:   // break; (no-op)
case Rx:  // break; (no-op)
case Ry:  // break; (no-op)
case Rz:  // break; (no-op)
```

**Required implementation:**
```c
case Y:
    fprintf(oq_file, "y ");
    break;
case Rx:
    fprintf(oq_file, "rx(%.20f) ", g.GateValue);
    break;
case Ry:
    fprintf(oq_file, "ry(%.20f) ", g.GateValue);
    break;
case Rz:
    fprintf(oq_file, "rz(%.20f) ", g.GateValue);
    break;
case R:
    // R gate may require decomposition or custom handling
    // OpenQASM 3.0 stdgates.inc doesn't have generic r(θ,φ)
    // May need: fprintf(oq_file, "// R gate not in stdgates.inc\n");
    break;
```

**Note on R gate:** The generic rotation gate R(θ, φ) is NOT in OpenQASM 3.0 stdgates.inc. If the C backend uses this, it needs either:
1. Decomposition to rx, ry, rz sequence
2. Custom gate definition in QASM output header
3. Mark as unsupported in export

### Measurement Syntax

**Standard measurement:**
```openqasm
OPENQASM 3.0;
include "stdgates.inc";
qubit[2] q;
bit[2] c;

h q[0];
cx q[0], q[1];
c[0] = measure q[0];
c[1] = measure q[1];
```

**Current C backend (line 308):**
```c
case M:
    fprintf(oq_file, "m ");
    break;
```

**Problem:** `m` is not a valid OpenQASM gate. Correct syntax is `measure`.

**Required fix:**
```c
case M:
    // OpenQASM 3.0 measurement requires classical bit declaration
    // For now, use measure keyword (Qiskit will auto-add classical register)
    fprintf(oq_file, "measure ");
    break;
```

**Alternative:** Have Qiskit add measurements via `circuit.measure_all()` after import.

## What NOT to Add

### 1. Do NOT add PyQASM or openqasm3 parser packages

**Reasoning:**
- PyQASM is for validating QASM we didn't write (external sources)
- openqasm3 (reference parser) is for building compilers
- We only need to GENERATE valid QASM, not parse/validate it
- Qiskit's qasm3.loads() provides sufficient validation (will error on invalid QASM)

**If validation is needed later:** Add to test suite, not production dependencies.

### 2. Do NOT add qiskit-transpiler-service

**Reasoning:**
- Cloud-based transpilation service for IBM Quantum hardware
- We're doing local simulation only
- Local AerSimulator handles transpilation efficiently

### 3. Do NOT add Qiskit Runtime (qiskit-ibm-runtime)

**Reasoning:**
- For running on real IBM quantum hardware
- Milestone is verification via simulation only
- Adds cloud dependencies we don't need

### 4. Do NOT add Qiskit Primitives (Sampler/Estimator)

**Reasoning:**
- Higher-level abstraction for quantum applications
- We need low-level circuit verification
- AerSimulator provides direct circuit execution we need

### 5. Do NOT change build system

**Reasoning:**
- OpenQASM export is pure C string generation
- Qiskit verification is Python test code
- No new compiled dependencies
- Keep existing setuptools + Cython build

## Implementation Checklist

### C Backend Enhancements

- [ ] Implement `circuit_to_openqasm3_string()` returning malloc'd string
- [ ] Fix `circuit_to_opanqasm()` to close file handle (line 326 - no fclose)
- [ ] Add Y gate export: `fprintf(oq_file, "y ");`
- [ ] Add Rx gate export: `fprintf(oq_file, "rx(%.20f) ", g.GateValue);`
- [ ] Add Ry gate export: `fprintf(oq_file, "ry(%.20f) ", g.GateValue);`
- [ ] Add Rz gate export: `fprintf(oq_file, "rz(%.20f) ", g.GateValue);`
- [ ] Fix M gate export: change "m" to "measure"
- [ ] Handle large_control array for NumControls > 2
- [ ] Add classical bit declaration for measurements (optional - Qiskit can infer)

### Cython Bindings

- [ ] Add Python method `to_openqasm3() -> str` to circuit wrapper
- [ ] Ensure proper memory management (free C string after Python string created)

### Python Test Suite

- [ ] Update tests/run_qisk.py to use current API:
  - [ ] Replace `from qiskit import Aer` with `from qiskit_aer import AerSimulator`
  - [ ] Replace `Aer.get_backend()` with `AerSimulator()`
  - [ ] Replace `execute()` with `transpile()` + `backend.run()`
  - [ ] Use `qasm3.loads()` for string import
- [ ] Add measurement-based verification tests
- [ ] Add multi-controlled gate tests (3, 4, 5+ controls)

### Dependencies

- [ ] Add to pyproject.toml `[project.optional-dependencies]` section:
  ```toml
  [project.optional-dependencies]
  test = [
      "qiskit>=2.3.0",
      "qiskit-aer>=0.17.1",
      "qiskit[qasm3-import]>=2.3.0",
  ]
  ```
- [ ] Document that test dependencies are optional (not required for core functionality)

## Confidence Assessment

| Area | Confidence | Source | Notes |
|------|-----------|--------|-------|
| Qiskit version | HIGH | Official docs, PyPI | 2.3.0 released Jan 8, 2026 |
| Qiskit Aer API | HIGH | Official docs, release notes | 0.17.1 stable, clear migration path |
| qasm3.loads() API | HIGH | Official IBM Quantum docs | Stable, requires qasm3-import extra |
| OpenQASM 3.0 syntax | HIGH | Official spec at openqasm.com | ctrl(n) modifier for multi-control |
| Integration approach | HIGH | Existing codebase analysis | Clear separation: C export, Python verify |
| No new core deps | HIGH | Architecture analysis | QASM is strings, verification is test-only |

## Sources

**Qiskit API Documentation:**
- [qasm3 module - IBM Quantum Documentation](https://quantum.cloud.ibm.com/docs/api/qiskit/qasm3)
- [Qiskit releases - GitHub](https://github.com/Qiskit/qiskit/releases)
- [Qiskit on PyPI](https://pypi.org/project/qiskit/)

**Qiskit Aer:**
- [AerSimulator documentation](https://qiskit.github.io/qiskit-aer/stubs/qiskit_aer.AerSimulator.html)
- [Qiskit Aer releases](https://github.com/Qiskit/qiskit-aer/releases)
- [qiskit-aer on PyPI](https://pypi.org/project/qiskit-aer/)

**QASM3 Import:**
- [qiskit-qasm3-import GitHub](https://github.com/Qiskit/qiskit-qasm3-import)
- [qiskit-qasm3-import on PyPI](https://pypi.org/project/qiskit-qasm3-import/)

**OpenQASM 3.0 Specification:**
- [Gates - OpenQASM 3.0 Specification](https://openqasm.com/versions/3.0/language/gates.html)
- [Standard library - OpenQASM Live Specification](https://openqasm.com/language/standard_library.html)

**Migration Guides:**
- [Qiskit 1.0 feature migration guide](https://docs.quantum.ibm.com/migration-guides/qiskit-1.0-features)
