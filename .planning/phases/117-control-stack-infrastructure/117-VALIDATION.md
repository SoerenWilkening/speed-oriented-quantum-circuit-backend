---
phase: 117
slug: control-stack-infrastructure
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 117 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | pytest (configured in `pytest.ini`) |
| **Config file** | `pytest.ini` |
| **Quick run command** | `pytest tests/python/test_nested_with_blocks.py::TestSingleLevelConditional -x -v` |
| **Full suite command** | `pytest tests/python/ -v --timeout=120` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Run `pytest tests/python/test_nested_with_blocks.py::TestSingleLevelConditional -x -v`
- **After every plan wave:** Run `pytest tests/python/ -v --timeout=120`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 117-01-01 | 01 | 1 | CTRL-02 | unit | `pytest tests/python/test_control_stack.py::test_push_makes_controlled -x` | Wave 0 | pending |
| 117-01-02 | 01 | 1 | CTRL-02 | unit | `pytest tests/python/test_control_stack.py::test_emit_ccx_gate_count -x` | Wave 0 | pending |
| 117-01-03 | 01 | 1 | CTRL-02 | unit | `pytest tests/python/test_control_stack.py::test_toffoli_and_allocation -x` | Wave 0 | pending |
| 117-01-04 | 01 | 1 | CTRL-03 | unit | `pytest tests/python/test_control_stack.py::test_pop_restores_uncontrolled -x` | Wave 0 | pending |
| 117-01-05 | 01 | 1 | CTRL-03 | unit | `pytest tests/python/test_control_stack.py::test_uncompute_toffoli_and -x` | Wave 0 | pending |
| 117-02-01 | 02 | 1 | CTRL-02/03 | regression | `pytest tests/python/test_nested_with_blocks.py::TestSingleLevelConditional -x -v` | Exists | pending |
| 117-02-02 | 02 | 1 | CTRL-02/03 | unit | `pytest tests/python/test_control_stack.py::test_circuit_reset_clears_stack -x` | Wave 0 | pending |
| 117-02-03 | 02 | 1 | CTRL-02/03 | unit | `pytest tests/python/test_control_stack.py::test_compile_save_restore -x` | Wave 0 | pending |

*Status: pending / green / red / flaky*

---

## Wave 0 Requirements

- [ ] `tests/python/test_control_stack.py` — unit tests for stack push/pop, emit_ccx, toffoli_and, uncompute_toffoli_and, circuit reset, compile save/restore
- [ ] Update `tests/python/test_nested_with_blocks.py` xfail markers — nested tests no longer raise NotImplementedError after Phase 117

*Existing `TestSingleLevelConditional` tests in `test_nested_with_blocks.py` provide regression coverage for single-level behavior.*

---

## Manual-Only Verifications

*All phase behaviors have automated verification.*

---

## Validation Sign-Off

- [ ] All tasks have automated verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
