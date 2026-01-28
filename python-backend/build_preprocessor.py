#!/usr/bin/env python3
"""Build preprocessor for quantum_language.pyx

Cython 3.0 removed support for 'include' directives inside class bodies.
This script preprocesses .pyx files to inline .pxi includes before Cython compilation.

Usage: python3 build_preprocessor.py
"""

import re
from pathlib import Path


def process_includes(source_file: Path, output_file: Path):
    """Process include directives in a .pyx file, inlining .pxi content."""

    content = source_file.read_text()
    lines = content.splitlines(keepends=True)

    result = []
    for line in lines:
        # Check for include directive (with or without indentation)
        match = re.match(r'^(\s*)include\s+"([^"]+)"', line)
        if match:
            indent = match.group(1)
            include_file = match.group(2)
            include_path = source_file.parent / include_file

            if include_path.exists():
                # Read and inline the included file
                included_content = include_path.read_text()
                result.append(f'{indent}# BEGIN include "{include_file}"\n')
                result.append(included_content)
                if not included_content.endswith("\n"):
                    result.append("\n")
                result.append(f'{indent}# END include "{include_file}"\n')
            else:
                # Keep original include line if file not found (let Cython handle error)
                result.append(line)
        else:
            result.append(line)

    output_file.write_text("".join(result))


if __name__ == "__main__":
    source = Path(__file__).parent / "quantum_language.pyx"
    output = Path(__file__).parent / "quantum_language_preprocessed.pyx"

    print(f"Preprocessing {source} -> {output}")
    process_includes(source, output)
    print("Preprocessing complete")
