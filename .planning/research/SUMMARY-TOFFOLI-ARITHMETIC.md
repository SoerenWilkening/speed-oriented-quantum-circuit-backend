# Research Summary: Toffoli-Based Arithmetic

**Domain:** Quantum arithmetic circuit generation (Toffoli/Clifford gate basis)
**Researched:** 2026-02-14
**Overall confidence:** HIGH

## Executive Summary

Toffoli-based arithmetic adds a second arithmetic backend to the existing Quantum Assembly framework. Where the current QFT-based arithmetic uses Hadamard and controlled-phase gates to perform addition in the Fourier domain, Toffoli-based arithmetic uses classical-style carry propagation with Toffoli (CCX), CNOT (CX), and NOT (X) gates. This is significant for fault-tolerant quantum computing, where Toffoli gates decompose into a fixed number of T-gates, making resource estimation straightforward and compilation to error-corrected hardware practical.

The existing codebase has complete gate-level support for all required primitives. The `ccx()`, `cx()`, and `x()` functions in `gate.c` are already used extensively by bitwise operations (AND, OR, XOR in `LogicOperations.c`) and comparison operations (equality checks in `IntegerComparison.c`). The `sequence_t` data structure, `run_instruction()` execution pipeline, `qubit_allocator_t` ancilla management, and `hot_path_*` performance infrastructure all generalize cleanly to Toffoli-based arithmetic sequences.

The primary algorithms to implement are: (1) CDKM ripple-carry adder (1 ancilla, O(n) depth, simplest), (2) Draper carry look-ahead adder (n-1 ancilla, O(log n) depth, faster), (3) schoolbook multiplier (built from repeated additions), and (4) restoring division (built from subtraction + conditional restore). All four have well-established reference implementations and can be verified against the existing QFT-based operations.

The only meaningful decision is implementation ordering. The CDKM ripple-carry adder should come first because it is simple, well-understood, requires only 1 ancilla qubit, and serves as the foundation for multiplication and division. The carry look-ahead adder is an optimization that can be added later without changing any API.

## Key Findings

**Stack:** No new dependencies. All algorithms use existing `ccx()`/`cx()`/`x()` gate primitives. New C source files follow established patterns from `IntegerAddition.c` and `hot_path_add.c`.

**Architecture:** New C files (`ToffoliAddition.c`, `ToffoliMultiplication.c`, `ToffoliDivision.c`) parallel the existing QFT files. An arithmetic mode selector routes Python operators to either QFT or Toffoli hot paths.

**Critical pitfall:** Ancilla qubit cleanup. Every Toffoli-based operation allocates ancilla qubits that must be deterministically uncomputed (returned to |0>) and freed. Failure to uncompute leaves dirty ancilla that corrupt subsequent operations. The existing `reverse_circuit_range()` in `execution.c` handles this pattern, but the Toffoli-specific uncomputation must be built into each `sequence_t` (the uncomputation gates are part of the sequence itself, not separate).

## Implications for Roadmap

Based on research, suggested phase structure:

1. **CDKM Ripple-Carry Adder** - Foundation phase
   - Addresses: QQ_add_toffoli, CQ_add_toffoli, controlled variants
   - Avoids: Complexity of prefix tree (CLA) before basic infrastructure is proven
   - Rationale: Simplest algorithm (MAJ+UMA chain), 1 ancilla, directly testable against QFT adder

2. **Toffoli-Based Subtraction** - Minimal addition to Phase 1
   - Addresses: Subtraction via two's complement (NOT + add + carry handling)
   - Avoids: Implementing subtraction as a separate algorithm when it is addition with negation
   - Rationale: Required for multiplication (partial product accumulation) and division (trial subtraction)

3. **Schoolbook Multiplication** - Built on adder
   - Addresses: QQ_mul_toffoli, CQ_mul_toffoli, controlled variants
   - Avoids: Advanced multiplication techniques (Karatsuba, CAS) before basic version works
   - Rationale: n iterations of controlled-addition with shift, directly analogous to existing QFT multiplication

4. **Restoring Division** - Built on subtractor
   - Addresses: QQ_divmod_toffoli, CQ_divmod_toffoli, controlled variants
   - Avoids: Non-restoring or SRT division (more complex, less clear correctness)
   - Rationale: Already implemented at Python level -- just need to push to C for performance

5. **Carry Look-Ahead Adder** - Depth optimization
   - Addresses: O(log n) depth for addition-heavy circuits
   - Avoids: Premature optimization before basic algorithms are verified
   - Rationale: CLA is a drop-in replacement for RCA with same API, tested against same reference

6. **Hot Path Migration** - Performance optimization
   - Addresses: Python/C boundary crossing elimination
   - Avoids: Optimizing before algorithms are correct
   - Rationale: Follow Phase 60 pattern (proven approach)

**Phase ordering rationale:**
- Adder must come first (multiplication and division depend on it)
- Subtraction is a trivial extension of addition (same phase or immediately after)
- Multiplication requires adder but not subtractor
- Division requires subtractor (which requires adder)
- CLA is an independent optimization that can slot in after Phase 1 or after all algorithms
- Hot path is pure performance, should come last

**Research flags for phases:**
- Phase 1 (RCA): Standard patterns, unlikely to need research
- Phase 3 (Multiplication): May need deeper research on CAS optimization if basic schoolbook is too expensive
- Phase 5 (CLA): Needs deeper research on Brent-Kung prefix tree indexing

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All gate primitives verified in codebase, no new dependencies |
| Features | HIGH | Algorithms well-established in literature (2004+), clear gate decompositions |
| Architecture | HIGH | Directly parallels existing QFT architecture with same patterns |
| Pitfalls | MEDIUM | Ancilla cleanup is well-understood in theory but error-prone in practice |

## Gaps to Address

- **Exact CDKM gate indexing:** The paper uses abstract MAJ/UMA descriptions; the exact qubit index mapping for the `sequence_t` layer structure needs to be worked out during implementation. The Qiskit source code (`adder_ripple_c04` in qiskit.synthesis.arithmetic) provides a concrete reference.
- **Controlled addition for multiplication:** Whether to use a separate controlled-adder circuit or to wrap the adder with control-qubit decomposition (Toffoli -> controlled-Toffoli = 3-controlled X = mcx) needs to be decided during multiplication phase.
- **Brent-Kung tree edge cases:** The prefix tree for CLA has well-known edge cases at non-power-of-2 widths. This should be researched during the CLA phase, not upfront.
- **Restoring division Python-to-C translation:** The current Python-level division uses high-level `with can_subtract:` context manager. The C-level implementation needs to handle the conditional logic differently (as a controlled-addition sequence within the `sequence_t`).
