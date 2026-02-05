# Technology Stack: Profiling and Optimization Tools

**Project:** Quantum Assembly v2.2 Performance Milestone
**Researched:** 2026-02-05
**Confidence:** HIGH (versions verified via PyPI, official docs)

## Executive Summary

This stack recommendation focuses on profiling and performance analysis tools for a Python/Cython/C hybrid codebase. The existing project uses Python 3.11+, Cython 3.0.11+, and a custom C backend. The recommended tools integrate with the existing build system (setup.py, pyproject.toml) and provide coverage across all three layers.

**Key constraint:** Python 3.12+ has broken Cython profiling support due to PEP-669. Pin profiling work to Python 3.11 for Cython-level analysis.

## Recommended Stack

### Python-Level Profiling

| Tool | Version | Purpose | Why |
|------|---------|---------|-----|
| cProfile | stdlib | Function-level timing | Built-in, zero install, works everywhere |
| line_profiler | 5.0.0 | Line-by-line timing | Identifies hot lines within functions; works with Cython when compiled with profiling |
| snakeviz | 2.2.2 | cProfile visualization | Browser-based flame graph and icicle charts; best pstats visualizer |
| pytest-benchmark | 5.2.3 | Regression testing | Integrates with existing pytest suite; tracks performance over time |

**Rationale:** cProfile is the standard first step. Once hot functions are identified, line_profiler drills down to specific lines. snakeviz provides visualization without external dependencies. pytest-benchmark prevents performance regressions by integrating with CI.

### Memory Profiling

| Tool | Version | Purpose | Why |
|------|---------|---------|-----|
| memray | 1.19.1 | Memory allocation profiling | Bloomberg's production-grade tool; traces Python AND native code; no annotation required |
| pytest-memray | latest | pytest integration | Memory limit enforcement per test; catches memory regressions |

**Rationale:** memray replaced memory_profiler (now unmaintained) as the industry standard. It traces allocations through C extensions, which is critical for this project's Cython/C architecture. pytest-memray integrates with the existing test suite.

**Note:** memray only works on Linux and macOS. Windows developers should use WSL2.

### Cython-Level Analysis

| Tool | Version | Purpose | Why |
|------|---------|---------|-----|
| cython -a | 3.2.4 | Annotation HTML reports | Shows Python/C API interaction per line; yellow = slow, white = fast C |
| cython --annotate-coverage | 3.2.4 | Coverage integration | Combines coverage.xml with annotation for test-guided optimization |

**Compiler directives for profiling:**
```python
# cython: profile=True
# cython: linetrace=True  # For coverage, requires CYTHON_TRACE=1
```

**Rationale:** Cython's built-in annotation is the authoritative tool for identifying Python/C boundary crossings. The HTML output shows exactly which lines incur Python API overhead.

**Critical limitation:** Cython profiling is non-functional in CPython 3.12+ due to PEP-669 changes. Use Python 3.11 for Cython profiling.

### C-Level Profiling

| Tool | Version | Purpose | Why |
|------|---------|---------|-----|
| perf | v6.8+ | CPU profiling | Linux standard; Python 3.12+ has native perf support |
| valgrind --tool=callgrind | 3.20+ | Detailed call analysis | 100% accurate but slow; use for deep-dive analysis |
| valgrind --tool=massif | 3.20+ | Heap profiling | Memory usage over time; identifies allocation hot spots |

**Rationale:** perf is the fastest option for Linux production profiling. valgrind provides perfect accuracy for detailed analysis but has 10-50x overhead, so use it for targeted investigation only.

**Existing support:** The project Makefile already has `memtest` and `asan-test` targets for memory debugging.

### Cross-Layer Profiling

| Tool | Version | Purpose | Why |
|------|---------|---------|-----|
| py-spy | 0.4.1 | Sampling profiler | Attaches to running process; `--native` flag profiles C extensions |
| scalene | 2.1.3 | All-in-one profiler | CPU, GPU, memory in one tool; AI-powered suggestions; web UI |

**Rationale:** py-spy is essential for profiling without code modification and without slowing the target. scalene provides the most comprehensive single-tool solution but has more overhead than py-spy.

## Installation

### Core profiling (required)

```bash
# In project virtualenv
pip install "line-profiler>=5.0.0" "snakeviz>=2.2.2" "pytest-benchmark>=5.2.3"
pip install "memray>=1.19.1" "pytest-memray"
pip install "py-spy>=0.4.1" "scalene>=2.1.3"
```

### System tools (Linux)

```bash
# Ubuntu/Debian
sudo apt install linux-tools-common linux-tools-$(uname -r) valgrind

# macOS (valgrind via Homebrew, perf not available)
brew install valgrind
```

### pyproject.toml addition

```toml
[project.optional-dependencies]
profiling = [
    "line-profiler>=5.0.0",
    "snakeviz>=2.2.2",
    "pytest-benchmark>=5.2.3",
    "memray>=1.19.1",
    "pytest-memray",
    "py-spy>=0.4.1",
    "scalene>=2.1.3",
]
```

### setup.py profiling build

Add to `setup.py` for Cython profiling builds:

```python
import os

# Profiling compiler directives
profiling_directives = {}
if os.environ.get('QUANTUM_PROFILE'):
    profiling_directives = {
        'profile': True,
        'linetrace': True,
    }
    compiler_args.append('-DCYTHON_TRACE=1')

setup(
    # ...
    ext_modules=cythonize(
        extensions,
        language_level="3",
        compiler_directives={
            "embedsignature": True,
            **profiling_directives,
        },
    ),
)
```

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Python timing | cProfile | profile | cProfile is C-based, lower overhead |
| Line profiling | line_profiler | pprofile | pprofile is slower; line_profiler is industry standard |
| Memory | memray | memory_profiler | memory_profiler is unmaintained since 2022 |
| Visualization | snakeviz | tuna | snakeviz has more polished UI; tuna claims to solve some limitations but is less mature |
| Sampling | py-spy | pyinstrument | py-spy supports native extensions via `--native`; pyinstrument is pure Python only |
| All-in-one | scalene | austin | scalene has AI suggestions and better GPU support |
| C profiling | perf | gprof | gprof requires recompilation with -pg; perf works on any binary |

## What NOT to Add

| Tool | Why Avoid |
|------|-----------|
| memory_profiler | Unmaintained since 2022; use memray instead |
| profile (stdlib) | Pure Python, much slower than cProfile |
| pyinstrument | Cannot profile C extensions; use py-spy instead |
| gprof | Requires special compilation flags; doesn't work with shared libs; no multi-threading |
| Intel VTune | Overkill for this project; requires Intel CPUs |
| cProfile decorators everywhere | Use sampling (py-spy) for production; decorators have overhead |

## Tool Integration Workflow

### Phase 1: Identify Hot Functions

```bash
# Quick overview with cProfile
python -m cProfile -s cumtime -o profile.stats your_script.py
snakeviz profile.stats  # Opens browser visualization

# Or use scalene for combined CPU/memory
scalene your_script.py
```

### Phase 2: Drill Into Hot Functions

```bash
# Line-by-line for specific functions (add @profile decorator)
kernprof -l -v your_script.py

# Or use py-spy for zero-modification profiling
py-spy record -o profile.svg -- python your_script.py
py-spy record --native -o profile.svg -- python your_script.py  # Include C
```

### Phase 3: Memory Analysis

```bash
# Full memory trace
memray run -o output.bin your_script.py
memray flamegraph output.bin  # Generate visualization

# Pytest integration
pytest --memray tests/
```

### Phase 4: Cython Annotation

```bash
# Generate HTML annotation for all .pyx files
cython -a src/quantum_language/*.pyx

# View in browser - yellow lines = Python API calls = slow
open src/quantum_language/qint.html
```

### Phase 5: C-Level Deep Dive (Linux)

```bash
# CPU profiling with perf
perf record -g python your_script.py
perf report

# Memory profiling with valgrind
valgrind --tool=massif python your_script.py
ms_print massif.out.*
```

## Makefile Targets (Recommended Additions)

```makefile
# === Profiling Targets ===

.PHONY: profile
profile:
	@echo "Running cProfile..."
	. $(VENV) && python -m cProfile -s cumtime -o profile.stats $(SCRIPT)
	. $(VENV) && snakeviz profile.stats

.PHONY: profile-line
profile-line:
	@echo "Running line_profiler..."
	. $(VENV) && kernprof -l -v $(SCRIPT)

.PHONY: profile-memory
profile-memory:
	@echo "Running memray..."
	. $(VENV) && memray run -o memray.bin $(SCRIPT)
	. $(VENV) && memray flamegraph memray.bin

.PHONY: profile-native
profile-native:
	@echo "Running py-spy with native support..."
	. $(VENV) && py-spy record --native -o profile.svg -- python $(SCRIPT)

.PHONY: benchmark
benchmark:
	@echo "Running benchmarks..."
	. $(VENV) && $(PYTEST) tests/ --benchmark-only -v

.PHONY: benchmark-compare
benchmark-compare:
	@echo "Comparing against saved benchmarks..."
	. $(VENV) && $(PYTEST) tests/ --benchmark-compare

.PHONY: cython-annotate
cython-annotate:
	@echo "Generating Cython annotation HTML..."
	. $(VENV) && cython -a src/quantum_language/*.pyx
	@echo "Open src/quantum_language/*.html to view"

.PHONY: build-profile
build-profile:
	@echo "Building with profiling enabled..."
	. $(VENV) && QUANTUM_PROFILE=1 pip install -e .
```

## Integration with Existing Project

### Existing Infrastructure to Leverage

The project already has:

1. **Makefile targets** (`memtest`, `asan-test`) - extend with profiling targets
2. **pytest configuration** (`pytest.ini`, `conftest.py`) - add benchmark fixtures
3. **circuit-gen-results/** - existing benchmark comparison with other frameworks
4. **setup.py Cython compilation** - add profiling compiler directives

### Recommended Build Modes

| Mode | Command | Use Case |
|------|---------|----------|
| Standard | `pip install -e .` | Development, testing |
| Profiling | `QUANTUM_PROFILE=1 pip install -e .` | Cython profiling with line tracing |
| Debug | `CFLAGS="-g -O0" pip install -e .` | C debugging with valgrind |
| Release | `pip install .` | Production, benchmarks |

### pytest-benchmark Integration Example

```python
# tests/test_benchmark.py
import quantum_language as ql

def test_qint_add_benchmark(benchmark):
    """Benchmark qint addition operation."""
    def setup():
        ql.circuit()
        a = ql.qint(5, width=8)
        b = ql.qint(3, width=8)
        return a, b

    def add_operation(a, b):
        return a + b

    result = benchmark.pedantic(
        add_operation,
        setup=setup,
        rounds=100,
        warmup_rounds=5,
    )
    # No assertion - benchmark only measures time
```

## Confidence Assessment

| Area | Level | Reason |
|------|-------|--------|
| Python tools (cProfile, line_profiler) | HIGH | Stdlib and PyPI verified |
| Memory tools (memray) | HIGH | Bloomberg production tool, PyPI verified |
| Cython annotation | HIGH | Official Cython docs, version verified |
| C tools (perf, valgrind) | HIGH | Linux standard tools |
| Cross-layer (py-spy) | HIGH | PyPI verified, native support documented |
| Integration approach | MEDIUM | Based on existing project structure analysis |

## Sources

### Version Information (Verified 2026-02-05)
- [line_profiler 5.0.0 - PyPI](https://pypi.org/project/line-profiler/)
- [memray 1.19.1 - PyPI](https://pypi.org/project/memray/)
- [snakeviz 2.2.2 - PyPI](https://pypi.org/project/snakeviz/)
- [pytest-benchmark 5.2.3 - PyPI](https://pypi.org/project/pytest-benchmark/)
- [py-spy 0.4.1 - PyPI](https://pypi.org/project/py-spy/)
- [scalene 2.1.3 - PyPI](https://pypi.org/project/scalene/)
- [Cython 3.2.4 - PyPI](https://pypi.org/project/Cython/)

### Documentation
- [Python cProfile Documentation](https://docs.python.org/3/library/profile.html)
- [Cython Profiling Tutorial](https://cython.readthedocs.io/en/latest/src/tutorial/profiling_tutorial.html)
- [memray Documentation](https://bloomberg.github.io/memray/)
- [py-spy GitHub](https://github.com/benfred/py-spy)
- [scalene GitHub](https://github.com/plasma-umass/scalene)
- [Python perf Support](https://docs.python.org/3/howto/perf_profiling.html)

### Best Practices
- [Real Python - Profiling in Python](https://realpython.com/python-profiling/)
- [scikit-learn Cython Best Practices](https://scikit-learn.org/stable/developers/cython.html)
- [Baeldung - Profiling C++ on Linux](https://www.baeldung.com/linux/profiling-c-cpp-code)
