#!/usr/bin/env python3
"""Benchmark YARDA cache-line unrolling against Cachegrind.

Prepared artifacts are intentionally outside the timed region: Cachegrind
receives a native binary and YARDA receives a Loop Annotated Trace (LAT).
This measures each tool's analysis cost rather than compilation cost.
"""

from __future__ import annotations

import csv
import json
import statistics
import subprocess
import sys
import time
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parent.parent
TASKS = ROOT / "tasks"
PLUGIN = ROOT / "build" / "libLoopAnnotatedTrace.so"
RESULTS = ROOT / "benchmark-results"
WORKLOADS = ("polybench_2mm", "polybench_atax", "polybench_correlation",
             "polybench_gemm", "polybench_jacobi")
CACHE = ("--D1=32768,4,32", "--LL=2097152,4,32")


def run(command: list[str], *, cwd: Path = ROOT) -> None:
    """Run a required command without contaminating benchmark output."""
    subprocess.run(command, cwd=cwd, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)


def prepare(workload: str, binary_dir: Path) -> Path:
    """Create the matching native binary and LAT before timed repetitions."""
    source = TASKS / f"{workload}.c"
    ll_path = TASKS / f"{workload}_g.ll"
    lat_path = TASKS / f"{workload}_g_ape.json"
    binary = binary_dir / workload
    run(["clang-14", "-O0", "-g", "-I", str(TASKS), str(source), "-lm",
         "-o", str(binary)])
    run(["clang-14", "-O0", "-Xclang", "-disable-O0-optnone", "-g",
         "-emit-llvm", "-S", "-o", str(ll_path), str(source)])
    run(["opt-14", f"-load-pass-plugin={PLUGIN}",
         "-passes=function(mem2reg),loop-simplify,loop-annotated-trace",
         str(ll_path), "-o", "/dev/null"], cwd=TASKS)
    if not lat_path.is_file():
        raise FileNotFoundError(f"YARDA LAT was not created: {lat_path}")
    return binary


def yarda_command(lat_path: Path) -> list[str]:
    """Return a command that runs YARDA's cache-line actual-unroll analysis."""
    source = (
        "import json,sys;"
        f"sys.path.insert(0,{str(ROOT / 'backend')!r});"
        "from block_trace import block_trace_results;"
        "from lru_sim import LRUProfiler;"
        "raw=json.load(open(sys.argv[1]));"
        "trace=[];"
        "[trace.extend(part[2]) for part in block_trace_results(raw,'cache-line',32)];"
        "LRUProfiler.calculate(trace)"
    )
    return [sys.executable, "-c", source, str(lat_path)]


def measure(command: list[str], repetitions: int, *, cwd: Path = ROOT) -> list[float]:
    """Run one warm-up followed by timed repetitions in seconds."""
    run(command, cwd=cwd)
    samples = []
    for _ in range(repetitions):
        started = time.perf_counter()
        run(command, cwd=cwd)
        samples.append(time.perf_counter() - started)
    return samples


def benchmark(repetitions: int) -> list[dict[str, str]]:
    """Measure all workloads and return one CSV row per tool repetition."""
    binary_dir = RESULTS / "bin"
    raw_dir = RESULTS / "cachegrind"
    binary_dir.mkdir(parents=True, exist_ok=True)
    raw_dir.mkdir(parents=True, exist_ok=True)
    rows = []
    for workload in WORKLOADS:
        binary = prepare(workload, binary_dir)
        lat_path = TASKS / f"{workload}_g_ape.json"
        commands = {
            "YARDA cache-line unroll": yarda_command(lat_path),
            "Cachegrind": ["valgrind", "--tool=cachegrind", *CACHE,
                            f"--cachegrind-out-file={raw_dir / workload}", str(binary)],
        }
        for tool, command in commands.items():
            for repetition, seconds in enumerate(measure(command, repetitions), start=1):
                rows.append({"workload": workload.removeprefix("polybench_"),
                             "tool": tool, "repetition": str(repetition),
                             "seconds": f"{seconds:.9f}"})
    return rows


def save_rows(rows: list[dict[str, str]], path: Path) -> None:
    """Write raw repetitions in a portable CSV format."""
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=("workload", "tool", "repetition", "seconds"))
        writer.writeheader()
        writer.writerows(rows)


def plot(rows: list[dict[str, str]], path: Path) -> None:
    """Render the presentation-style grouped runtime comparison figure."""
    median = {(workload, tool): statistics.median(
        float(row["seconds"]) for row in rows
        if row["workload"] == workload.removeprefix("polybench_") and row["tool"] == tool
    ) for workload in WORKLOADS for tool in ("Cachegrind", "YARDA cache-line unroll")}
    labels = [workload.removeprefix("polybench_") for workload in WORKLOADS]
    x = np.arange(len(labels))
    width = 0.31
    plt.rcParams.update({"font.family": "serif", "font.size": 16})
    figure, axis = plt.subplots(figsize=(16, 6.5))
    cachegrind = [median[(workload, "Cachegrind")] for workload in WORKLOADS]
    yarda = [median[(workload, "YARDA cache-line unroll")] for workload in WORKLOADS]
    axis.bar(x - width / 2, cachegrind, width, color="#dd8452", edgecolor="black",
             linewidth=1.1, label="Cachegrind")
    axis.bar(x + width / 2, yarda, width, color="#4c72b0", edgecolor="black",
             linewidth=1.1, label="YARDA (cache-line unroll)")
    for index, (cachegrind_seconds, yarda_seconds) in enumerate(zip(cachegrind, yarda)):
        speedup = cachegrind_seconds / yarda_seconds
        axis.text(index, max(cachegrind_seconds, yarda_seconds) * 1.17,
                  f"{speedup:.2f}×", ha="center", va="bottom", fontsize=14)
    axis.set_yscale("log")
    axis.set_ylabel("Analysis time (s)")
    axis.set_xticks(x, labels, rotation=28, ha="right")
    axis.legend(loc="upper right", frameon=False)
    axis.grid(axis="y", which="both", linestyle=":", alpha=0.35)
    axis.set_axisbelow(True)
    figure.tight_layout()
    figure.savefig(path, dpi=220, bbox_inches="tight")
    plt.close(figure)


def main() -> None:
    """Execute the benchmark and write raw samples plus a PNG/PDF figure."""
    repetitions = 5
    if not PLUGIN.is_file():
        raise FileNotFoundError(f"YARDA plugin was not built: {PLUGIN}")
    RESULTS.mkdir(exist_ok=True)
    rows = benchmark(repetitions)
    csv_path = RESULTS / "yarda_vs_cachegrind_runtime.csv"
    save_rows(rows, csv_path)
    figure_dir = RESULTS / "figures"
    figure_dir.mkdir(exist_ok=True)
    plot(rows, figure_dir / "yarda_vs_cachegrind_runtime.png")
    plot(rows, figure_dir / "yarda_vs_cachegrind_runtime.pdf")


if __name__ == "__main__":
    main()
