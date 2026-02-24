# Phase 87: Scope & Segfault Fixes - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix crashes at valid operation widths and controlled multiplication correctness. Three active bug fixes (BUG-01, BUG-02, BUG-07) plus explicit deferral of BUG-09 (BUG-MOD-REDUCE). No new capabilities — pure bug-fix phase.

</domain>

<decisions>
## Implementation Decisions

### BUG-MOD-REDUCE Disposition (BUG-09)
- **Defer with documented rationale** — do NOT attempt a fix in this phase
- Document why modular reduction needs a Beauregard-style algorithm redesign, drawing on findings from Phase 86 and prior investigations
- Update REQUIREMENTS.md to mark BUG-09 as explicitly deferred (not just pending)
- No specific future phase assignment — just explain the redesign requirement

### BUG-01: 32-bit Multiplication Segfault
- **Static increase of MAXLAYERINSEQUENCE** — do not implement dynamic calculation
- Set the limit high enough to accommodate **64-bit operations** as headroom (not just 32-bit)
- Separate plan from BUG-02 (independent investigation and fix)

### BUG-02: qarray *= scalar Crash
- **Investigate from scratch** — Claude debugs, identifies root cause, fixes
- Separate plan from BUG-01
- Test all widths 1-8 as specified in success criteria

### BUG-07: Controlled Multiplication Scope Uncomputation
- **Targeted patch** — fix the specific case where controlled multiplication results get corrupted by scope uncomputation, minimal blast radius
- **Keep pushing** even if root cause is deeply entangled with uncomputation architecture — find a workaround rather than deferring
- Claude investigates the corruption mechanism from scratch
- Claude picks representative test cases for verification

### Testing & Verification
- BUG-01: **Circuit generation only** — 32-bit multiplication exceeds 17-qubit simulation limit, verify it generates without segfault
- BUG-02: Test **all widths 1-8** as success criteria specify, simulate where feasible within 17-qubit limit
- BUG-07: Qiskit simulation to verify correctness of controlled multiplication in `with` blocks
- **Add regression tests** for each fix to the permanent test suite
- **Per-bug verification** — no full regression suite run needed after all fixes

### Claude's Discretion
- BUG-07 test case selection (representative controlled multiplication scenarios)
- Exact MAXLAYERINSEQUENCE value (as long as it supports 64-bit)
- BUG-02 root cause investigation approach
- BUG-09 deferral rationale wording (based on Phase 86 and prior findings)

</decisions>

<specifics>
## Specific Ideas

- Each bug gets its own separate plan with atomic commits
- BUG-01 and BUG-02 are segfault fixes (crash prevention); BUG-07 is a correctness fix
- BUG-07 should not be deferred even if the fix is architecturally imperfect — controlled multiplication must work

</specifics>

<deferred>
## Deferred Ideas

- BUG-MOD-REDUCE (BUG-09): Needs Beauregard-style algorithm redesign — documented and deferred from this phase
- BUG-06/BUG-08 (division ancilla leak, QFT division failures): Already deferred from Phase 86, require uncomputation architecture redesign

</deferred>

---

*Phase: 87-scope-segfault-fixes*
*Context gathered: 2026-02-24*
