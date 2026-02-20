---
status: complete
phase: 76-gate-primitive-exposure
source: 76-01-SUMMARY.md, 76-02-SUMMARY.md, 76-03-SUMMARY.md
started: 2026-02-20T09:00:00Z
updated: 2026-02-20T09:05:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Build and Import
expected: Package builds successfully with C compiler and imports without error. Run: pip install -e . && python -c "from quantum_language import qint"
result: pass

### 2. branch(0.5) Equal Superposition
expected: Calling x.branch(0.5) on a qint creates equal superposition. When measured, approximately 50% probability for each basis state. Verify by running: pytest tests/python/test_branch_superposition.py::test_branch_equal_superposition_widths_1_to_8 -v
result: issue
reported: "ModuleNotFoundError: No module named 'quantum_language._core' - conftest.py fails to import quantum_language"
severity: blocker

### 3. branch() with Other Probabilities
expected: branch(0.3) creates 30%/70% superposition, branch(0.7) creates 70%/30%. Verify by running: pytest tests/python/test_branch_superposition.py::test_branch_probability_values -v
result: issue
reported: "Same _core import error as Test 2"
severity: blocker

### 4. qbool.branch() Method
expected: qbool (1-qubit type) supports branch() method identically to qint. Verify by running: pytest tests/python/test_branch_superposition.py::test_qbool_branch -v
result: skipped
reason: Blocked by _core import error (same as Test 2)

### 5. Indexed branch (x[i].branch())
expected: Can target specific qubit for superposition via indexing. x[0].branch(0.5) only affects qubit 0. Verify by running: pytest tests/python/test_branch_superposition.py::test_indexed_branch -v
result: skipped
reason: Blocked by _core import error (same as Test 2)

### 6. Controlled branch (inside with block)
expected: branch() inside a with qbool block emits CRy (controlled Ry) instead of plain Ry. Verify by running: pytest tests/python/test_branch_superposition.py::test_controlled_branch -v
result: skipped
reason: Blocked by _core import error (same as Test 2)

### 7. branch() Validation
expected: branch(-0.1) and branch(1.5) raise ValueError for out-of-range probability. Verify by running: pytest tests/python/test_branch_superposition.py::test_branch_validation -v
result: skipped
reason: Blocked by _core import error (same as Test 2)

### 8. MCZ Gate Foundation
expected: emit_mcz() multi-controlled Z gate works correctly (0, 1, 2+ controls). This is foundation for diffusion operator. Verify by running: pytest tests/python/test_branch_superposition.py::TestMCZGate -v
result: skipped
reason: Blocked by _core import error (same as Test 2)

## Summary

total: 8
passed: 1
issues: 2
pending: 0
skipped: 5

## Gaps

- truth: "Tests run successfully via pytest with quantum_language import"
  status: failed
  reason: "User reported: ModuleNotFoundError: No module named 'quantum_language._core' - conftest.py fails to import quantum_language"
  severity: blocker
  test: 2
  root_cause: ""
  artifacts: []
  missing: []
  debug_session: ""

- truth: "branch() probability tests run via pytest"
  status: failed
  reason: "User reported: Same _core import error as Test 2"
  severity: blocker
  test: 3
  root_cause: ""
  artifacts: []
  missing: []
  debug_session: ""
