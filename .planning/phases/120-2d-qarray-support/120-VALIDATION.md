---
phase: 120
slug: 2d-qarray-support
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 120 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | pytest 9.0.2 |
| **Config file** | pytest.ini |
| **Quick run command** | `python3 -m pytest tests/test_qarray.py tests/test_qarray_mutability.py -x -v` |
| **Full suite command** | `python3 -m pytest tests/test_qarray.py tests/test_qarray_mutability.py tests/test_qarray_elementwise.py tests/test_qarray_reductions.py -v` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `python3 -m pytest tests/test_qarray.py tests/test_qarray_mutability.py -x -v`
- **After every plan wave:** Run `python3 -m pytest tests/test_qarray.py tests/test_qarray_mutability.py tests/test_qarray_elementwise.py tests/test_qarray_reductions.py -v`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 120-01-01 | 01 | 0 | ARR-01, ARR-02 | unit | `pytest tests/test_qarray_2d.py -x -v` | ❌ W0 | ⬜ pending |
| 120-01-02 | 01 | 1 | ARR-02 | unit | `pytest tests/test_qarray_2d.py::test_row_augmented_assignment -x` | ❌ W0 | ⬜ pending |
| 120-01-03 | 01 | 1 | ARR-01 | unit | `pytest tests/test_qarray_2d.py::test_dim_qbool_construction -x` | ❌ W0 | ⬜ pending |
| 120-01-04 | 01 | 1 | ARR-02 | unit | `pytest tests/test_qarray_2d.py::test_qbool_ior -x` | ❌ W0 | ⬜ pending |
| 120-01-05 | 01 | 1 | ARR-02 | unit | `pytest tests/test_qarray_2d.py::test_view_identity -x` | ❌ W0 | ⬜ pending |
| 120-01-06 | 01 | 1 | ARR-02 | unit | `pytest tests/test_qarray_2d.py::test_qint_index_rejection -x` | ❌ W0 | ⬜ pending |
| 120-01-07 | 01 | 2 | - | regression | `pytest tests/test_qarray.py tests/test_qarray_mutability.py tests/test_qarray_elementwise.py -v` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_qarray_2d.py` — stubs for ARR-01 (dim-based qbool construction) and ARR-02 (qbool |=, row augmented assignment, view identity on dim-constructed arrays, qint index rejection)
- [ ] No framework install needed — pytest already configured

---

## Manual-Only Verifications

*All phase behaviors have automated verification.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
