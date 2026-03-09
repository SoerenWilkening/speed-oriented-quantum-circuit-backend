---
phase: 119
slug: compile-compatibility
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 119 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | pytest 9.0.2 |
| **Config file** | pytest.ini |
| **Quick run command** | `pytest tests/python/test_compile_nested_with.py -x -v` |
| **Full suite command** | `pytest tests/python/test_compile_nested_with.py tests/python/test_nested_with_blocks.py tests/python/test_control_stack.py -v` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Run `pytest tests/python/test_compile_nested_with.py -x -v`
- **After every plan wave:** Run `pytest tests/python/test_compile_nested_with.py tests/python/test_nested_with_blocks.py tests/python/test_control_stack.py -v`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 119-01-01 | 01 | 1 | CTRL-06a | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_replay_both_true -x` | ❌ W0 | ⬜ pending |
| 119-01-02 | 01 | 1 | CTRL-06b | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_replay_inner_false -x` | ❌ W0 | ⬜ pending |
| 119-01-03 | 01 | 1 | CTRL-06c | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_replay_outer_false -x` | ❌ W0 | ⬜ pending |
| 119-01-04 | 01 | 1 | CTRL-06d | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_replay_both_false -x` | ❌ W0 | ⬜ pending |
| 119-01-05 | 01 | 1 | CTRL-06e | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_first_call_trade_off -x` | ❌ W0 | ⬜ pending |
| 119-01-06 | 01 | 1 | CTRL-06f | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileNestedWith::test_three_level_smoke -x` | ❌ W0 | ⬜ pending |
| 119-01-07 | 01 | 1 | CTRL-06g | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileInverseNestedWith -x` | ❌ W0 | ⬜ pending |
| 119-01-08 | 01 | 1 | CTRL-06h | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileAdjointNestedWith -x` | ❌ W0 | ⬜ pending |
| 119-01-09 | 01 | 1 | CTRL-06i | unit | `pytest tests/python/test_compile_nested_with.py::TestCompiledCallingCompiled -x` | ❌ W0 | ⬜ pending |
| 119-01-10 | 01 | 1 | CTRL-06j | unit | `pytest tests/python/test_compile_nested_with.py::TestCompileSingleLevelRegression -x` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/python/test_compile_nested_with.py` — stubs for CTRL-06 (all sub-requirements)
- No framework install needed — pytest and Qiskit already installed and working

*Existing infrastructure covers framework requirements.*

---

## Manual-Only Verifications

*All phase behaviors have automated verification.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
