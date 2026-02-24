# Phase 86: QFT Bug Fixes - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix 4 QFT arithmetic bugs (BUG-04, BUG-05, BUG-06, BUG-08) with root-cause fixes applied in dependency order. Minimal targeted patches — no refactoring or new features. All previously-passing tests must continue to pass (zero regressions).

</domain>

<decisions>
## Implementation Decisions

### Fix dependency order
- Bottom-up execution: BUG-04 (mixed-width addition) → BUG-05 (controlled QQ addition) → BUG-06 (MSB ancilla leak) → BUG-08 (QFT division/modulo)
- One plan per bug, strict sequential execution — each must fully pass verification before the next begins
- If fixing BUG-04 also resolves BUG-05 (same root cause), collapse the plans and verify both together
- Same collapse logic applies to BUG-06/BUG-08 — fix BUG-06 first, check if BUG-08 resolves

### Fix scope & strategy
- Minimal targeted patches only — fix the bug, nothing more. No surrounding code cleanup
- Root-cause diagnosis captured in commit messages, not excessive code comments (comments only where fix isn't self-explanatory)
- If a bug is deeper than expected (e.g., fundamental QFT rotation flaw), attempt the deeper fix rather than escalating to a separate phase
- Remove xfail markers as part of each bug fix — tests should pass clean immediately after each fix

### Verification depth
- Test widths match success criteria exactly: up to 8 bits for addition, widths 2-4 for cQQ_add, widths 2-5 for division (width 6 exceeds 17-qubit simulation limit since QQ_div requires 3*width qubits)
- Width combinations for mixed-width addition: representative sampling (same-width, off-by-one, max asymmetry), not exhaustive
- Value coverage: boundary + random values at every width (0, 1, max, and a few random), not exhaustive enumeration
- Full test suite (all arithmetic tests) run after every single bug fix to catch regressions immediately
- Mod tested separately from division even though they share underlying code

### Division bugs approach
- BUG-06 (MSB ancilla leak) and BUG-08 (QFT division failures) are likely related — ancilla leak probably causes incorrect comparisons cascading into division failures
- Fix BUG-06 first, then run division tests to see if BUG-08 resolves
- "Zero xfail" means all tests pass within feasible simulation widths (≤5 bits). Failures at width 6+ due to qubit limits are acceptable
- Mod operations have their own test suite and must be verified independently

### Claude's Discretion
- Exact diagnostic approach for each bug (code analysis, bisection, etc.)
- Test fixture design and parameterization strategy
- Whether to add targeted regression tests beyond existing test files

</decisions>

<specifics>
## Specific Ideas

- QQ_div requires 3*width qubits — practical simulation ceiling is width 5 (15 qubits). Width 6 (18 qubits) exceeds the 17-qubit Qiskit simulation limit
- Success criteria #5 is paramount: zero regressions across add, sub, mul, div, mod, compare, bitwise

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 86-qft-bug-fixes*
*Context gathered: 2026-02-24*
