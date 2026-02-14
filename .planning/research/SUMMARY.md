# Project Research Summary

**Project:** Quantum Assembly v3.0 — Toffoli-Based Fault-Tolerant Arithmetic
**Domain:** Quantum circuit generation (Clifford+T gate set for fault-tolerant quantum computing)
**Researched:** 2026-02-14
**Confidence:** HIGH

## Executive Summary

Quantum Assembly currently implements all arithmetic operations (addition, multiplication, division) using QFT-based circuits that rely on phase rotation gates (CP). While these work correctly, they are unsuitable for fault-tolerant quantum computing where each small-angle rotation requires expensive synthesis into hundreds of T-gates. This milestone adds Toffoli-based arithmetic as an alternative backend, allowing users to switch via `ql.option('fault_tolerant', True)` for circuits optimized for error-corrected quantum computers.

The existing codebase infrastructure is already 95% compatible with this addition. All required gate primitives (CCX, CX, X) exist and work correctly. The sequence generation pipeline, qubit allocator, hot path dispatch mechanism, and operator overloading infrastructure are ready to use. The work is fundamentally about implementing new sequence generators (RCA/CLA adders, schoolbook multiplier, restoring divider) and wiring them through the existing dispatch mechanism. No new dependencies, languages, or architectural patterns are required.

The critical risk is ancilla qubit lifecycle management. QFT arithmetic uses zero ancilla qubits; Toffoli arithmetic uses O(n) ancilla that must be uncomputed to |0> after each operation. Incorrect uncomputation produces silent wrong results rather than crashes. Additionally, several existing bugs (reverse_circuit_range negating self-inverse gate values, qubit_array layout mismatches, allocator contiguous block reuse) must be fixed before Toffoli integration. The recommended approach: start with the simplest algorithm (CDKM ripple-carry adder with 1 ancilla), verify thoroughly, then build multiplication and division on top of the proven adder foundation.

## Key Findings

### Recommended Stack

The existing stack (C backend, Cython bindings, Python frontend) requires zero new dependencies. All Toffoli-based operations are implemented purely through existing gate primitives that are already functional.

**Core technologies:**
- **CDKM ripple-carry adder** (Cuccaro et al. 2004) — Foundation for all operations. 2n-1 Toffoli gates, 1 ancilla, O(n) depth. Simple to implement and verify.
- **Draper CLA adder** (optional upgrade) — O(log n) depth optimization. 5n-6 Toffoli gates, 2n-2 ancilla. Complex prefix tree logic. Defer until RCA proven.
- **Schoolbook multiplication** (Litinski 2024) — n^2+4n+3 Toffoli gates using controlled add-subtract technique. 50% gate reduction vs naive approach.
- **Restoring division** — Already implemented at Python level using comparisons and conditional subtraction. Automatically works with Toffoli adders underneath.
- **Existing infrastructure** — `ccx()`, `cx()`, `x()` gates, `run_instruction()` pipeline, `allocator_alloc()` for ancilla, hot path dispatch, sequence caching.

**Critical version/compatibility notes:**
- Existing `reverse_circuit_range()` function MUST be fixed before Toffoli integration (negates GateValue, breaks self-inverse gates).
- Allocator currently only reuses freed ancilla for count=1 allocations; CLA needs contiguous blocks (fix before CLA).

### Expected Features

**Must have (table stakes):**
- Ripple-carry adder (QQ, CQ, cQQ, cCQ variants) — All four variants required to match existing QFT API surface
- Subtraction via circuit inversion — Users write `a -= b`, must work transparently
- `ql.option('fault_tolerant', True/False)` backend selector — Same API, different gate set
- Schoolbook multiplication (QQ, CQ variants) — Core arithmetic operation
- Restoring division (quantum and classical divisor) — Already works at Python level, must preserve behavior
- Ancilla lifecycle management — Allocate, use, uncompute to |0>, free
- Verification against QFT results — Correctness guarantee for all operations

**Should have (competitive):**
- Carry look-ahead adder — O(log n) depth improvement for large widths (n >= 16)
- Controlled multiplication (cQQ, cCQ) — Needed for nested conditionals
- Hardcoded sequences (widths 1-8) — Performance parity with existing QFT hardcoded sequences
- Hot path C implementation — Eliminate Python/C boundary crossings
- T-count reporting — Primary cost metric for fault-tolerant QC (each Toffoli = 7 T-gates)
- Controlled add-subtract circuit — Litinski technique saves 50% Toffoli gates in multiplication

**Defer (v2+):**
- Karatsuba multiplication — Only helps for n > 100, far beyond target 1-64 bit range
- Non-restoring division — Marginal T-count improvement (15%) over restoring, significantly more complex
- Measurement-based uncomputation (Gidney's AND trick) — Requires mid-circuit measurement infrastructure
- Modular arithmetic (mod 2^n - 1) — Specialized for Shor's algorithm, not general-purpose arithmetic

### Architecture Approach

Follow the existing QFT arithmetic pattern exactly: parallel file structure with separate sequence generators, separate precompiled caches, shared execution pipeline. The ToffoliAddition.c module generates `sequence_t*` using MAJ/UMA gate patterns, ToffoliMultiplication.c builds on the adder as a subroutine. Backend selection happens at hot path dispatch via flag check.

**Major components:**
1. **ToffoliAddition.c** — Implements QQ_add_toffoli, CQ_add_toffoli, cQQ_add_toffoli, cCQ_add_toffoli using MAJ/UMA chains. Returns cached sequence_t*. Manages ancilla allocation internally.
2. **ToffoliMultiplication.c** — Schoolbook multiplication via n iterations of controlled add-subtract. Builds on Toffoli adder. Parallel to IntegerMultiplication.c.
3. **ToffoliDivision.c** (optional) — C-level restoring division gate generation. Alternative to existing Python-level implementation.
4. **hot_path_toffoli_add.c** — Qubit array assembly with ancilla positions, dispatch to run_instruction(). Parallel to hot_path_add.c.
5. **Backend dispatch** — Extend hot_path_add_qq() to check `fault_tolerant` flag and call Toffoli vs QFT sequence generator.

**Data flow:** Python operator → Cython dispatch → hot path (check backend flag) → sequence generator (build MAJ/UMA chain) → run_instruction (execute with qubit mapping) → circuit layers.

### Critical Pitfalls

1. **Ancilla qubits left dirty after uncomputation** — Toffoli adders use O(n) ancilla for carry bits that MUST be uncomputed to |0> or they corrupt all subsequent operations. QFT uses zero ancilla, so this infrastructure is untested. Prevention: MAJ/UMA chain must be symmetric (forward MAJ sweep, reverse UMA sweep). Add debug assertion that ancilla measure to 0 after free.

2. **reverse_circuit_range() negates GateValue for self-inverse gates** — Current code negates GateValue to invert gates (works for CP(θ) → CP(-θ)). For Toffoli gates (GateValue=1, self-inverse), negation produces GateValue=-1 which is undefined. This breaks subtraction and division uncomputation. Prevention: Fix reverse_circuit_range() BEFORE any Toffoli integration. Check gate type, skip negation for self-inverse gates (X, CX, CCX).

3. **qubit_array layout collision between QFT and Toffoli backends** — QFT adder uses [target, other] layout. Toffoli RCA uses [a, b, ancilla]. Toffoli CLA uses [a, b, g_ancilla, p_ancilla]. If backend selection happens at wrong level, qubit_array assembled for QFT will be executed with Toffoli sequence or vice versa, producing silent wrong results. Prevention: Separate hot path functions per backend OR explicit layout descriptor. Never share qubit_array assembly.

4. **Carry-lookahead generate/propagate tree index off-by-one** — CLA prefix tree has O(log n) levels with complex index arithmetic. Off-by-one errors produce circuits that work for n=1,2,4,8 (powers of 2) but fail for n=3,5,6,7. Prevention: Start with CDKM ripple-carry first (simple, 1 ancilla, well-understood). Only implement CLA after RCA proven. Exhaustive test CLA for all inputs at widths 1-6.

5. **Qubit count explosion from ancilla non-reuse** — Current allocator only reuses freed ancilla for count=1. CLA needs contiguous blocks (2n-2 ancilla). If not reused, each of n partial products in multiplication allocates fresh ancilla → O(n^2) total instead of O(n). Prevention: Fix allocator_alloc() to support contiguous block reuse from freed stack before implementing CLA or multiplication.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Infrastructure Fixes
**Rationale:** Three existing bugs block Toffoli integration. Must fix before any algorithm work.
**Delivers:** Fixed reverse_circuit_range() for self-inverse gates, fixed allocator contiguous block reuse, fixed qubit_array layout contracts.
**Addresses:** Pitfalls 2, 3, 5 from research.
**Avoids:** Retroactive fixes to working code (much harder than fixing prerequisites).

### Phase 2: CDKM Ripple-Carry Adder (All Variants)
**Rationale:** Simplest correct Toffoli adder. Only 1 ancilla, linear MAJ/UMA chain, well-documented. Foundation for all other operations.
**Delivers:** QQ_add_toffoli, CQ_add_toffoli, cQQ_add_toffoli, cCQ_add_toffoli. Verified against QFT addition for all widths 1-8.
**Uses:** Fixed allocator, fixed reverse_circuit_range for subtraction testing.
**Implements:** ToffoliAddition.c MAJ/UMA pattern, ancilla lifecycle management.
**Avoids:** CLA complexity (Pitfall 4), ancilla lifecycle bugs (Pitfall 1).

### Phase 3: Backend Dispatch Integration
**Rationale:** Wire Toffoli adder into operator overloading via fault_tolerant flag. Users can now switch backends for addition.
**Delivers:** `ql.option('fault_tolerant', True)` working for `+`, `-`, `+=`, `-=` operators.
**Uses:** hot_path_add.c modification, separate precompiled cache arrays.
**Implements:** Backend selection pattern that extends to multiplication/division.
**Avoids:** Backend collision (Pitfall 3), cached sequence mutation.

### Phase 4: Schoolbook Multiplication
**Rationale:** Built on proven Toffoli adder. Schoolbook with controlled add-subtract is the recommended approach from literature (Litinski 2024).
**Delivers:** QQ_mul_toffoli, CQ_mul_toffoli. Verified against QFT multiplication.
**Uses:** Controlled Toffoli adder (cQQ_add_toffoli from Phase 2).
**Implements:** ToffoliMultiplication.c, n iterations of shift-and-add with controlled add-subtract.
**Avoids:** Karatsuba complexity (deferred), add-subtract sign errors (test plain controlled addition first).

### Phase 5: Restoring Division (Automatic via Python)
**Rationale:** Already works at Python level. With Toffoli adders underneath, division automatically produces Toffoli circuits.
**Delivers:** Division and modulo operations working with fault_tolerant backend. No new C code required.
**Uses:** Existing qint_division.pxi comparison loop, Toffoli subtraction (inverted adder).
**Implements:** Verification that existing division logic composes correctly with Toffoli primitives.
**Avoids:** Rewriting division algorithm, non-restoring division complexity.

### Phase 6: Performance Optimization
**Rationale:** Algorithms proven correct, now optimize. Hardcoded sequences for common widths, hot path migration, controlled multiplication.
**Delivers:** Hardcoded Toffoli sequences (widths 1-8), hot_path_toffoli_add.c, cQQ_mul_toffoli, cCQ_mul_toffoli.
**Uses:** Existing hardcoding pattern from scripts/generate_seq_*.py, hot path pattern from Phase 60.
**Implements:** T-count reporting in circuit stats, comparative benchmarking vs QFT.

### Phase 7 (Optional): Carry Look-Ahead Adder
**Rationale:** Depth optimization for large widths. O(log n) vs O(n) depth. Only valuable for n >= 16.
**Delivers:** QQ_add_cla, automatic adder strategy selection based on width.
**Uses:** RCA as fallback, separate cache for CLA sequences.
**Implements:** Generate/propagate prefix tree (Brent-Kung structure), 2n-2 ancilla management.
**Avoids:** Index errors (Pitfall 4) via exhaustive testing against RCA results.

### Phase Ordering Rationale

- **Infrastructure first:** Fixing reverse_circuit_range and allocator before any algorithm work prevents painful retrofits. These bugs would silently corrupt Toffoli circuits.
- **RCA before CLA:** Ripple-carry is simpler (1 ancilla vs 2n-2, linear chain vs tree structure). Proves ancilla lifecycle management works before attempting complex CLA.
- **Adder before multiplication:** Multiplication uses adder as subroutine. Testing adder in isolation simplifies multiplication debugging.
- **Automatic division before C-level division:** Existing Python division composes with Toffoli primitives for free. C-level gate generation is optional optimization, not required for correctness.
- **Optimization last:** Hardcoding and hot path migration are pure performance improvements. Defer until algorithms proven correct via dynamic generation.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 7 (CLA):** Complex prefix tree index arithmetic. Draper paper has detailed description but implementation subtleties. Consider `/gsd:research-phase` for Brent-Kung tree structure.

Phases with standard patterns (skip research-phase):
- **Phase 1 (Infrastructure):** Bug fixes in existing code. Straightforward C modifications.
- **Phase 2 (RCA):** Cuccaro paper has explicit gate-by-gate description. MAJ/UMA pattern well-documented in Qiskit reference implementation.
- **Phase 3 (Dispatch):** Existing hot path pattern from Phase 60. Direct application.
- **Phase 4 (Multiplication):** Schoolbook algorithm is standard. Litinski paper provides exact gate counts and circuits.
- **Phase 5 (Division):** Uses existing Python code, no algorithm changes.
- **Phase 6 (Optimization):** Existing patterns for hardcoding and hot paths.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All gate primitives verified in gate.c. No new dependencies. Cuccaro/Draper algorithms are well-established (2004, 1000+ citations each). |
| Features | HIGH | Feature set derived from existing QFT API surface (must match) plus literature survey of standard Toffoli arithmetic operations. |
| Architecture | HIGH | Parallel structure to existing IntegerAddition.c/IntegerMultiplication.c. Pattern proven in codebase. MAJ/UMA decomposition from Cuccaro paper is explicit. |
| Pitfalls | HIGH | Codebase inspection revealed three critical bugs (reverse_circuit_range, qubit_array layout, allocator reuse) that directly impact Toffoli integration. Ancilla lifecycle management is well-understood risk from literature. |

**Overall confidence:** HIGH

### Gaps to Address

- **Exact CLA prefix tree implementation:** Draper paper describes algorithm conceptually but gate-level indexing requires careful translation. Qiskit CDKMRippleCarryAdder exists but no Qiskit CLA reference implementation found. Plan to implement RCA first, then use RCA as test oracle for CLA.

- **Controlled multiplication Toffoli count:** Litinski 2024 gives n^2+4n+3 for uncontrolled QQ_mul. Controlled version (cQQ_mul_toffoli) requires additional control qubit on all gates. Unsure if controlled add-subtract technique still applies or if naive 2x cost. Resolution: implement uncontrolled first, research controlled separately if needed.

- **Optimizer interaction with Toffoli circuits:** Optimizer gate cancellation and commutation rules designed for QFT circuits. Unclear if optimizer helps, hinders, or breaks Toffoli circuits. Plan: disable optimizer for Toffoli initially (flag check), test with optimizer enabled after correctness proven, fix commutation rules if needed.

- **BUG-DIV-02 and BUG-MOD-REDUCE interaction:** Division and modulo have existing deferred bugs. Division uses Toffoli comparison gates (IntegerComparison.c). Unknown if Toffoli arithmetic backend interacts with these bugs. Resolution: fix or work around existing bugs before Phase 5, or accept Phase 5 inherits existing limitations.

## Sources

### Primary (HIGH confidence)
- [Cuccaro et al. 2004 - arXiv:quant-ph/0410184](https://arxiv.org/abs/quant-ph/0410184) — CDKM ripple-carry adder: 2n-1 Toffoli, 5n-3 CNOT, 1 ancilla, MAJ/UMA gate decomposition
- [Draper et al. 2004 - arXiv:quant-ph/0406142](https://arxiv.org/abs/quant-ph/0406142) — CLA adder: 5n-6 Toffoli, O(log n) depth, 2n-2 ancilla, generate/propagate prefix tree
- [Litinski 2024 - arXiv:2410.00899](https://arxiv.org/html/2410.00899v1) — Schoolbook multiplication: n^2+4n+3 Toffoli with controlled add-subtract
- [Qiskit CDKMRippleCarryAdder](https://github.com/Qiskit/qiskit/blob/main/qiskit/circuit/library/arithmetic/adders/cdkm_ripple_carry_adder.py) — Reference implementation matching Cuccaro paper
- Codebase inspection — gate.c (ccx/cx/x verified working), execution.c (run_instruction pipeline), qubit_allocator.c (ancilla allocation API), IntegerAddition.c (QFT adder pattern), hot_path_add.c (hot path dispatch pattern)

### Secondary (MEDIUM confidence)
- [Thapliyal - arXiv:1609.01241](https://ar5iv.labs.arxiv.org/html/1609.01241) — Restoring division T-count: 35n^2-28n
- [Gidney 2018 - arXiv:1709.06648](https://arxiv.org/abs/1709.06648) — Measurement-based uncomputation (4n T-gates), deferred to future milestone
- [Nature - Higher radix CLA](https://www.nature.com/articles/s41598-023-41122-4) — CLA variations, not directly applicable
- [Comprehensive Study 2024 - arXiv:2406.03867v1](https://arxiv.org/html/2406.03867v1) — Survey of arithmetic circuits, contextual comparison

### Tertiary (LOW confidence)
- [Babbush - arXiv:1611.07995](https://arxiv.org/pdf/1611.07995) — Factoring with 2n+2 qubits, mentions Toffoli modular multiplication but not detailed enough for implementation

---
*Research completed: 2026-02-14*
*Ready for roadmap: yes*
