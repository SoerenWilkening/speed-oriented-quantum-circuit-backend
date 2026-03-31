# Benchmarks

Benchmark suite for `circuit-c-backend` (libquantum). Measures gate counts,
circuit depth, and generation time for arithmetic operations across register
widths.

## Operations benchmarked

| Operation       | Description                          | Width range |
|-----------------|--------------------------------------|-------------|
| `qft_add`       | QFT Draper addition (a += b)         | 1 - 64      |
| `toffoli_cdkm`  | Toffoli CDKM ripple-carry (a += b)   | 1 - 64      |
| `toffoli_cla`   | Toffoli Brent-Kung CLA (b += a)      | 2 - 64      |
| `qft_mul`       | QFT multiplication (result = a * b)  | 1 - 32      |
| `toffoli_mul`   | Toffoli multiplication               | 1 - 32      |

## Quick start

```bash
# Build (from repo root)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmark_runner

# Run benchmarks
python benchmarks/run.py --output benchmarks/results.json

# Generate plots (requires matplotlib, numpy)
pip install matplotlib numpy
python benchmarks/plot.py --input benchmarks/results.json
```

## CI mode

For faster CI runs with smaller widths (max 16):

```bash
python benchmarks/run.py --ci --output benchmarks/results.json
```

## Output

- `results.json` -- raw benchmark data (JSON)
- `plots/gate_counts.png` -- gate count scaling
- `plots/circuit_depth.png` -- depth scaling
- `plots/timing.png` -- wall-clock generation time
