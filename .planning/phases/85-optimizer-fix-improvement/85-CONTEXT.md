# Phase 85: Optimizer Fix & Improvement - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix the circuit optimizer's latent loop direction bug, upgrade gate placement from O(L) linear scan to O(log L) binary search, and reduce @ql.compile replay overhead. No new optimizer features — correctness and performance only.

</domain>

<decisions>
## Implementation Decisions

### Golden-master verification
- Curated set of 15-20 representative circuits covering different operation types (add, mul, grover, QFT, bitwise, etc.)
- Stored as committed JSON files in the test directory — diffs visible in PRs
- JSON format with gate type, qubit indices, and layer number
- Kept permanently as ongoing regression tests for future optimizer changes

### Performance benchmarks
- Both synthetic large circuits (10K+ gates) AND real workloads (multiplication, Grover's, QFT) for benchmarking
- Any measurable improvement is satisfactory — the algorithmic change is the point
- Benchmark results stored in docs with tables AND scaling plots (gate count vs time)
- Combined end-to-end circuit generation time (optimizer + compile replay measured together, not separately)
- Test at small to large range: 100, 1K, 10K, 50K gates to show scaling behavior
- On-demand benchmarks only — not in CI
- 5 runs per benchmark, take median for stability

### Compile replay optimization
- Profile-driven plus low-hanging fruit — optimize hot spots AND obvious inefficiencies found during code review
- If dict iteration is a bottleneck, swap to a more efficient data structure (not just optimize usage)
- Prefer C-level replacement if data structure swap is needed — maximum performance
- Target both wall-clock time reduction and hot-path elimination as metrics
- Up to 5% regression acceptable on non-optimized paths if overall improvement is significant
- Caching for repeated gate patterns is acceptable if profiling shows benefit
- Internal data structures are not public API — break and fix tests if data structure changes
- Profiling infrastructure kept as opt-in after Phase 85 (activation mechanism at Claude's discretion)
- Use case for profiling: Claude picks representative compiled functions from the test suite

### Binary search implementation
- Standard bisect pattern implemented in C — well-understood algorithm, integrated into optimizer
- Include debug fallback: linear scan runs alongside to verify binary search correctness during development
- Remove debug linear scan fallback after golden-master verification confirms correctness

### Rollout order
- **Plan 1 (PERF-01):** Fix loop direction bug first, verify via manual inspection of a few circuits
- **Plan 2 (PERF-02):** Take golden-master snapshots of correct behavior, then implement binary search and verify
- **Plan 3 (PERF-03):** Optimize compile replay overhead and verify against golden-masters
- 3 separate plans — each independently verified and committed
- If bug fix causes test failures, fix tests to match correct behavior (the fix is correct)

### Claude's Discretion
- Profiling activation mechanism (env var vs Python flag)
- Which specific compiled functions to profile
- Exact golden-master circuit selection (within the 15-20 curated set guideline)
- Compression algorithm for gate pattern caching (if implemented)
- Specific data structure choice for dict replacement (sorted array, B-tree, etc.)

</decisions>

<specifics>
## Specific Ideas

- Benchmark docs should include scaling plots showing O(L) vs O(log L) curve — visual proof of improvement
- Debug linear scan is a temporary verification tool, not permanent code — remove once golden-masters confirm correctness

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 85-optimizer-fix-improvement*
*Context gathered: 2026-02-23*
