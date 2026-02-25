# Phase 93: Depth/Ancilla Tradeoff - Context

**Gathered:** 2026-02-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Users can control whether the framework optimizes for circuit depth or qubit count when selecting adder implementations. Three modes: `auto`, `min_depth`, `min_qubits` via `ql.option('tradeoff', ...)`. Modular arithmetic primitives force RCA regardless of policy. CLA subtraction limitation handled transparently.

</domain>

<decisions>
## Implementation Decisions

### Option API design
- Global state only via `ql.option('tradeoff', value)` — no per-operation overrides
- Default mode is `auto` when no option is set
- Must be set before any arithmetic operations; changing after ops raises an error
- Getter: `ql.option('tradeoff')` with no value returns the current setting
- Invalid values (anything other than `auto`, `min_depth`, `min_qubits`) raise an error immediately

### Auto mode threshold
- Threshold is fixed internally — not user-configurable
- Determined by empirical benchmarking: measure depth/qubit counts for CLA vs CDKM at various widths, set crossover where CLA depth advantage justifies extra ancillas
- Decision based on operand width only — does not consider total circuit qubit budget
- Note: threshold may need revisiting later when error correction overhead is factored in

### Adder selection in explicit modes
- `min_depth`: always pick the method with minimal depth for the given width, even for small widths (2-3 bits)
- `min_qubits`: always use CDKM (fewest ancillas)
- Modular arithmetic: always force RCA regardless of tradeoff policy

### Selection visibility
- Silent by default — no logging or output about which adder was selected
- No circuit metadata about adder selection; users infer from circuit properties
- RCA override for modular ops is documented behavior — no runtime warning
- Invalid tradeoff values raise an error (fail fast)

### CLA subtraction handling
- Framework implements CLA subtraction via two's complement: X-gate negation + CLA addition
- In `min_depth` mode, subtraction seamlessly uses this approach
- When CLA truly can't be used, auto-fallback to CDKM
- Documented in code docstrings explaining the two's complement approach

### Claude's Discretion
- Exact empirical benchmark methodology and threshold value
- How to implement set-once-before-ops enforcement
- Two's complement negation circuit details
- Error message wording for invalid values and post-ops setting changes

</decisions>

<specifics>
## Specific Ideas

- "The minimal depth threshold might change later, based on error correction overhead" — design threshold as a named constant that's easy to update
- CLA subtraction via two's complement: negate one operand then use CLA for addition, rather than trying to invert the CLA circuit directly

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 93-depth-ancilla-tradeoff*
*Context gathered: 2026-02-25*
