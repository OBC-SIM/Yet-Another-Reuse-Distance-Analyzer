#!/usr/bin/env python3
"""Compare Cachegrind with the Python and C++ YARDA backends."""

from __future__ import annotations

import argparse
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
CPP_BACKEND = ROOT / "build" / "backend" / "yarda_cpp"
CACHE_CONFIG = ROOT / "backend" / "config" / "cache.32b.yaml"
RESULTS = ROOT / "benchmark-results" / "cpp-backend-comparison"
WORKLOADS = ("polybench_2mm", "polybench_atax", "polybench_correlation",
             "polybench_gemm", "polybench_jacobi")
TOOLS = ("Cachegrind", "YARDA (Python)", "YARDA (C++)")
CACHE = ("--D1=32768,4,32", "--LL=2097152,4,32")


def run(command: list[str], *, cwd: Path = ROOT) -> None:
    """Run a required command while suppressing benchmark diagnostics."""
    subprocess.run(command, cwd=cwd, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)


def prepare(plugin: Path, workload: str) -> tuple[Path, Path]:
    """Build one native executable and matching LAT outside timed regions."""
    generated = RESULTS / "generated"
    generated.mkdir(parents=True, exist_ok=True)
    source = TASKS / f"{workload}.c"
    binary = generated / workload
    llvm_ir = generated / f"{workload}_g.ll"
    lat = generated / f"{workload}_g_ape.json"
    run(["clang-14", "-O0", "-g", "-I", str(TASKS), str(source), "-lm",
         "-o", str(binary)])
    run(["clang-14", "-O0", "-Xclang", "-disable-O0-optnone", "-g",
         "-emit-llvm", "-S", "-o", str(llvm_ir), str(source)])
    run(["opt-14", f"-load-pass-plugin={plugin}",
         "-passes=function(mem2reg),loop-simplify,loop-annotated-trace",
         str(llvm_ir), "-o", "/dev/null"], cwd=generated)
    if not lat.is_file():
        raise FileNotFoundError(f"LAT was not created: {lat}")
    return binary, lat


def python_command(lat: Path) -> list[str]:
    """Return Python YARDA's exact 32-byte cache-line unroll command."""
    source = (
        "import json,sys;"
        f"sys.path.insert(0,{str(ROOT / 'backend')!r});"
        "from block_trace import block_trace_results;"
        "from lru_sim import LRUProfiler;"
        "raw=json.load(open(sys.argv[1]));"
        "trace=[];"
        "[trace.extend(block[2]) for block in "
        "block_trace_results(raw,'cache-line',32)];"
        "LRUProfiler.calculate(trace)"
    )
    return [sys.executable, "-c", source, str(lat)]


def cpp_command(lat: Path) -> list[str]:
    """Return C++ YARDA's exact 32-byte cache-line unroll command."""
    return [str(CPP_BACKEND), str(lat), "--mode", "unroll",
            "--granularity", "cache-line", "--cache", str(CACHE_CONFIG)]


def cachegrind_command(binary: Path, workload: str) -> list[str]:
    """Return Cachegrind's command using the same 32-byte cache-line size."""
    output = RESULTS / "cachegrind" / workload
    output.parent.mkdir(parents=True, exist_ok=True)
    return ["valgrind", "--tool=cachegrind", *CACHE,
            f"--cachegrind-out-file={output}", str(binary)]


def measure(command: list[str], repetitions: int) -> list[float]:
    """Run one warm-up followed by timed subprocess invocations."""
    run(command)
    samples = []
    for _ in range(repetitions):
        started = time.perf_counter()
        run(command)
        samples.append(time.perf_counter() - started)
    return samples


def benchmark(plugin: Path, repetitions: int) -> list[dict[str, str]]:
    """Measure all tools and return one row per timed repetition."""
    rows = []
    for workload in WORKLOADS:
        binary, lat = prepare(plugin, workload)
        commands = {
            "Cachegrind": cachegrind_command(binary, workload),
            "YARDA (Python)": python_command(lat),
            "YARDA (C++)": cpp_command(lat),
        }
        for tool in TOOLS:
            samples = measure(commands[tool], repetitions)
            for repetition, seconds in enumerate(samples, start=1):
                rows.append({
                    "workload": workload.removeprefix("polybench_"),
                    "tool": tool,
                    "repetition": str(repetition),
                    "seconds": f"{seconds:.9f}",
                })
            print(f"{workload.removeprefix('polybench_'):>11}  {tool:<16}  "
                  f"{statistics.median(samples):.6f} s", flush=True)
    return rows


def save_rows(rows: list[dict[str, str]], path: Path) -> None:
    """Write raw timing samples to CSV."""
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=("workload", "tool", "repetition", "seconds"))
        writer.writeheader()
        writer.writerows(rows)


def median(rows: list[dict[str, str]], workload: str, tool: str) -> float:
    """Return the median sample for one workload/tool pair."""
    return statistics.median(
        float(row["seconds"]) for row in rows
        if row["workload"] == workload and row["tool"] == tool)


def geometric_mean(values: list[float]) -> float:
    """Return the geometric mean of positive ratios."""
    return float(np.exp(np.mean(np.log(values))))


def plot(rows: list[dict[str, str]], path: Path) -> None:
    """Render a presentation-style grouped runtime comparison."""
    labels = [item.removeprefix("polybench_") for item in WORKLOADS]
    x = np.arange(len(labels))
    width = 0.23
    colors = ("#dd8452", "#8172b3", "#4c72b0")
    values = {
        tool: [median(rows, label, tool) for label in labels] for tool in TOOLS
    }
    speedups = [
        python / cpp
        for python, cpp in zip(values["YARDA (Python)"], values["YARDA (C++)"])
    ]

    plt.rcParams.update({"font.family": "serif", "font.size": 17})
    figure, axis = plt.subplots(figsize=(16, 6.5))
    for offset, tool, color in zip((-width, 0, width), TOOLS, colors):
        axis.bar(x + offset, values[tool], width, color=color,
                 edgecolor="black", linewidth=1.1, label=tool)
    for index, speedup in enumerate(speedups):
        peak = max(values[tool][index] for tool in TOOLS)
        axis.text(index, peak * 1.16, f"{speedup:.1f}×",
                  ha="center", va="bottom", fontsize=13)

    axis.text(0.985, 0.82,
              f"Python → C++ geomean speedup = "
              f"{geometric_mean(speedups):.1f}×",
              transform=axis.transAxes, ha="right", va="top", fontsize=15)
    axis.set_yscale("log")
    axis.set_ylim(0.04, 10)
    axis.set_ylabel("Analysis time (s)")
    axis.set_xticks(x, labels, rotation=28, ha="right")
    axis.legend(loc="upper center", bbox_to_anchor=(0.58, 0.995),
                ncol=3, frameon=False)
    axis.grid(axis="y", which="both", linestyle=":", alpha=0.35)
    axis.set_axisbelow(True)
    figure.tight_layout()
    figure.savefig(path, dpi=220, bbox_inches="tight")
    plt.close(figure)


def main() -> None:
    """Run the benchmark and save raw samples plus its figure."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=5)
    args = parser.parse_args()
    plugin = args.plugin.resolve()
    if not plugin.is_file():
        raise FileNotFoundError(f"frontend plugin not found: {plugin}")
    if not CPP_BACKEND.is_file():
        raise FileNotFoundError(f"C++ backend not built: {CPP_BACKEND}")
    if args.repetitions < 1:
        raise ValueError("repetitions must be positive")

    RESULTS.mkdir(parents=True, exist_ok=True)
    rows = benchmark(plugin, args.repetitions)
    save_rows(rows, RESULTS / "runtime.csv")
    figure_dir = RESULTS / "figures"
    figure_dir.mkdir(exist_ok=True)
    plot(rows, figure_dir / "runtime_comparison_presentation.png")


if __name__ == "__main__":
    main()
