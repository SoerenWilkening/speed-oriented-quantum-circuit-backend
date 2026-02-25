# Phase 94: Parametric Compilation - Context

**Gathered:** 2026-02-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Compile quantum functions once and replay them with different classical values without re-capturing the gate sequence. Users decorate with `@ql.compile(parametric=True)`. Cache key includes `arithmetic_mode`, `cla_override`, and `tradeoff_policy` so mode switches invalidate correctly. Toffoli CQ operations and oracle decorators fall back to per-value caching. Execution, simulation, and new gate types are separate phases.

</domain>

<decisions>
## Implementation Decisions

### Decorator API design
- Parametric mode activated via flag on existing decorator: `@ql.compile(parametric=True)`
- When combined with `@ql.grover_oracle`, oracle wins — forces per-value caching silently (oracle parameters are structural by nature)
- Minimal introspection: only `.is_parametric` property exposed on compiled functions
- If function has no classical arguments, `parametric=True` is a silent no-op — behaves as normal compile

### Cache invalidation behavior
- Silent invalidation: cache key includes `arithmetic_mode`, `cla_override`, `tradeoff_policy` — switching modes causes a cache miss and transparent re-capture
- In-memory only cache, no disk persistence
- Unbounded cache size within session (circuits are typically few, sessions are finite)
- Toffoli CQ fallback documented in `@ql.compile` docstring only (no separate design doc)

### Replay semantics
- Automatic detection of classical vs structural parameters — compiler analyzes which params affect gate structure vs just gate arguments
- Fresh circuit returned each replay (no mutation of cached circuit objects)
- Toffoli CQ fallback to per-value caching is silent — user gets correct results, documented in docstring
- Fixed-shape inputs only — changing argument count/types is structural, triggers re-capture

### Error & edge case handling
- Ambiguous parameters fall back to per-value caching (correctness over speed)
- Type changes between calls (e.g., int then float) trigger re-capture and cache both versions
- `.clear_cache()` method exposed on compiled functions for manual cache invalidation
- Replay correctness verification is test-time only — no runtime overhead for users

### Claude's Discretion
- Internal cache data structure and key hashing approach
- Exact algorithm for detecting classical vs structural parameters
- How fresh circuits are constructed from cached gate sequences
- Test structure and specific test cases for verification

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 94-parametric-compilation*
*Context gathered: 2026-02-25*
