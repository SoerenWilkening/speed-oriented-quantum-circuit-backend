# Research Summary: v1.4 OpenQASM Export & Verification

**Synthesized:** 2026-01-30
**Sources:** STACK.md, FEATURES.md, ARCHITECTURE.md, PITFALLS-OPENQASM-EXPORT.md
**Overall Confidence:** HIGH

## Executive Summary

v1.4 adds production-quality OpenQASM 3.0 string export from the C backend and a standalone Qiskit-based verification script. The existing three-layer architecture (C → Cython → Python) requires minimal changes: one new C function returning a malloc'd string buffer, one Cython wrapper with memory-safe cleanup, and one Python convenience function. Qiskit is a TEST-ONLY optional dependency, never imported at package load time.

The current C export (`circuit_to_opanqasm`) has critical bugs — missing gates (Y, Rx, Ry, Rz are no-ops), invalid measurement syntax (`m` instead of `measure`), ignored `large_control` array, file handle leak, and a function name typo. The new implementation fixes all of these while adding in-memory string return.

## Key Findings

### 1. Stack Additions Are Minimal
- **Qiskit >=2.3.0** + **qiskit-aer >=0.17.1** as TEST-ONLY optional dependencies
- Use `qiskit.qasm3.loads()` (stable API) for in-memory QASM string import
- Use `AerSimulator()` + `transpile()` + `backend.run()` (current non-deprecated pattern)
- No changes to core stack (Python, Cython, numpy, C backend)

### 2. Architecture: Three-Layer String Export
- **C layer:** New `circuit_to_qasm_string()` returns `malloc()`'d buffer
- **Cython layer:** Decodes to Python `str`, frees C buffer in `finally` block
- **Python layer:** `ql.to_openqasm()` convenience function
- **Verification:** Standalone `scripts/verify_circuit.py` (not in package)
- Qiskit dependency isolated via `extras_require` — never imported at package load time

### 3. Current C Export Has Critical Bugs

| Bug | Location | Impact |
|-----|----------|--------|
| Y, R, Rx, Ry, Rz gates are no-ops | circuit_output.c:310-318 | Silent gate loss |
| M gate exports as `m` (invalid) | circuit_output.c:308 | QASM parse failure |
| `large_control` array ignored | circuit_output.c:322 | Wrong multi-controlled gates |
| No `fclose()` call | circuit_output.c:277+ | File descriptor leak |
| Function name typo (`opanqasm`) | circuit_output.c:274 | API confusion |
| No classical register for measurements | — | Invalid measurement syntax |

### 4. OpenQASM 3.0 Compliance Requirements
- Gate names lowercase: `x, y, z, h, p(θ), rx(θ), ry(θ), rz(θ)`
- Multi-control: `cx` (1 ctrl), `ccx` (2 ctrl), `ctrl(n) @ gate` (>2 ctrl)
- Measurement: `bit[n] c; ... c[i] = measure q[i];`
- Header: `OPENQASM 3.0;` + `include "stdgates.inc";`
- Angle format: `%.17g` (IEEE double precision)

### 5. Critical Pitfalls to Watch
1. **Qubit ordering mismatch** — Quantum Assembly right-aligned vs Qiskit little-endian (may need bit reversal in verification)
2. **Multi-controlled gate syntax** — Must use `ctrl(n) @` for >2 controls, not `ccc...x`
3. **Deterministic verification** — Classical init circuits need exact equality checks (1 shot sufficient), not statistical tests
4. **Phase precision** — Normalize angles to [0, 2π), use `%.17g` format
5. **File handle leak** — Existing code never closes opened file

### 6. Feature Scope (MVP)

**Must have:**
- Production-quality OpenQASM 3.0 string export (all gate types, large_control, error handling)
- Python API `ql.to_openqasm()` returning string
- Standalone verification script with built-in arithmetic/comparison test cases

**Should have:**
- Comprehensive test case library (arithmetic, comparison, bitwise, edge cases)
- Detailed failure diagnostics (expected vs actual, QASM snippet)

**Explicitly deferred:**
- OpenQASM 2.0 support, noise model simulation, hardware execution, dynamic circuits, statistical verification

## Build Order

1. **C String Export** — Fix all gates, add `circuit_to_qasm_string()`, handle `large_control`
2. **Cython Binding** — Add extern declaration, `to_openqasm()` method with memory safety
3. **Python API** — Module-level `ql.to_openqasm()` convenience function
4. **Measurement Support** — Ensure M gates export as proper `measure` syntax with classical register
5. **Verification Script** — Standalone script with Qiskit integration and built-in test cases

## Risk Assessment

| Risk | Level | Mitigation |
|------|-------|------------|
| Qiskit API deprecation | LOW | Pin version, use stable `loads()` API |
| Qubit ordering bugs | MEDIUM | Document mapping, test with known values |
| Memory leaks in C export | MEDIUM | Cython `try/finally` cleanup, valgrind testing |
| Large circuit performance | LOW | Pre-allocated buffer with size estimate |
| OpenQASM 3 feature limits | LOW | Stick to stdgates.inc subset |

## Implications for Roadmap

The build order naturally maps to 5 phases following the bottom-up dependency chain. Each phase is independently testable. The C export (Phase 1) is the foundation — all subsequent phases depend on valid QASM output. Measurement support (Phase 4) should be integrated before verification (Phase 5) since the verification script needs measurements to check outcomes.

Research flags Phase 5 (Verification) as potentially needing deeper investigation during planning to handle qubit ordering conventions correctly.

## Sources

- OpenQASM 3.0 specification (openqasm.com)
- Qiskit 2.3.0 documentation (quantum.cloud.ibm.com)
- Qiskit Aer 0.17.1 documentation
- Codebase analysis (circuit_output.c, _core.pyx, __init__.py)

---
*Research completed: 2026-01-30*
*Ready for roadmap: yes*
