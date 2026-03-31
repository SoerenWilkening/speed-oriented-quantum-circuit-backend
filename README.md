# circuit-c-backend

Standalone C library for quantum circuit generation and manipulation.

Produces `libquantum.so` (shared) and `libquantum.a` (static) with a stable C ABI.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Installing

```bash
cmake --install build --prefix /usr/local
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

## Project Structure

```
circuit-c-backend/
  src/           — C source files
  include/       — Public headers (the API contract)
  tests/         — Test programs
  benchmarks/    — Performance benchmarks
  CMakeLists.txt — Build system
```

## Status

This package is part of the Quantum Assembly micro-package refactoring.
Currently in scaffolding phase (Module 1.1). Source files will be extracted
from the monolith in Modules 1.2 through 1.15.
