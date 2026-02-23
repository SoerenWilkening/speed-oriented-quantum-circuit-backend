# Phase 83: Tech Debt Cleanup - Research

**Researched:** 2026-02-23
**Domain:** Code hygiene (dead code removal, build automation, documentation)
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **QPU removal strategy:** Full cleanup -- remove QPU.c, QPU.h, all #include/import lines, AND any functions/code that existed solely to support QPU functionality. Fix compile errors inline. Clean up build config too: remove QPU references from Makefiles, CMakeLists, CI workflows, and any other build scripts. Verification: compile check is sufficient; full test suite not required during removal (tests run separately).
- **Preprocessor drift check:** Pre-commit hook only, no CI check needed. Use pre-commit framework (.pre-commit-config.yaml), not a standalone script. On drift detection: auto-fix and stage -- regenerate the .pyx file automatically and stage it so the commit includes the fix.
- **Dead code removal scope:** Scan production code only: src/ and (if it existed) python-backend/ directories (skip tests, scripts, tooling). One-time cleanup: don't persist vulture config to the repo. If supposedly dead code turns out to be needed: remove it, and if tests break, restore it and add a comment explaining why it's needed.
- **Sequence generation docs:** Documentation lives inline in the generator script: docstring/comments at top, plus a --help flag. Quick reference style: one-liner command to regenerate, plus a note on when you'd need to. Clean up the generator script too: improve arg parsing, clearer output, make it more user-friendly. Add a Makefile target (e.g., `make generate-sequences`) so developers don't need to remember the exact command.

### Claude's Discretion
- Preprocessor drift detection approach (regenerate-and-diff vs checksum)
- Borderline vulture confidence cases (80-89%): examine each and make the call

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DEBT-01 | Remove dead QPU.c/QPU.h stubs and all references across C/Cython/Python layers | QPU.h is a trivial wrapper around circuit.h (Section: QPU Removal Analysis). 15 C files include QPU.h, 2 Cython .pxd files reference it, setup.py lists QPU.c, CMakeLists.txt lists QPU.c, tests/c/Makefile references QPU.c in 4 source lists. All can be replaced with circuit.h. |
| DEBT-02 | Automate qint_preprocessed.pyx generation with build-time sync and drift check | build_preprocessor.py already handles regeneration. Current --check mode is broken (no actual diff). Pre-commit local hook with regenerate-and-stage approach recommended (Section: Preprocessor Drift Detection). |
| DEBT-03 | Remove duplicate/dead code identified by vulture scan (unused Python functions, unreachable code) | vulture not currently installed. Scan targets: 12 Python files in src/quantum_language/ (Section: Dead Code Scanning). Cython .pyx files NOT scannable by vulture -- only .py files. |
| DEBT-04 | Document hardcoded sequence generation process and regeneration instructions | 7 generator scripts exist in scripts/ directory. Only generate_seq_all.py has good --help/argparse. Others need improvement. Makefile target needed (Section: Sequence Generation Documentation). |
</phase_requirements>

## Summary

Phase 83 is a low-risk code hygiene phase with four independent work streams. All four requirements can be addressed with high confidence using well-understood tools and patterns already partially established in the codebase.

**QPU removal** (DEBT-01) is the most mechanical task: QPU.h is a one-line wrapper that simply `#include "circuit.h"`, and QPU.c is an empty stub. Replacing `#include "QPU.h"` with `#include "circuit.h"` across 15 C files and updating 2 Cython .pxd files is straightforward. The circuit_t and quantum_int_t typedefs are defined in circuit.h, so all dependent code will compile without QPU.h.

**Preprocessor drift detection** (DEBT-02) builds on the existing build_preprocessor.py which already handles regeneration. The hook needs to regenerate qint_preprocessed.pyx and auto-stage it. The pre-commit framework's local hook mechanism supports this directly.

**Dead code removal** (DEBT-03) requires installing vulture, running it on the .py files in src/quantum_language/, and removing confirmed dead code. Note that vulture cannot scan .pyx files (Cython syntax), so the scan is limited to the 12 pure Python files.

**Sequence documentation** (DEBT-04) involves improving the existing generator scripts' --help output and adding Makefile targets. The scripts already have argparse and decent docstrings; they need polish and a unified entry point.

**Primary recommendation:** Execute in four parallel work streams: QPU removal first (affects build config), then preprocessor hook, dead code scan, and sequence docs (all independent).

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| vulture | >=2.11 | Dead Python code detection | Industry standard for Python dead code scanning |
| pre-commit | >=3.0 (already installed, v4.5.1) | Git hook framework | Already in use in this project |
| build_preprocessor.py | (project-local) | .pxi -> .pyx inlining | Already handles the core regeneration logic |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| ruff | >=0.9.0 (already configured) | Python linting | Already enforced via pre-commit |
| clang-format | v19.1.6 (already configured) | C formatting | Already enforced via pre-commit |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| vulture | pyflakes, pylint --unused | vulture is purpose-built for dead code; pyflakes catches fewer cases |
| regenerate-and-diff | checksum/hash comparison | Regenerate is more reliable -- checksums could false-positive on formatting changes; regeneration guarantees current output matches source |

**Installation:**
```bash
pip install vulture  # One-time use, not persisted to repo deps
```

## Architecture Patterns

### QPU.h -> circuit.h Migration Pattern

**What:** QPU.h contains only `#include "circuit.h"`. Every file that includes QPU.h can directly include circuit.h instead. The types `circuit_t` and `quantum_int_t` are defined in circuit.h.

**Current include chain:**
```
QPU.h -> circuit.h -> types.h, gate.h, qubit_allocator.h, optimizer.h, circuit_output.h, circuit_stats.h, circuit_optimizer.h
```

**Migration:** Replace `#include "QPU.h"` with `#include "circuit.h"` in all 15 C files. In Cython .pxd files, change `cdef extern from "QPU.h":` to `cdef extern from "circuit.h":`.

**Files requiring changes:**

C backend (replace `#include "QPU.h"` with `#include "circuit.h"`):
1. `c_backend/src/QPU.c` -- DELETE entirely
2. `c_backend/src/Integer.c`
3. `c_backend/src/IntegerComparison.c`
4. `c_backend/src/circuit_allocations.c`
5. `c_backend/src/circuit_output.c`
6. `c_backend/src/qubit_allocator.c`
7. `c_backend/src/optimizer.c`
8. `c_backend/src/ToffoliMultiplication.c`
9. `c_backend/include/Integer.h`
10. `c_backend/include/LogicOperations.h`
11. `c_backend/include/execution.h`
12. `c_backend/include/hot_path_add.h`
13. `c_backend/include/hot_path_mul.h`
14. `c_backend/include/hot_path_xor.h`
15. `c_backend/include/toffoli_arithmetic_ops.h`

Headers to DELETE:
- `c_backend/include/QPU.h`

Cython declarations (change extern from):
- `src/quantum_language/_core.pxd` (line 88)
- `src/quantum_language/openqasm.pxd` (line 3)

Build configs (remove QPU.c from source lists):
- `setup.py` (line 28: `f"{C_BACKEND}/QPU.c"`)
- `CMakeLists.txt` (line 10: `c_backend/src/QPU.c`)
- `tests/c/Makefile` (4 source lists referencing `$(BACKEND_SRC)/QPU.c`)

Other (clean up references in comments):
- `main.c` (line 21 comment mentions QPU_state -- update comment)
- `c_backend/src/execution.c` (line 5 comment mentions QPU_state)
- `src/quantum_language/_core.pyx` (line 19 comment mentions QPU_state)
- `src/quantum_language/_core.pxd` (line 95 comment mentions QPU_state)

### Pre-commit Local Hook Pattern

**What:** A local hook in .pre-commit-config.yaml that regenerates qint_preprocessed.pyx from qint.pyx + .pxi files and auto-stages the result.

**When to use:** Every commit that touches .pxi or qint.pyx files.

**Recommended approach: regenerate-and-stage** (Claude's discretion decision)

The regenerate approach is more reliable than checksumming because:
1. It guarantees the output always matches the current source
2. No false positives from formatting or encoding differences
3. Auto-fix+stage means the developer never needs manual intervention
4. The preprocessing is fast (single file, text manipulation only)

**Example hook configuration:**
```yaml
- repo: local
  hooks:
    - id: sync-preprocessed-pyx
      name: Sync qint_preprocessed.pyx from source
      entry: python build_preprocessor.py --sync-and-stage
      language: system
      files: '(qint\.pyx|qint_\w+\.pxi)$'
      pass_filenames: false
```

The build_preprocessor.py `--check` mode currently does NOT detect drift (both branches do the same thing). It needs a new `--sync-and-stage` mode that:
1. Regenerates qint_preprocessed.pyx
2. Compares with existing file
3. If different: overwrites and runs `git add` on the preprocessed file
4. If same: exits 0 (no action needed)

### Vulture Scan Pattern

**What:** One-time dead code scan with manual review of results.

**Scan scope:** Only pure Python (.py) files in `src/quantum_language/`:
```
src/quantum_language/__init__.py
src/quantum_language/_qarray_utils.py
src/quantum_language/amplitude_estimation.py
src/quantum_language/compile.py
src/quantum_language/diffusion.py
src/quantum_language/draw.py
src/quantum_language/grover.py
src/quantum_language/oracle.py
src/quantum_language/profiler.py
src/quantum_language/sim_backend.py
src/quantum_language/state/__init__.py
```

**NOT scannable by vulture:** .pyx/.pxd/.pxi files (Cython syntax causes parse errors).

**Command:**
```bash
vulture src/quantum_language/ --min-confidence 80 --exclude "*_preprocessed*"
```

### Sequence Generator Makefile Pattern

**What:** Unified Makefile target that runs all sequence generators.

**Current generators (7 scripts):**
| Script | Output | Notes |
|--------|--------|-------|
| `generate_seq_all.py` | `add_seq_{1-16}.c`, `add_seq_dispatch.c` | Good argparse, replaces deprecated 1_4 and 5_8 |
| `generate_seq_1_4.py` | DEPRECATED | Keep but mark clearly |
| `generate_seq_5_8.py` | DEPRECATED | Keep but mark clearly |
| `generate_toffoli_seq.py` | `toffoli_add_seq_{1-8}.c`, dispatch | Has argparse |
| `generate_toffoli_decomp_seq.py` | `toffoli_decomp_seq_{1-8}.c`, dispatch | Has argparse |
| `generate_toffoli_clifft_cq_inc.py` | `toffoli_clifft_cq_inc_{1-8}.c`, `toffoli_clifft_ccq_inc_{1-8}.c`, dispatch | Has argparse |
| `generate_toffoli_clifft_cla.py` | `toffoli_clifft_cla_*_{2-8}.c`, dispatch | Has argparse |

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Dead code detection | Manual code review | vulture | Automated, confidence-scored, repeatable |
| Pre-commit hook management | .git/hooks/pre-commit script | pre-commit framework | Already in project, handles environments, cross-platform |
| Include preprocessing | Manual copy-paste | build_preprocessor.py | Already exists, handles edge cases |

**Key insight:** This phase is entirely about removing things and automating existing processes. Every tool needed either already exists in the project or is a standard pip install.

## Common Pitfalls

### Pitfall 1: Include Guard Collisions After QPU.h Removal
**What goes wrong:** After removing QPU.h as intermediary, some files might include circuit.h multiple times (directly and via another header that includes it).
**Why it happens:** QPU.h's include guard (`CQ_BACKEND_IMPROVED_QPU_H`) was distinct from circuit.h's guard (`QUANTUM_CIRCUIT_H`). If a file included both QPU.h and circuit.h, they'd coexist. After migration, only circuit.h's guard matters.
**How to avoid:** circuit.h has proper include guards. Double-inclusion is safe. No action needed -- this is a non-issue but worth noting.
**Warning signs:** Compilation warnings about redefined macros or types.

### Pitfall 2: Cython .pxd extern Changes Require Full Rebuild
**What goes wrong:** Changing `cdef extern from "QPU.h"` to `cdef extern from "circuit.h"` in .pxd files requires a clean rebuild of all Cython extensions that cimport from those modules.
**Why it happens:** Cython caches C code. Old generated C files still reference QPU.h in #include directives.
**How to avoid:** Run `python setup.py build_ext --inplace --force` after changing .pxd files.
**Warning signs:** Build succeeds but import fails, or stale .c files reference QPU.h.

### Pitfall 3: Vulture False Positives on Cython-Called Functions
**What goes wrong:** vulture reports Python functions as dead that are actually called from Cython (.pyx) code via cimport or Python-level import.
**Why it happens:** vulture only analyzes .py files. It cannot see calls from .pyx files.
**How to avoid:** For each vulture finding, grep the entire src/ directory (including .pyx files) for the function name before removing it. Functions in `__init__.py` are especially prone to false positives (they're the public API surface).
**Warning signs:** Test failures after removing "dead" code.

### Pitfall 4: Pre-commit Hook Must Not Modify Unstaged Files
**What goes wrong:** The auto-fix hook regenerates and stages the preprocessed file, but if the developer has unstaged changes to .pxi files, the hook might stage unintended content.
**Why it happens:** The hook runs on staged changes but regeneration reads the working tree (not the index).
**How to avoid:** The hook's `files` pattern should trigger only on .pxi and qint.pyx. The regeneration reads from disk (which matches what the developer intends to commit). The `git add` stages only the preprocessed output.
**Warning signs:** Unexpected diff in staged files after commit attempt.

### Pitfall 5: Removing "Dead" __init__.py Exports Breaks Public API
**What goes wrong:** vulture flags imports in __init__.py as unused because they're re-exports, not local uses.
**Why it happens:** `from .module import Class` in __init__.py exists for the public API, not for local use.
**How to avoid:** Never remove imports from __init__.py based on vulture results alone. These are intentional re-exports.
**Warning signs:** `ImportError` when users do `import quantum_language; quantum_language.SomeClass`.

## Code Examples

### QPU.h Removal in C Files

Before:
```c
// c_backend/src/Integer.c
#include "QPU.h"
```

After:
```c
// c_backend/src/Integer.c
#include "circuit.h"
```

### QPU.h Removal in Cython .pxd Files

Before (src/quantum_language/_core.pxd):
```cython
cdef extern from "QPU.h":
    ctypedef struct circuit_t:
        pass

    ctypedef struct quantum_int_t:
        pass

    # instruction_t and QPU_state removed in Phase 11
    # Backend is now stateless - all functions take explicit parameters

    circuit_t *init_circuit();
    void print_circuit(circuit_t *circ);
    void free_circuit(circuit_t *circ);
```

After:
```cython
cdef extern from "circuit.h":
    ctypedef struct circuit_t:
        pass

    ctypedef struct quantum_int_t:
        pass

    circuit_t *init_circuit();
    void print_circuit(circuit_t *circ);
    void free_circuit(circuit_t *circ);
```

Before (src/quantum_language/openqasm.pxd):
```cython
cdef extern from "QPU.h":
    ctypedef struct circuit_t:
        pass
```

After:
```cython
cdef extern from "circuit.h":
    ctypedef struct circuit_t:
        pass
```

### Pre-commit Hook Configuration

```yaml
# Append to existing .pre-commit-config.yaml
- repo: local
  hooks:
    - id: sync-preprocessed-pyx
      name: Sync qint_preprocessed.pyx
      entry: python build_preprocessor.py --sync-and-stage
      language: system
      files: '(qint\.pyx|qint_\w+\.pxi)$'
      pass_filenames: false
```

### build_preprocessor.py Enhancement (--sync-and-stage mode)

```python
import subprocess

def sync_and_stage(src_dir: Path = SRC_DIR) -> int:
    """Regenerate preprocessed files and stage if changed.

    Returns 0 if no drift (or successfully fixed), 1 on error.
    """
    for pyx_file in sorted(src_dir.rglob("*.pyx")):
        if pyx_file.stem.endswith("_preprocessed"):
            continue
        content = pyx_file.read_text()
        if not INCLUDE_RE.search(content):
            continue
        output = pyx_file.with_name(pyx_file.stem + "_preprocessed" + pyx_file.suffix)

        # Read existing content before regeneration
        old_content = output.read_text() if output.exists() else ""

        # Regenerate
        process_includes(pyx_file, output)
        new_content = output.read_text()

        if old_content != new_content:
            print(f"Drift detected: {output.name} -- regenerated and staging")
            subprocess.run(["git", "add", str(output)], check=True)

    return 0
```

### Vulture Scan and Review

```bash
# Install (one-time, not persisted to deps)
pip install vulture

# Run scan
vulture src/quantum_language/ --min-confidence 80 --sort-by-size

# For each finding, verify it's not called from .pyx files:
# grep -r "function_name" src/quantum_language/ --include="*.pyx" --include="*.pxd"
```

### Makefile Target for Sequence Generation

```makefile
.PHONY: generate-sequences
generate-sequences:
	@echo "Regenerating all hardcoded sequence C files..."
	@echo "1/4: QFT addition sequences (widths 1-16)..."
	$(PYTHON) scripts/generate_seq_all.py
	@echo "2/4: Toffoli CDKM sequences (widths 1-8)..."
	$(PYTHON) scripts/generate_toffoli_seq.py
	@echo "3/4: Toffoli MCX-decomposed sequences (widths 1-8)..."
	$(PYTHON) scripts/generate_toffoli_decomp_seq.py
	@echo "4/4: Clifford+T sequences (CDKM + BK CLA)..."
	$(PYTHON) scripts/generate_toffoli_clifft_cq_inc.py
	$(PYTHON) scripts/generate_toffoli_clifft_cla.py
	@echo "All sequences regenerated."
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| QPU.h as API entry point | circuit.h as API entry point | Phase 4/11 | QPU.h became dead shim |
| Manual .pxi copy | build_preprocessor.py auto-inline | Phase consolidation | Preprocessed file can drift |
| No dead code scanning | vulture scan | Industry standard | One-time cleanup removes cruft |

**Deprecated/outdated:**
- `QPU.c` / `QPU.h`: Empty backward-compat stubs since Phase 11 (global state removal). All functionality moved to circuit.h ecosystem.
- `generate_seq_1_4.py` / `generate_seq_5_8.py`: Replaced by `generate_seq_all.py`. Already marked deprecated in their headers.

## Open Questions

1. **Vulture on .pyx files**
   - What we know: vulture cannot parse Cython syntax. Only .py files are scannable.
   - What's unclear: Whether there is significant dead code in .pyx files that this phase misses.
   - Recommendation: Accept the limitation. The .pyx files are the core compute layer and tend to have less dead code. Manual review could be done separately if desired.

2. **Pre-commit hook timing with build_preprocessor.py**
   - What we know: The hook needs to run build_preprocessor.py which imports from project root.
   - What's unclear: Whether `language: system` vs `language: python` is more appropriate.
   - Recommendation: Use `language: system` since the script is already in the project root and has no additional dependencies. The `system` language runs the entry command directly in the user's environment.

## Sources

### Primary (HIGH confidence)
- Direct codebase analysis of all referenced files (QPU.c, QPU.h, circuit.h, setup.py, CMakeLists.txt, Makefiles, .pxd files, build_preprocessor.py, .pre-commit-config.yaml, all generator scripts)
- `.planning/codebase/CONCERNS.md` -- confirms QPU.c/QPU.h are empty compat wrappers
- `.planning/codebase/STRUCTURE.md` -- confirmed directory layout and file purposes

### Secondary (MEDIUM confidence)
- [pre-commit.com](https://pre-commit.com) -- local hook syntax (repo: local, required fields)
- [vulture PyPI](https://pypi.org/project/vulture/) -- installation, usage, --min-confidence flag
- [vulture GitHub](https://github.com/jendrikseipp/vulture) -- dead code detection capabilities and limitations

## Metadata

**Confidence breakdown:**
- QPU removal: HIGH -- complete file audit done, all references identified, migration path is trivial (QPU.h is literally just `#include "circuit.h"`)
- Preprocessor drift: HIGH -- existing infrastructure in place, just needs hook wiring and a proper --check implementation
- Dead code scan: HIGH -- vulture is well-understood; limitation with .pyx files is documented
- Sequence docs: HIGH -- all 7 scripts identified, most already have argparse; Makefile pattern is straightforward

**Research date:** 2026-02-23
**Valid until:** 2026-03-23 (stable domain, no external dependencies changing)
