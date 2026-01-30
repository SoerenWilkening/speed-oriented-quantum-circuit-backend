# Feature Landscape: OpenQASM Export & Circuit Verification

**Domain:** Quantum circuit compilation and verification
**Researched:** 2026-01-30
**Confidence:** MEDIUM-HIGH

## Executive Summary

OpenQASM 3.0 export and circuit verification are table stakes for quantum frameworks in 2026. The feature landscape has matured significantly with OpenQASM 3.0 bringing classical control flow, mid-circuit measurements, and robust gate modifiers. Modern verification relies on statevector or shot-based simulation with deterministic test patterns for arithmetic, comparison, and conditional operations.

**Key insight:** The Quantum Assembly framework's deterministic circuit design (all outcomes predictable from classical initialization) is a significant advantage for verification testing—tests can use simple equality assertions rather than statistical confidence intervals.

## Table Stakes

Features users expect from any quantum framework with export/verification capabilities. Missing = product feels incomplete.

| Feature | Why Expected | Complexity | Dependencies | Notes |
|---------|--------------|------------|--------------|-------|
| **Core OpenQASM 3.0 export** | Industry standard interchange format | **Medium** | Existing `circuit_to_opanqasm` | QASM 3.0 is standard as of 2026 |
| Single-qubit gate export (X, H, Z, Y, P) | Basic quantum operations | **Low** | Existing gate types | Already partially implemented |
| Multi-controlled gate export (CX, CCX, C^n X) | Standard controlled operations | **Medium** | `large_control` array handling | OpenQASM 3.0 supports `ctrl(n) @` modifier |
| Qubit register declaration | Required for all QASM files | **Low** | Circuit qubit count | Already implemented |
| Comment/metadata in QASM | Circuit documentation, debugging | **Low** | None | Standard practice for readability |
| Rotation gate export (Rx, Ry, Rz) | Parameterized gates | **Low** | Existing gate types | Currently missing in export |
| **Statevector simulation verification** | Standard verification method | **Medium** | Qiskit/Cirq as dependency | Primary verification approach |
| Classical initialization test pattern | Verify circuit correctness | **Low** | X-gate initialization | Already implemented (`qint(value)`) |
| Measurement-based verification | Check computational outcomes | **Medium** | Simulation + outcome comparison | Standard for all quantum tests |
| Pass/fail reporting | Clear test results | **Low** | Test harness | Essential for CI/CD |
| Arithmetic verification test cases | Verify add/sub/mul operations | **Medium** | Classical init + simulate | Core use case for framework |
| Comparison verification test cases | Verify <, <=, ==, >=, >, != | **Medium** | Classical init + simulate | Core use case for framework |
| Python `ql.to_openqasm()` API | User-facing export function | **Low** | Cython bindings to C export | Expected Python API pattern |

## Differentiators

Features that set product apart. Not expected by all frameworks, but add significant value.

| Feature | Value Proposition | Complexity | Dependencies | Notes |
|---------|-------------------|------------|--------------|-------|
| **In-memory QASM export** (no file I/O) | Cleaner API, better testability | **Low** | String buffer instead of file | Most frameworks force file writes |
| **Deterministic verification** (no statistical noise) | Simpler tests, faster CI | **Low** | Framework's classical init design | Classical inputs → deterministic outputs |
| **Built-in verification script** | Works out of the box | **Medium** | Qiskit integration, test cases | Most users need to write their own |
| **Self-contained test suite** | No external test files needed | **Medium** | Embedded test cases in script | Reduces friction for validation |
| **Large control array QASM export** | Efficient n-controlled gates | **Medium** | `large_control` to `ctrl(n) @` mapping | Rare for frameworks to optimize this |
| Barrier export for circuit segmentation | Debugging, optimization boundaries | **Low** | Barrier concept in circuit | Useful for complex circuits |
| Conditional operation test cases | Verify quantum if/then logic | **High** | `with` statement verification | Tests framework's core abstraction |
| Bitwise operation verification | Verify AND, OR, XOR, NOT | **Medium** | Classical init + simulate | Framework-specific feature |
| Automatic test case generation | Reduce manual test writing | **High** | Property-based testing approach | Advanced QA capability |
| Detailed failure diagnostics | Pinpoint exact mismatch | **Medium** | State comparison, circuit diff | Better DX than pass/fail |

## Anti-Features

Features to explicitly NOT build. Common mistakes or scope creep in this domain.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **OpenQASM 2.0 export** | Deprecated format, limited capabilities | OpenQASM 3.0 only—industry moved on |
| **Custom gate definitions in QASM** | Complicates parsing, hardware compatibility | Decompose to standard gates in export |
| **Classical register tracking** | Framework is quantum-only for computation | Export measurements only when present |
| **Subroutine/def statements** | Qiskit has limited parse support (2026) | Flatten to gate sequence |
| **QASM file I/O in Python API** | Messy, requires path management | Return string, let user handle I/O |
| **Noise model simulation** | Out of scope, use Qiskit Aer for that | Focus on exact simulation for correctness |
| **Hardware backend execution** | Not a verification concern | OpenQASM export enables hardware access |
| **Statistical verification with shots** | Unnecessary for deterministic circuits | Use statevector for correctness tests |
| **Dynamic circuit features** (mid-circuit measurement, reset) | Not supported by framework yet | Defer to future work |
| **While/for loop export** | Framework doesn't generate loops | Circuit is fully unrolled |
| **GUI for verification** | Overkill, command-line sufficient | Standalone script with clear output |
| **Real-time verification** | Unrealistic for simulation | Batch verification is fine |

## Feature Dependencies

### OpenQASM Export Dependencies

```
Basic QASM structure (DONE in v1.0)
    ├── Single-qubit gates (DONE: X, H, Z, partially P)
    │   └── Rotation gates (TODO: Rx, Ry, Rz, proper P format)
    ├── Multi-controlled gates (PARTIAL: Control[2] works)
    │   └── Large control array (TODO: large_control → ctrl(n) @)
    ├── Qubit register (DONE)
    └── Proper error handling (TODO: NULL checks, file errors)

In-memory QASM export (v1.4 target)
    ├── String buffer allocation
    ├── Dynamic memory management
    └── NULL termination, return to Python

Python API (v1.4 target)
    ├── Cython wrapper for C function
    ├── String conversion to Python str
    └── Error propagation to Python exceptions
```

### Verification Dependencies

```
Classical initialization (DONE in v1.1)
    └── X-gate placement on specific qubits

QASM export (v1.4 target)
    ├── Valid OpenQASM 3.0 syntax
    └── All gate types represented

Qiskit integration (v1.4 target)
    ├── Parse QASM string
    ├── Create QuantumCircuit
    ├── Run statevector simulation
    └── Extract measurement outcomes or final state

Test case library (v1.4 target)
    ├── Arithmetic: add, subtract, multiply (multiple bit widths)
    ├── Comparison: <, <=, ==, >=, >, !=
    ├── Bitwise: AND, OR, XOR, NOT
    ├── Conditionals: with statement behavior
    └── Edge cases: 0, max value, overflow behavior

Result comparison (v1.4 target)
    ├── Expected vs actual state
    ├── Tolerance for floating point (phase)
    └── Clear error messages
```

## MVP Recommendation (v1.4 Scope)

### Must Have (Priority 1)

1. **Production-quality OpenQASM 3.0 export** (C function)
   - All gate types: X, Y, Z, H, P(theta), Rx, Ry, Rz
   - Multi-controlled gates: Control[2] and large_control array
   - Proper format: `ctrl(n) @ gate q[c0], q[c1], ..., q[target];`
   - Error handling: NULL circuit, invalid gates, file errors
   - In-memory string buffer (no forced file I/O)

2. **Python API `ql.to_openqasm()`**
   - Takes optional circuit object (default: current circuit)
   - Returns QASM string
   - Raises Python exceptions on error

3. **Standalone verification script**
   - Classical init → QASM export → Qiskit simulate → check outcomes
   - Built-in test cases for arithmetic and comparison
   - Clear pass/fail output with failure details

### Should Have (Priority 2)

4. **Comprehensive test case library**
   - Arithmetic: addition (4-bit, 8-bit), subtraction, multiplication
   - Comparison: all six operators (<, <=, ==, >=, >, !=)
   - Bitwise: AND, OR, XOR, NOT
   - Edge cases: 0, 1, max value

5. **Detailed failure diagnostics**
   - Show expected vs actual values
   - Display QASM snippet for failing operation
   - Circuit statistics (gates, depth) for context

### Nice to Have (Priority 3)

6. **Barrier export** (circuit segmentation)
7. **Comment annotations** (operation descriptions in QASM)
8. **Conditional operation tests** (quantum if/then verification)

### Explicitly Deferred

- OpenQASM 2.0 support (obsolete)
- Noise model simulation (use Qiskit Aer separately)
- Hardware execution (QASM export enables this separately)
- Dynamic circuits (mid-circuit measurement, reset)
- Statistical verification (deterministic circuits don't need shots)

## Standard Verification Patterns (2026)

Based on research of Qiskit, Cirq, and academic literature.

### Pattern 1: Statevector Equality

**Use case:** Verify deterministic circuits with classical inputs

```python
# Quantum Assembly pattern
x = qint(5, width=4)  # Classical init
y = qint(3, width=4)
z = x + y
qasm = ql.to_openqasm()

# Qiskit verification
from qiskit import QuantumCircuit
from qiskit.quantum_info import Statevector

circuit = QuantumCircuit.from_qasm_str(qasm)
state = Statevector.from_instruction(circuit)

# Check: z should be 8
expected_state = computational_basis_state("1000")  # Binary for 8
assert state == expected_state  # Exact equality for deterministic circuits
```

**Why it works:** Classical initialization produces deterministic superposition. Arithmetic operations are reversible. Final state is unique.

**Confidence:** HIGH (verified with Qiskit documentation)

### Pattern 2: Measurement-Based

**Use case:** Verify operations that include measurement

```python
# Quantum Assembly
a = qint(7, width=4)
b = qint(2, width=4)
result = (a > b)  # Returns qbool
# Measure result implicitly

qasm = ql.to_openqasm()

# Qiskit verification
from qiskit_aer import AerSimulator

circuit = QuantumCircuit.from_qasm_str(qasm)
simulator = AerSimulator()
result = simulator.run(circuit, shots=1).result()  # Deterministic → 1 shot sufficient
counts = result.get_counts()

# Check: should measure 1 (True) 100% of the time
assert counts == {'1': 1}  # 7 > 2 is True
```

**Why it works:** Deterministic circuits collapse to single outcome. No statistical noise for classical inputs.

**Confidence:** HIGH (standard Qiskit pattern)

### Pattern 3: Unitary Matrix Comparison

**Use case:** Verify circuit transformations preserve correctness

```python
# Used in Qiskit Algorithms testing (2025 study)
from qiskit.quantum_info import Operator

original = QuantumCircuit.from_qasm_str(qasm_v1)
optimized = QuantumCircuit.from_qasm_str(qasm_v2)

U1 = Operator(original)
U2 = Operator(optimized)

# Global phase doesn't matter
assert U1.equiv(U2)
```

**Why it works:** Optimization should preserve unitary transformation (up to global phase).

**Use for:** Verifying that optimization passes don't break circuits.

**Confidence:** HIGH (documented in Giallar, Qiskit testing research)

### Pattern 4: Property-Based Testing

**Use case:** Generate many test cases automatically

```python
# Hypothesis-style (not quantum-specific, but applicable)
from hypothesis import given, strategies as st

@given(st.integers(min_value=0, max_value=15),
       st.integers(min_value=0, max_value=15))
def test_addition_correctness(a, b):
    # Build circuit: x = qint(a), y = qint(b), z = x + y
    qasm = build_addition_circuit(a, b, width=4)
    result = simulate_and_extract(qasm)
    expected = (a + b) % 16  # 4-bit overflow
    assert result == expected
```

**Why it works:** Tests many input combinations, catches edge cases.

**Use for:** Comprehensive verification across input space.

**Confidence:** MEDIUM (not quantum-specific, but valid approach)

## Implementation Recommendations

### OpenQASM 3.0 Export Quality Checklist

Based on Cirq, Qiskit, and OpenQASM spec (2026).

- [ ] **Version header:** `OPENQASM 3.0;`
- [ ] **Include statement:** `include "stdgates.inc";`
- [ ] **Qubit register:** `qubit[N] q;`
- [ ] **Single-qubit gates:** `h q[0];`, `x q[1];`, `z q[2];`
- [ ] **Rotation gates:** `rx(0.5) q[0];`, `ry(1.57) q[1];`, `rz(3.14) q[2];`
- [ ] **Phase gate:** `p(1.5708) q[0];` (not `P4.0` as in current code)
- [ ] **Controlled gates (n=1):** `cx q[0], q[1];` or `ctrl @ x q[0], q[1];`
- [ ] **Multi-controlled (n=2):** `ccx q[0], q[1], q[2];` or `ctrl(2) @ x q[0], q[1], q[2];`
- [ ] **Large control (n>2):** `ctrl(5) @ x q[0], q[1], q[2], q[3], q[4], q[5];`
- [ ] **Measurement:** `m q[0];` or `bit c0; c0 = measure q[0];` (if classical register)
- [ ] **Barriers (optional):** `barrier q[0], q[1], q[2];`
- [ ] **Comments:** `// Operation: x + y`
- [ ] **No custom gates:** Decompose to standard gates
- [ ] **No subroutines:** Flatten all operations
- [ ] **Error handling:** Validate circuit before export

**Current gaps in `circuit_to_opanqasm` (lines 274-326):**
1. Writes to file—should support in-memory string buffer
2. Missing Rx, Ry, Rz export (lines 314-319: empty cases)
3. Y gate export missing (line 311: empty case)
4. Phase gate uses wrong format: `p(value)` not `P4.0` (line 296)
5. Large control array not exported (line 322: only uses `g.Control[i]`)
6. No barrier support
7. No comments
8. No error handling (NULL circuit, invalid gates)

### Verification Script Architecture

```
verify_circuits.py
├── Built-in test cases
│   ├── test_arithmetic()
│   │   ├── Addition: (5 + 3 = 8), (15 + 1 = 0 overflow)
│   │   ├── Subtraction: (8 - 3 = 5), (0 - 1 = 15 underflow)
│   │   └── Multiplication: (3 * 4 = 12), (5 * 5 = 9 overflow)
│   ├── test_comparison()
│   │   ├── Less than: 5 < 7 = True, 7 < 5 = False
│   │   ├── Equal: 5 == 5 = True, 5 == 3 = False
│   │   └── Greater: 7 > 5 = True, 5 > 7 = False
│   ├── test_bitwise()
│   │   ├── AND: 0b1100 & 0b1010 = 0b1000
│   │   ├── OR: 0b1100 | 0b1010 = 0b1110
│   │   └── XOR: 0b1100 ^ 0b1010 = 0b0110
│   └── test_conditionals()
│       └── with statement: if (x > y) then z = x else z = y
├── Qiskit integration
│   ├── parse_qasm(qasm_str) → QuantumCircuit
│   ├── simulate_statevector(circuit) → Statevector
│   ├── simulate_measurement(circuit) → counts
│   └── extract_value(state, qubits) → int
├── Comparison logic
│   ├── compare_states(expected, actual, tolerance)
│   ├── compare_measurements(expected, actual)
│   └── format_failure(test_name, expected, actual, qasm)
└── Reporting
    ├── print_summary(passed, failed, total)
    ├── print_failures(failure_list)
    └── exit_code(0 if all pass, 1 if any fail)
```

**Best practice:** Standalone script, no framework import needed. User runs `python verify_circuits.py` and sees results.

### Test Case Specifications

#### Arithmetic Tests

| Operation | Width | Input A | Input B | Expected | Notes |
|-----------|-------|---------|---------|----------|-------|
| Addition | 4 | 5 | 3 | 8 | Basic |
| Addition | 4 | 15 | 1 | 0 | Overflow |
| Subtraction | 4 | 8 | 3 | 5 | Basic |
| Subtraction | 4 | 0 | 1 | 15 | Underflow |
| Multiplication | 4 | 3 | 4 | 12 | Basic |
| Multiplication | 4 | 5 | 5 | 9 | Overflow (25 mod 16) |
| Addition | 8 | 100 | 28 | 128 | Larger width |
| Subtraction | 8 | 200 | 50 | 150 | Larger width |

#### Comparison Tests

| Operation | Width | Input A | Input B | Expected | Notes |
|-----------|-------|---------|---------|----------|-------|
| Less than | 4 | 5 | 7 | True | Basic |
| Less than | 4 | 7 | 5 | False | Basic |
| Less than | 4 | 5 | 5 | False | Boundary |
| Equal | 4 | 5 | 5 | True | Basic |
| Equal | 4 | 5 | 3 | False | Basic |
| Greater | 4 | 7 | 5 | True | Basic |
| Greater | 4 | 5 | 7 | False | Basic |
| Less/equal | 4 | 5 | 5 | True | Boundary |
| Greater/equal | 4 | 5 | 5 | True | Boundary |

#### Bitwise Tests

| Operation | Width | Input A | Input B | Expected | Binary Check |
|-----------|-------|---------|---------|----------|--------------|
| AND | 4 | 12 | 10 | 8 | 1100 & 1010 = 1000 |
| OR | 4 | 12 | 10 | 14 | 1100 \| 1010 = 1110 |
| XOR | 4 | 12 | 10 | 6 | 1100 ^ 1010 = 0110 |
| NOT | 4 | 5 | - | 10 | ~0101 = 1010 (4-bit) |
| AND | 4 | 15 | 0 | 0 | Edge: zero |
| OR | 4 | 0 | 0 | 0 | Edge: zero |

## Complexity Assessment

| Feature Category | Effort | Risk | Priority |
|------------------|--------|------|----------|
| **OpenQASM export (C)** | 2-3 days | Low | P0 (must have) |
| String buffer allocation | - | Medium | Implementation detail |
| All gate types | - | Low | Straightforward |
| Large control export | - | Medium | Format parsing |
| Error handling | - | Low | Standard C patterns |
| **Python API** | 1 day | Low | P0 (must have) |
| Cython wrapper | - | Low | Existing pattern |
| String conversion | - | Low | Standard Cython |
| **Verification script** | 3-4 days | Medium | P0 (must have) |
| Qiskit integration | - | Medium | External dependency |
| Test case library | - | Low | Straightforward |
| Result comparison | - | Low | Standard testing |
| Failure diagnostics | - | Medium | UI/formatting |
| **Total v1.4 effort** | **6-8 days** | **Low-Medium** | - |

**Risk factors:**
- Qiskit API changes (MEDIUM): Mitigation—pin Qiskit version, test on 1.0+
- QASM parsing errors (LOW): Mitigation—validate against OpenQASM spec
- Simulation performance (LOW): Mitigation—limit test case circuit size
- Memory leaks in C export (MEDIUM): Mitigation—valgrind testing

## Open Questions

1. **Should QASM export include circuit metadata?** (Gate count, depth, optimization level)
   - Leans NO: Keep QASM pure, metadata in separate function
   - Confidence: MEDIUM

2. **Should verification script support custom test cases?** (User provides input/expected pairs)
   - Leans YES: Increases utility, not much complexity
   - Implementation: Command-line flag `--custom tests.json`
   - Confidence: LOW (need user feedback)

3. **Should in-memory QASM use fixed buffer or dynamic allocation?**
   - Leans DYNAMIC: Circuits vary widely in size
   - Concern: Memory management complexity
   - Confidence: MEDIUM

4. **Should verification report include circuit visualizations?**
   - Leans NO for v1.4: Text output sufficient, add later if requested
   - Alternative: `--verbose` flag for circuit diagrams
   - Confidence: MEDIUM

## Sources

### High Confidence (Official Documentation)

- [OpenQASM 3 feature table | IBM Quantum Documentation](https://quantum.cloud.ibm.com/docs/en/guides/qasm-feature-table) - Qiskit OpenQASM 3.0 support matrix
- [Gates — OpenQASM Live Specification](https://openqasm.com/language/gates.html) - Multi-controlled gate syntax
- [Exact simulation with Qiskit SDK primitives | IBM Quantum Documentation](https://quantum.cloud.ibm.com/docs/guides/simulate-with-qiskit-sdk-primitives) - Statevector and shot-based simulation
- [Built-in quantum instructions — OpenQASM Specification](https://openqasm.com/language/insts.html) - Classical register initialization

### Medium Confidence (Research Papers, 2025-2026)

- [Optimizing Gate Decomposition for High-Level Quantum Programming – Quantum (March 2025)](https://quantum-journal.org/papers/q-2025-03-12-1659/) - Linear-time multi-controlled gate decomposition
- [Quantum Testing in the Wild: A Case Study with Qiskit Algorithms (January 2025)](https://arxiv.org/html/2501.06443v1) - Seven testing patterns in Qiskit
- [AutoQ 2.0: Verification of Quantum Programs (2025)](https://link.springer.com/chapter/10.1007/978-3-031-90660-2_5) - Conditional operation verification
- [Low-depth Quantum Circuit Decomposition of Multi-controlled Gates (2024)](https://arxiv.org/abs/2407.05162) - Multi-controlled gate depth optimization

### Medium Confidence (Web Search, Multiple Sources)

- [The Power of OpenQASM 3 for Real-Time Quantum-Classical Control](https://www.quantum-machines.co/blog/why-openqasm3-will-transform-the-quantum-dev-world/) - OpenQASM 3.0 capabilities
- [Import/export circuits | Cirq | Google Quantum AI](https://quantumai.google/cirq/build/interop) - Cirq OpenQASM 2.0/3.0 export
- [Advent of Code Day 1 on a Quantum Computer with Qiskit (2025)](https://aqora.io/blog/advent-of-code-quantum-edition-day-1) - Full adder verification example
- [DraperQFTAdder | IBM Quantum Documentation](https://quantum.cloud.ibm.com/docs/en/api/qiskit/qiskit.circuit.library.DraperQFTAdder) - Qiskit arithmetic circuit library

### Low Confidence (Single Source, Unverified)

- [Efficient Quantum Comparator Circuit – Egretta Thula (2023)](https://egrettathula.wordpress.com/2023/04/18/efficient-quantum-comparator-circuit/) - Comparator verification patterns (dated)

---

**Research complete.** This feature landscape provides clear prioritization for v1.4 milestone requirements. Table stakes cover 80% of scope, differentiators add competitive advantage, anti-features prevent scope creep.
