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

## QFT Generation Benchmark

![QFT generation timing](circuit-gen-results/time_circuit_generation.pdf)

Wall-clock cost of building a standard n-qubit Quantum Fourier Transform with
`libquantum`, swept over 43 register widths from n=1 to n=2000 (logarithmically
spaced) and averaged over 10 repetitions per size. The QFT contains
n(n+1)/2 gates (one Hadamard per qubit and a controlled-phase between every
ordered pair) and produces a circuit of depth 2n−1.

Two curves are reported:

- **`cq_impr2`** — sequence generation only. The context runs in count-only
  mode (`qc_circuit_set_simulate(ctx, false)`), so each gate is counted but
  not stored; this isolates the cost of computing the gate sequence itself.
- **`cq2`** — full insertion. The same QFT, but each gate is appended into
  the circuit data structure (layer assignment, spatial index, occupancy
  tracking).

The benchmark source is `benchmarks/bench_qft.c`; raw averaged numbers are in
`benchmarks/qft_scaling.csv` (`size,time,memory,method`, time in seconds and
memory as peak RSS in bytes from `/proc/self/status`).

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
