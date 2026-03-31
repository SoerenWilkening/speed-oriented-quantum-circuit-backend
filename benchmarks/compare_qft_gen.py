"""Compare QFT sequence generation times: C backend vs monolith results.csv.

results.csv columns:
  - cq: generate QFT sequence + add to circuit (Python/Cython monolith)
  - cq_impr: generate QFT sequence + add to circuit (C backend, previous measurement)

bench_qft outputs:
  - gen: gate sequence computation only (count-only mode)
  - full: compute + insert into circuit data structure
"""

import csv
import ctypes
import os
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
RESULTS_CSV = REPO_ROOT / "Quantum_Assembly" / "circuit-gen-results" / "results.csv"
LIB_PATH = REPO_ROOT / "circuit-c-backend" / "build" / "libquantum.so"


def load_monolith_results():
    """Load cq and cq_impr times from results.csv."""
    cq = {}
    cq_impr = {}
    with open(RESULTS_CSV) as f:
        reader = csv.DictReader(f)
        for row in reader:
            n = int(row["n"])
            t = float(row["t"])
            meth = row["meth"]
            if meth == "cq":
                cq[n] = t
            elif meth == "cq_impr":
                cq_impr[n] = t
    return cq, cq_impr


def bench_qft_c(lib, n):
    """Build a QFT circuit of n qubits via ctypes and return (gen_s, full_s)."""
    import math

    # Phase 1: count-only mode (gen)
    ctx = lib.qc_circuit_create(n)
    lib.qc_circuit_set_simulate(ctx, 0)
    q = ctypes.c_uint32(0)
    for _ in range(n):
        lib.qc_qubit_alloc(ctx, ctypes.byref(q))

    t0 = time.perf_counter()
    for i in range(n):
        lib.qc_circuit_h(ctx, i)
        for j in range(i + 1, n):
            angle = math.pi / (1 << (j - i))
            lib.qc_circuit_cp(ctx, j, i, ctypes.c_double(angle))
    t1 = time.perf_counter()
    lib.qc_circuit_destroy(ctx)

    # Phase 2: full mode (gen + insert)
    ctx = lib.qc_circuit_create(n)
    lib.qc_circuit_set_simulate(ctx, 1)
    for _ in range(n):
        lib.qc_qubit_alloc(ctx, ctypes.byref(q))

    t2 = time.perf_counter()
    for i in range(n):
        lib.qc_circuit_h(ctx, i)
        for j in range(i + 1, n):
            angle = math.pi / (1 << (j - i))
            lib.qc_circuit_cp(ctx, j, i, ctypes.c_double(angle))
    t3 = time.perf_counter()
    lib.qc_circuit_destroy(ctx)

    return t1 - t0, t3 - t2


def main():
    if not RESULTS_CSV.exists():
        sys.exit(f"results.csv not found at {RESULTS_CSV}")
    if not LIB_PATH.exists():
        sys.exit(f"libquantum.so not found at {LIB_PATH}")

    lib = ctypes.CDLL(str(LIB_PATH))
    lib.qc_circuit_create.restype = ctypes.c_void_p
    lib.qc_circuit_create.argtypes = [ctypes.c_uint32]
    lib.qc_circuit_destroy.restype = None
    lib.qc_circuit_destroy.argtypes = [ctypes.c_void_p]
    lib.qc_circuit_set_simulate.restype = ctypes.c_int
    lib.qc_circuit_set_simulate.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.qc_qubit_alloc.restype = ctypes.c_int
    lib.qc_qubit_alloc.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
    lib.qc_circuit_h.restype = ctypes.c_int
    lib.qc_circuit_h.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    lib.qc_circuit_cp.restype = ctypes.c_int
    lib.qc_circuit_cp.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_double]

    cq, cq_impr = load_monolith_results()
    widths = sorted(cq.keys())

    print(f"QFT Sequence Generation: C backend (now) vs Monolith (results.csv)")
    print(f"===================================================================")
    print(f"  cq       = monolith Python/Cython (gen + insert)")
    print(f"  cq_impr  = monolith C backend measurement")
    print(f"  C full   = current C backend (gen + insert)")
    print(f"  C gen    = current C backend (gen only, no circuit insertion)")
    print()
    print(f"{'n':>6}  {'cq (s)':>12}  {'cq_impr (s)':>12}  {'C full (s)':>12}  {'C gen (s)':>12}  {'full vs cq':>10}  {'gen vs impr':>11}")
    print("-" * 95)

    for w in widths:
        gen_s, full_s = bench_qft_c(lib, w)
        t_cq = cq[w]
        t_impr = cq_impr.get(w, 0)

        speedup_cq = t_cq / full_s if full_s > 0 else float("inf")
        ratio_impr = t_impr / gen_s if gen_s > 0 and t_impr else float("inf")

        print(f"{w:>6}  {t_cq:>12.6f}  {t_impr:>12.6f}  {full_s:>12.6f}  {gen_s:>12.6f}  {speedup_cq:>8.1f}x  {ratio_impr:>9.1f}x")


if __name__ == "__main__":
    main()
