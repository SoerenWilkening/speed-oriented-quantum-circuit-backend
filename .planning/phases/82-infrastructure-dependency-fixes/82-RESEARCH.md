# Phase 82: Infrastructure & Dependency Fixes - Research

**Researched:** 2026-02-22
**Domain:** Build tooling (coverage), dependency management, import guards
**Confidence:** HIGH

## Summary

This phase establishes measurement infrastructure (pytest-cov + Cython coverage plugin + gcov/lcov for C) and fixes the qiskit-aer undeclared dependency before any code changes begin. The technical domain is well-understood: pytest-cov and coverage.py are stable, mature tools; the Cython.Coverage plugin is built into Cython 3.x and works with the existing `QUANTUM_PROFILE` build pattern; and lcov/genhtml can merge Python LCOV output with C gcov data into a single HTML report.

The project already has two qiskit-dependent simulation functions (`_simulate_single_shot` in `grover.py` and `_simulate_multi_shot` in `amplitude_estimation.py`) that use identical import patterns (`import qiskit.qasm3; from qiskit import transpile; from qiskit_aer import AerSimulator`). These will be consolidated into a new `sim_backend.py` wrapper module with lazy import guards. The `tests/conftest.py` also imports qiskit at module level, which needs addressing for the import guard strategy.

**Primary recommendation:** Use `pytest-cov` + `Cython.Coverage` plugin for Python/Cython, `gcov` + `lcov` for C, then merge via `lcov -a` + `genhtml` into a unified HTML report under `reports/coverage/`. Provide a `make coverage` target that orchestrates the full pipeline.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Measure Python (.py), Cython (.pyx), and C backend (.c) code coverage
- No minimum coverage threshold — report only, no build failures on low coverage
- HTML reports generated in `reports/coverage/` (gitignored)
- Exclude from measurement: `tests/*`, `setup.py`, `build_preprocessor.py`, `*_preprocessed.pyx`
- Unified report: merge C coverage (gcov/lcov) into the same HTML report as Python/Cython
- Coverage collection is opt-in via `pytest --cov` flag, not always-on
- C coverage build mode: Claude's discretion on env var vs build flag (follow existing `QUANTUM_PROFILE` pattern)
- Provide a convenience script or Makefile target (e.g., `make coverage`) for the full pipeline
- `pyproject.toml` is the single source of truth for all dependency metadata
- Remove `extras_require` and `install_requires` from `setup.py` (keep setup.py for Cython build only)
- Keep the extras group named `[verification]`
- Add `qiskit-aer>=0.13` to the `[verification]` group alongside `qiskit>=1.0`
- Coverage tooling deps (pytest-cov, Cython coverage plugin): Claude's discretion on group placement
- Lazy guards: imports succeed, error raised when simulation function is actually called
- Short generic message: "Simulation backend required. Install with: pip install quantum_language[verification]"
- Per-module guards (not a central helper function)
- Guard all qiskit-dependent modules, not just grover + amplitude_estimate
- Create a thin wrapper module at `src/quantum_language/sim_backend.py`
- Wraps qiskit/qiskit-aer with thin function wrappers
- Guards qiskit and qiskit-aer separately
- Migrate all existing qiskit imports across the codebase to use the wrapper in this phase
- Generic wording (no mention of "qiskit" in error messages) — future-proofs for backend swaps
- Record overall coverage %, per-module breakdown, and automatically identified critical untested paths
- Include timestamp and git commit hash for traceability
- Manual baseline recording (coverage script generates reports, recording baseline is a separate explicit step)
- Automated gap identification: script parses coverage data to find uncovered functions/methods

### Claude's Discretion
- C coverage build mode implementation (env var pattern preferred)
- Coverage tooling extras group placement ([dev] vs [test])
- Exact sim_backend.py function signatures and API surface
- Exact gcov/lcov integration approach for unified reporting
- Convenience script format (Makefile vs shell script)

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BUG-03 | Fix qiskit_aer undeclared dependency (add to pyproject.toml verification group, add friendly ImportError message) | Add `qiskit-aer>=0.13` to `[verification]` extras in pyproject.toml; create `sim_backend.py` wrapper with lazy import guards; migrate all qiskit imports in `src/` to use wrapper; generic error messages without mentioning qiskit |
| TEST-01 | Add pytest-cov infrastructure with Cython coverage plugin and coverage config in pyproject.toml | Configure `[tool.coverage.run]` with `Cython.Coverage` plugin in pyproject.toml; add `QUANTUM_COVERAGE` env var to setup.py for linetrace builds; add gcov/lcov pipeline for C code; Makefile `coverage` target |
| TEST-02 | Measure baseline coverage and identify critical untested paths | Run coverage pipeline after infrastructure is set up; parse JSON/XML output to identify uncovered functions; record baseline with timestamp + git hash |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| pytest-cov | >=6.0 | pytest plugin wrapping coverage.py | Standard pytest coverage integration; already uses pytest for testing |
| coverage.py | >=7.0 (transitive via pytest-cov) | Python/Cython code coverage measurement | Industry standard Python coverage tool |
| Cython.Coverage | (bundled with Cython 3.x) | Coverage plugin for .pyx files | Built into Cython; no extra install needed |
| gcov | (system, bundled with gcc 15.2) | C code coverage data collection | Standard GNU coverage tool, already available in environment |
| lcov | (needs install) | Coverage data aggregation and HTML generation | Standard tool for merging gcov/lcov data and generating HTML |
| genhtml | (bundled with lcov) | HTML report generation from LCOV data | Part of lcov package |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| qiskit-aer | >=0.13 | Quantum circuit simulation (already installed as 0.17.2) | Listed as optional `[verification]` dependency |
| py2lcov | (bundled with lcov 2.x) | Convert Python coverage.py data to LCOV format | Needed for unified Python+C report merging |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| lcov + genhtml | gcovr (Python-based) | gcovr generates HTML natively but doesn't natively merge with Python coverage; lcov has first-class Python coverage integration via py2lcov |
| Separate Python + C reports | Two independent HTML reports | Unified report is a locked decision; lcov merging solves this cleanly |

**Installation (for [dev] or [test] extras):**
```bash
pip install pytest-cov
# lcov must be installed via system package manager:
# apt-get install lcov  (or brew install lcov on macOS)
```

## Architecture Patterns

### Recommended Project Structure Changes
```
src/quantum_language/
├── sim_backend.py        # NEW: thin wrapper around qiskit/qiskit-aer
├── grover.py             # MODIFIED: use sim_backend instead of direct qiskit imports
├── amplitude_estimation.py  # MODIFIED: use sim_backend instead of direct qiskit imports
├── ...
pyproject.toml            # MODIFIED: add [tool.coverage.*] config, update extras
setup.py                  # MODIFIED: add QUANTUM_COVERAGE env var, remove extras_require/install_requires
Makefile                  # MODIFIED: add `coverage` target
scripts/
├── record_baseline.py    # NEW: parse coverage data, record baseline
reports/
├── coverage/             # NEW (gitignored): HTML coverage reports
```

### Pattern 1: Lazy Import Guard with Backend Wrapper
**What:** Create `sim_backend.py` that wraps qiskit imports with try/except and provides thin function wrappers. The guard fires at call time, not import time.
**When to use:** For all simulation-dependent code paths.
**Example:**
```python
# src/quantum_language/sim_backend.py

_QISKIT_AVAILABLE = None
_AER_AVAILABLE = None

def _check_qiskit():
    """Check if qiskit is importable (cached)."""
    global _QISKIT_AVAILABLE
    if _QISKIT_AVAILABLE is None:
        try:
            import qiskit  # noqa: F401
            _QISKIT_AVAILABLE = True
        except ImportError:
            _QISKIT_AVAILABLE = False
    return _QISKIT_AVAILABLE

def _check_aer():
    """Check if qiskit-aer is importable (cached)."""
    global _AER_AVAILABLE
    if _AER_AVAILABLE is None:
        try:
            import qiskit_aer  # noqa: F401
            _AER_AVAILABLE = True
        except ImportError:
            _AER_AVAILABLE = False
    return _AER_AVAILABLE

def _require_sim_backend():
    """Raise friendly error if simulation backend is not available."""
    if not _check_qiskit() or not _check_aer():
        raise ImportError(
            "Simulation backend required. "
            "Install with: pip install quantum_language[verification]"
        )

def load_qasm(qasm_str):
    """Load OpenQASM 3.0 string into a circuit object."""
    _require_sim_backend()
    import qiskit.qasm3
    return qiskit.qasm3.loads(qasm_str)

def simulate(circuit, shots=1, max_parallel_threads=4):
    """Simulate a circuit and return measurement counts."""
    _require_sim_backend()
    from qiskit import transpile
    from qiskit_aer import AerSimulator
    sim = AerSimulator(max_parallel_threads=max_parallel_threads)
    result = sim.run(transpile(circuit, sim), shots=shots).result()
    return result.get_counts()
```

### Pattern 2: QUANTUM_COVERAGE Env Var for Cython Linetrace Build
**What:** Follow existing `QUANTUM_PROFILE` pattern to enable Cython linetrace + `CYTHON_TRACE=1` macro when `QUANTUM_COVERAGE` env var is set. This is separate from `QUANTUM_PROFILE` because coverage linetrace has different performance characteristics than profiling.
**When to use:** When building for coverage measurement.
**Example:**
```python
# In setup.py, alongside existing QUANTUM_PROFILE block:
coverage_directives = {}
coverage_extra_args = []
if os.environ.get("QUANTUM_COVERAGE"):
    coverage_directives = {
        "linetrace": True,
    }
    coverage_extra_args = ["-DCYTHON_TRACE=1"]
```

### Pattern 3: C Coverage Build with gcov Flags
**What:** When `QUANTUM_COVERAGE` is set, also add `--coverage` (or `-fprofile-arcs -ftest-coverage`) to C compiler args. This causes gcc to emit `.gcno` and `.gcda` files that gcov/lcov can process.
**When to use:** As part of the unified coverage pipeline.
**Example:**
```python
# In setup.py, within the QUANTUM_COVERAGE block:
if os.environ.get("QUANTUM_COVERAGE"):
    compiler_args.extend(["--coverage"])
    # Note: --coverage is shorthand for -fprofile-arcs -ftest-coverage
```

### Pattern 4: Unified Coverage Pipeline (Makefile target)
**What:** A `make coverage` target that: (1) builds with coverage flags, (2) runs pytest with --cov, (3) captures C coverage with lcov, (4) converts Python coverage to LCOV, (5) merges, (6) generates HTML.
**When to use:** When measuring full project coverage.
**Example:**
```makefile
.PHONY: coverage
coverage:
	@echo "Building with coverage instrumentation..."
	. $(VENV) && QUANTUM_COVERAGE=1 $(PYTHON) setup.py build_ext --inplace --force
	@echo "Running tests with coverage..."
	. $(VENV) && $(PYTEST) tests/python -v --cov=quantum_language --cov-report=json --cov-report=lcov:reports/coverage/python.lcov
	@echo "Capturing C coverage..."
	lcov --capture --directory . --output-file reports/coverage/c.lcov --no-external
	@echo "Merging coverage data..."
	lcov -a reports/coverage/python.lcov -a reports/coverage/c.lcov -o reports/coverage/combined.lcov
	@echo "Generating HTML report..."
	genhtml reports/coverage/combined.lcov --output-directory reports/coverage/html
	@echo "Coverage report: reports/coverage/html/index.html"
```

### Anti-Patterns to Avoid
- **Always-on coverage in pytest.ini**: Coverage linetrace slows Cython code substantially. Keep it opt-in via `--cov` flag or Makefile target, never in `addopts`.
- **`CYTHON_TRACE=1` in normal builds**: The `CYTHON_TRACE` macro adds significant runtime overhead. Only enable via `QUANTUM_COVERAGE` env var.
- **Central import guard module with try/except at import time**: The user explicitly decided on lazy guards (error at call time, not import time) and a wrapper module pattern.
- **Mentioning "qiskit" in error messages**: User explicitly decided generic wording for future-proofing.
- **Putting `install_requires` in both setup.py and pyproject.toml**: User decided pyproject.toml is the single source of truth; remove from setup.py.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Python/Cython coverage measurement | Custom trace hooks | pytest-cov + Cython.Coverage plugin | Cython.Coverage is maintained by the Cython team; handles .pyx line mapping |
| C code coverage | Custom instrumentation | gcov + lcov | gcc's `--coverage` flag is standard; lcov handles HTML generation |
| Coverage data merging | Custom report combiner | lcov `-a` merge + genhtml | lcov natively understands both gcov and Python LCOV data formats |
| LCOV from Python coverage | Manual format conversion | `coverage lcov` (built into coverage.py 7.x) | coverage.py has native LCOV output since v6.3 |

**Key insight:** Coverage tooling is mature and standardized around the LCOV format. The Python ecosystem (coverage.py) and C ecosystem (gcov) both converge on LCOV, making unified reporting straightforward via lcov's merge command.

## Common Pitfalls

### Pitfall 1: Cython Coverage Without Linetrace Build
**What goes wrong:** Running `pytest --cov` without building with `CYTHON_TRACE=1` produces 0% coverage for all .pyx files. The Cython.Coverage plugin needs linetrace instrumentation in the compiled .so files.
**Why it happens:** Cython's coverage support requires explicit C macro and compiler directive enablement at build time. It's not automatic.
**How to avoid:** The `make coverage` target must rebuild with `QUANTUM_COVERAGE=1` before running tests. The Makefile target should enforce this.
**Warning signs:** All .pyx files show 0% coverage; only .py files have data.

### Pitfall 2: gcov Data Files (.gcda/.gcno) Location
**What goes wrong:** lcov can't find the C coverage data because `.gcda`/`.gcno` files are generated next to the object files, not next to the source. With setuptools/Cython builds, object files end up in `build/` subdirectories.
**Why it happens:** gcc generates `.gcno` at compile time (next to `.o`) and `.gcda` at runtime (same location). setuptools places `.o` files in `build/temp.*` directories.
**How to avoid:** Use `lcov --capture --directory . --no-external` to recursively search from project root. Alternatively, use `--directory build/` to target the specific build directory.
**Warning signs:** `lcov --capture` reports "no data found" or 0 tracefiles.

### Pitfall 3: setup.py install_requires vs pyproject.toml dependencies
**What goes wrong:** If both `setup.py` and `pyproject.toml` declare dependencies, pip may use either one depending on the installation method, causing confusion about which dependency list is authoritative.
**Why it happens:** PEP 621 (`pyproject.toml [project]`) is the modern standard, but `setup.py` `install_requires` is still respected by some build tools.
**How to avoid:** Remove `install_requires` and `extras_require` from `setup.py` entirely. Only keep setup.py for the Cython `build_ext` configuration (Extension definitions, cythonize). Move `Pillow>=9.0` from `setup.py install_requires` to `pyproject.toml [project] dependencies`.
**Warning signs:** `pip install -e .` installs different packages than `pip install quantum_language`.

### Pitfall 4: Preprocessed .pyx Files Confusing Coverage
**What goes wrong:** The Cython.Coverage plugin may try to map coverage data from `qint_preprocessed.pyx` instead of the original `qint.pyx`, leading to confusing reports with duplicated/unmapped lines.
**Why it happens:** The build preprocessor creates `*_preprocessed.pyx` files that are the actual compilation sources. The coverage plugin maps from the compiled C file back to the .pyx source.
**How to avoid:** Exclude `*_preprocessed.pyx` from coverage measurement (already in user's exclude list). Ensure the Cython.Coverage plugin can find the original .pyx source files. May need to test whether the plugin maps back to original or preprocessed sources.
**Warning signs:** Coverage report shows `qint_preprocessed.pyx` instead of `qint.pyx`, or shows no data for `qint.pyx`.

### Pitfall 5: Missing `Pillow` in pyproject.toml Dependencies
**What goes wrong:** When removing `install_requires` from `setup.py`, forgetting to move `Pillow>=9.0` to `pyproject.toml` will break `ql.draw_circuit()`.
**Why it happens:** Currently `Pillow` is only declared in `setup.py install_requires`, not in `pyproject.toml [project] dependencies`.
**How to avoid:** Add `"Pillow>=9.0"` to the `dependencies` list in `pyproject.toml` alongside `numpy>=1.24`.
**Warning signs:** `pip install quantum_language` no longer installs Pillow; `ql.draw_circuit()` fails.

### Pitfall 6: tests/conftest.py Module-Level qiskit Import
**What goes wrong:** The `tests/conftest.py` file imports `qiskit.qasm3` and `qiskit_aer` at module level. If qiskit-aer is not installed, ALL verification tests fail at collection time with `ModuleNotFoundError` before any test runs.
**Why it happens:** conftest.py is loaded by pytest during collection, not lazily.
**How to avoid:** This conftest.py is in `tests/` (the verification tests directory, not the main `tests/python/` directory). Since verification tests inherently require qiskit, this is acceptable — but consider adding a pytest skip marker or a try/except guard so other tests still run when qiskit is absent.
**Warning signs:** `pytest tests/python` fails even though it shouldn't need qiskit.

## Code Examples

### Coverage Configuration in pyproject.toml
```toml
# Source: coverage.py docs (https://coverage.readthedocs.io/en/latest/config.html)
[tool.coverage.run]
source = ["quantum_language"]
plugins = ["Cython.Coverage"]
omit = [
    "tests/*",
    "setup.py",
    "build_preprocessor.py",
    "*_preprocessed.pyx",
]

[tool.coverage.report]
show_missing = true
# No fail_under — report only, per user decision

[tool.coverage.html]
directory = "reports/coverage/html"
title = "Quantum Language Coverage Report"
```

### QUANTUM_COVERAGE Build Mode in setup.py
```python
# Source: Cython profiling docs (https://cython.readthedocs.io/en/latest/src/tutorial/profiling_tutorial.html)
# Follow existing QUANTUM_PROFILE pattern
coverage_directives = {}
if os.environ.get("QUANTUM_COVERAGE"):
    coverage_directives = {
        "linetrace": True,
    }
    compiler_args.append("-DCYTHON_TRACE=1")
    # Also enable C coverage via gcc --coverage
    compiler_args.append("--coverage")
```

### sim_backend.py Wrapper Module
```python
# New file: src/quantum_language/sim_backend.py
"""Thin simulation backend wrapper.

Abstracts the simulation backend (currently qiskit + qiskit-aer) behind a
generic interface.  Provides lazy import guards that produce a friendly
error message when the backend is not installed.

Usage from within quantum_language:
    from .sim_backend import load_qasm, simulate
    circuit = load_qasm(qasm_str)
    counts = simulate(circuit, shots=1)
"""

_QISKIT_AVAILABLE = None
_AER_AVAILABLE = None

_INSTALL_MSG = (
    "Simulation backend required. "
    "Install with: pip install quantum_language[verification]"
)

def _check_qiskit():
    global _QISKIT_AVAILABLE
    if _QISKIT_AVAILABLE is None:
        try:
            import qiskit  # noqa: F401
            _QISKIT_AVAILABLE = True
        except ImportError:
            _QISKIT_AVAILABLE = False
    return _QISKIT_AVAILABLE

def _check_aer():
    global _AER_AVAILABLE
    if _AER_AVAILABLE is None:
        try:
            import qiskit_aer  # noqa: F401
            _AER_AVAILABLE = True
        except ImportError:
            _AER_AVAILABLE = False
    return _AER_AVAILABLE

def _require_backend():
    if not _check_qiskit():
        raise ImportError(_INSTALL_MSG)

def _require_simulator():
    _require_backend()
    if not _check_aer():
        raise ImportError(_INSTALL_MSG)

def load_qasm(qasm_str):
    """Load OpenQASM 3.0 string into a circuit object."""
    _require_backend()
    import qiskit.qasm3
    return qiskit.qasm3.loads(qasm_str)

def simulate(circuit, *, shots=1, max_parallel_threads=4):
    """Simulate a circuit and return measurement counts dict."""
    _require_simulator()
    from qiskit import transpile
    from qiskit_aer import AerSimulator
    if not circuit.cregs:
        circuit.measure_all()
    sim = AerSimulator(max_parallel_threads=max_parallel_threads)
    result = sim.run(transpile(circuit, sim), shots=shots).result()
    return result.get_counts()
```

### Migrated grover.py _simulate_single_shot
```python
# After migration: grover.py uses sim_backend
from .sim_backend import load_qasm, simulate

def _simulate_single_shot(qasm_str, register_widths):
    circuit = load_qasm(qasm_str)
    counts = simulate(circuit, shots=1)
    bitstring = list(counts.keys())[0]
    return _parse_bitstring(bitstring, register_widths)
```

### Baseline Recording Script
```python
# scripts/record_baseline.py
"""Parse coverage data and record baseline metrics."""
import json
import subprocess
import datetime

def record_baseline():
    # Get git commit hash
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()

    # Parse coverage JSON report
    with open("reports/coverage/coverage.json") as f:
        data = json.load(f)

    baseline = {
        "timestamp": datetime.datetime.now().isoformat(),
        "commit": commit,
        "overall_percent": data["totals"]["percent_covered"],
        "modules": {},
        "uncovered_functions": [],
    }

    for filename, file_data in data["files"].items():
        baseline["modules"][filename] = {
            "percent": file_data["summary"]["percent_covered"],
            "missing_lines": file_data["summary"]["missing_lines"],
        }

    # Write baseline
    with open("reports/coverage/baseline.json", "w") as f:
        json.dump(baseline, f, indent=2)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `.coveragerc` file | `[tool.coverage.*]` in pyproject.toml | coverage.py 5.0+ (2019) | Single config file for all Python tooling |
| `coverage html` only for Python | `coverage lcov` + lcov merge for multi-language | coverage.py 6.3+ (2022) | Native LCOV output enables unified reports |
| `setup.py install_requires` | `pyproject.toml [project] dependencies` (PEP 621) | pip 21.3+ (2021) | Single source of truth for package metadata |
| `BasicAer` / `Aer.get_backend()` | `from qiskit_aer import AerSimulator` | qiskit 1.0 (2024) | BasicAer deprecated in Qiskit 1.0; qiskit-aer is separate package |

**Deprecated/outdated:**
- `qiskit.providers.basicaer`: Removed in Qiskit 1.0; use `qiskit_aer.AerSimulator` instead
- `.coveragerc` standalone file: Still works but `pyproject.toml` is preferred for new projects
- `setup.py install_requires` + `extras_require`: Still works but PEP 621 pyproject.toml is the modern standard

## Open Questions

1. **lcov availability in CI/local dev**
   - What we know: `gcov` is available (gcc 15.2), but `lcov` is NOT currently installed in the environment (no `lcov` or `genhtml` binary found)
   - What's unclear: Whether the Makefile target should gracefully degrade to Python-only coverage when lcov is not installed
   - Recommendation: Make the `make coverage` target check for lcov availability and fall back to Python/Cython-only HTML report (via `pytest --cov --cov-report=html`) if lcov is not installed. Print a warning about missing C coverage.

2. **Preprocessed .pyx coverage mapping**
   - What we know: `qint.pyx` is compiled via `qint_preprocessed.pyx` (inline preprocessing). The Cython.Coverage plugin maps from compiled C back to source .pyx.
   - What's unclear: Whether the plugin will map to `qint_preprocessed.pyx` or `qint.pyx`, and whether this causes issues.
   - Recommendation: Test empirically after setting up coverage. If it maps to preprocessed, may need to configure the plugin or post-process the report. The user already decided to exclude `*_preprocessed.pyx` from coverage measurement.

3. **Coverage data from `--coverage` flag interfering with normal builds**
   - What we know: gcc's `--coverage` flag emits `.gcno` at compile time and `.gcda` at runtime. These files accumulate in the build directory.
   - What's unclear: Whether `make clean` or `make coverage` should clean up .gcda/.gcno files between runs.
   - Recommendation: The `make coverage` target should `--force` rebuild (already planned) and clean up stale .gcda files before running. Add cleanup to `make clean`.

## Sources

### Primary (HIGH confidence)
- [coverage.py configuration docs](https://coverage.readthedocs.io/en/latest/config.html) - pyproject.toml configuration format, plugin support, HTML options
- [pytest-cov configuration docs](https://pytest-cov.readthedocs.io/en/latest/config.html) - pytest integration, report types
- [Cython profiling/coverage tutorial](https://cython.readthedocs.io/en/latest/src/tutorial/profiling_tutorial.html) - linetrace directive, CYTHON_TRACE macro, coverage plugin
- [Cython Coverage.py plugin source](https://github.com/cython/cython/blob/master/Cython/Coverage.py) - Plugin class, registration, requirements
- [qiskit-aer PyPI](https://pypi.org/project/qiskit-aer/) - Latest version 0.17.2, Python version support

### Secondary (MEDIUM confidence)
- [LCOV Python Coverage Integration](https://deepwiki.com/linux-test-project/lcov/4.1-python-coverage-integration) - py2lcov workflow, merge commands, genhtml
- [LCOV GitHub](https://github.com/linux-test-project/lcov) - lcov merge capabilities, genhtml usage
- [Better Cython coverage workflow](https://medium.com/@dfdeshom/better-test-coverage-workflow-for-cython-modules-631615eb197a) - Practical Cython coverage patterns

### Project-Verified (HIGH confidence)
- Existing codebase: `setup.py` lines 81-86 — `QUANTUM_PROFILE` env var pattern (verified via Read)
- Existing codebase: `grover.py` lines 258-260, `amplitude_estimation.py` lines 182-184 — qiskit import locations (verified via Grep)
- Existing codebase: `tests/conftest.py` lines 14-15 — module-level qiskit imports in verification tests (verified via Read)
- Existing codebase: `pyproject.toml` line 24 — current verification extras only lists `qiskit>=1.0` (verified via Read)
- Existing codebase: `setup.py` lines 169-179 — `install_requires` and `extras_require` to be removed (verified via Read)
- Environment: gcc 15.2, gcov 15.2 available; lcov NOT installed; qiskit-aer 0.17.2 installed (verified via Bash)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — pytest-cov, coverage.py, Cython.Coverage are mature and well-documented; gcov/lcov are standard C tools
- Architecture: HIGH — the sim_backend.py wrapper pattern is straightforward; coverage config in pyproject.toml is well-documented
- Pitfalls: HIGH — identified from direct codebase inspection (preprocessed .pyx, setup.py dual declaration, conftest.py imports)

**Research date:** 2026-02-22
**Valid until:** 2026-03-22 (stable tools, low churn risk)
