#!/usr/bin/env python3
"""Benchmark Cachegrind and both YARDA backends on PolyBench/C MINI."""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from polybench_suite import PreparedWorkload, discover_workloads, prepare_workload


ROOT = Path(__file__).resolve().parent.parent
CPP_BACKEND = ROOT / "build" / "backend" / "yarda_cpp"
RESULTS = ROOT / "benchmark-results" / "polybench-4.2.1-mini"
TOOLS = ("Cachegrind", "YARDA (Python)", "YARDA (C++)")
CATEGORIES = ("Data mining", "Linear algebra kernels", "BLAS & solvers",
              "Medley", "Stencils")
CACHE = ("--D1=32768,4,32", "--LL=2097152,4,32")


def run_quiet(command: list[str], timeout: int = 300) -> None:
    """Run a measured command without contaminating benchmark output."""
    subprocess.run(command, cwd=ROOT, check=True, timeout=timeout,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def python_command(lat: Path) -> list[str]:
    """Return Python YARDA's exact cache-line analysis command."""
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
    """Return C++ YARDA's exact cache-line analysis command."""
    return [str(CPP_BACKEND), str(lat), "--mode", "unroll",
            "--granularity", "cache-line", "--cache-line-size", "32"]


def cachegrind_command(artifact: PreparedWorkload) -> list[str]:
    """Return Cachegrind's 32-byte cache-line command."""
    output = RESULTS / "cachegrind" / artifact.workload.name
    output.parent.mkdir(parents=True, exist_ok=True)
    return ["valgrind", "--tool=cachegrind", *CACHE,
            f"--cachegrind-out-file={output}", str(artifact.binary)]


def measure(command: list[str], repetitions: int) -> list[float]:
    """Run one warm-up and return timed subprocess samples."""
    run_quiet(command)
    samples = []
    for _ in range(repetitions):
        started = time.perf_counter()
        run_quiet(command)
        samples.append(time.perf_counter() - started)
    return samples


def python_profile(lat: Path) -> tuple[dict[int, int], int, int]:
    """Calculate Python's reference histogram, cold count, and trace length."""
    sys.path.insert(0, str(ROOT / "backend"))
    from block_trace import block_trace_results
    from lru_sim import LRUProfiler

    raw = json.loads(lat.read_text())
    trace = []
    for _, _, block in block_trace_results(raw, "cache-line", 32):
        trace.extend(block)
    profile = LRUProfiler.calculate(trace)
    return profile.histogram, len(profile.cold_misses), len(trace)


def verify_parity(artifact: PreparedWorkload) -> tuple[int, int]:
    """Reject a workload unless C++ matches Python's exact profile."""
    histogram, cold, accesses = python_profile(artifact.lat)
    export = artifact.lat.with_name(f"{artifact.workload.name}_cpp.json")
    run_quiet([*cpp_command(artifact.lat), "--export", str(export)])
    program = json.loads(export.read_text())["program"]
    cpp_histogram = {int(key): value for key, value in program["histogram"].items()}
    if cpp_histogram != histogram or program["cold_misses"] != cold:
        raise RuntimeError("Python/C++ RDH parity mismatch")
    return accesses, cold


def error_text(error: Exception) -> str:
    """Flatten an experiment failure into one CSV-safe line."""
    return " ".join(str(error).split())[:500]


def benchmark(
    suite: Path, plugin: Path, repetitions: int
) -> tuple[list[dict[str, str]], list[dict[str, str]]]:
    """Prepare, verify, and measure every supported workload."""
    runtime_rows = []
    support_rows = []
    generated = RESULTS / "generated"
    for workload in discover_workloads(suite):
        if not workload.supported:
            support_rows.append(
                {
                    "workload": workload.name,
                    "category": workload.category,
                    "status": "excluded",
                    "reason": workload.reason,
                    "accesses": "",
                    "cold_lines": "",
                }
            )
            continue
        try:
            artifact = prepare_workload(suite, workload, plugin, generated)
            accesses, cold = verify_parity(artifact)
            commands = {
                "Cachegrind": cachegrind_command(artifact),
                "YARDA (Python)": python_command(artifact.lat),
                "YARDA (C++)": cpp_command(artifact.lat),
            }
            for tool in TOOLS:
                samples = measure(commands[tool], repetitions)
                print(f"{workload.name:>15}  {tool:<16}  "
                      f"{statistics.median(samples):.6f} s", flush=True)
                for repetition, seconds in enumerate(samples, start=1):
                    runtime_rows.append(
                        {
                            "workload": workload.name,
                            "category": workload.category,
                            "tool": tool,
                            "repetition": str(repetition),
                            "seconds": f"{seconds:.9f}",
                        }
                    )
            support_rows.append(
                {
                    "workload": workload.name,
                    "category": workload.category,
                    "status": "measured",
                    "reason": "",
                    "accesses": str(accesses),
                    "cold_lines": str(cold),
                }
            )
        except Exception as error:
            print(f"{workload.name:>15}  FAILED  {error_text(error)}", flush=True)
            support_rows.append(
                {
                    "workload": workload.name,
                    "category": workload.category,
                    "status": "failed",
                    "reason": error_text(error),
                    "accesses": "",
                    "cold_lines": "",
                }
            )
    return runtime_rows, support_rows


def write_csv(
    path: Path, rows: list[dict[str, str]], fields: tuple[str, ...]
) -> None:
    """Write experiment records with stable column ordering."""
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def medians(rows: list[dict[str, str]]) -> dict[tuple[str, str], float]:
    """Aggregate raw repetitions by workload and tool."""
    groups: dict[tuple[str, str], list[float]] = defaultdict(list)
    for row in rows:
        groups[(row["workload"], row["tool"])].append(float(row["seconds"]))
    return {key: statistics.median(values) for key, values in groups.items()}


def plot(rows: list[dict[str, str]], path: Path, repetitions: int) -> None:
    """Draw a category-faceted presentation runtime comparison."""
    if not rows:
        raise RuntimeError("no successful workload measurements to plot")
    aggregate = medians(rows)
    category_names = {row["workload"]: row["category"] for row in rows}
    speedups = [
        aggregate[(name, "YARDA (Python)")] / aggregate[(name, "YARDA (C++)")]
        for name in category_names
    ]
    faster_than_cachegrind = sum(
        aggregate[(name, "YARDA (C++)")] < aggregate[(name, "Cachegrind")]
        for name in category_names
    )

    plt.rcParams.update({"font.family": "serif", "font.size": 14})
    figure, axes = plt.subplots(3, 2, figsize=(18, 14))
    colors = ("#dd8452", "#8172b3", "#4c72b0")
    for axis, category in zip(axes.flat, CATEGORIES):
        names = [name for name, current in category_names.items()
                 if current == category]
        x = np.arange(len(names))
        width = 0.24
        for offset, tool, color in zip((-width, 0, width), TOOLS, colors):
            values = [aggregate[(name, tool)] for name in names]
            axis.bar(x + offset, values, width, color=color,
                     edgecolor="black", linewidth=1.0, label=tool)
        for index, name in enumerate(names):
            python = aggregate[(name, "YARDA (Python)")]
            cpp = aggregate[(name, "YARDA (C++)")]
            peak = max(aggregate[(name, tool)] for tool in TOOLS)
            axis.text(index, peak * 1.13, f"{python / cpp:.1f}x",
                      ha="center", va="bottom", fontsize=10)
        values = [aggregate[(name, tool)] for name in names for tool in TOOLS]
        axis.set_yscale("log")
        axis.set_ylim(min(values) * 0.65, max(values) * 1.9)
        axis.set_title(category)
        axis.set_xticks(x, names, rotation=25, ha="right")
        axis.set_ylabel("Analysis time (s)")
        axis.grid(axis="y", which="both", linestyle=":", alpha=0.3)
        axis.set_axisbelow(True)

    summary = axes.flat[-1]
    summary.axis("off")
    geomean = math.exp(sum(math.log(value) for value in speedups) / len(speedups))
    summary.text(
        0.08,
        0.78,
        f"Coverage: {len(category_names)}/30 workloads\n"
        f"Python to C++ geomean speedup: {geomean:.2f}x\n"
        f"C++ faster than Cachegrind: {faster_than_cachegrind}/"
        f"{len(category_names)}\n\n"
        "MINI_DATASET, 32-byte cache line\n"
        f"Median of {repetitions} timed runs after warm-up",
        va="top",
        fontsize=17,
        linespacing=1.5,
    )
    handles, labels = axes.flat[0].get_legend_handles_labels()
    figure.legend(handles, labels, loc="upper center", ncol=3, frameon=False)
    figure.tight_layout(rect=(0, 0, 1, 0.96))
    figure.savefig(path, dpi=220, bbox_inches="tight")
    plt.close(figure)


def main() -> None:
    """Run the expanded experiment and produce CSV plus presentation PNG."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", type=Path,
                        default=Path("/workspace/PolyBenchC-4.2.1"))
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=5)
    args = parser.parse_args()
    if not CPP_BACKEND.is_file() or not args.plugin.is_file():
        raise FileNotFoundError("C++ backend and frontend plugin must be built")
    if args.repetitions < 1:
        raise ValueError("repetitions must be positive")

    RESULTS.mkdir(parents=True, exist_ok=True)
    runtime, support = benchmark(
        args.suite.resolve(), args.plugin.resolve(), args.repetitions)
    write_csv(
        RESULTS / "runtime.csv",
        runtime,
        ("workload", "category", "tool", "repetition", "seconds"),
    )
    write_csv(
        RESULTS / "support.csv",
        support,
        ("workload", "category", "status", "reason", "accesses", "cold_lines"),
    )
    figure = RESULTS / "figures" / "runtime_comparison_presentation.png"
    figure.parent.mkdir(exist_ok=True)
    plot(runtime, figure, args.repetitions)


if __name__ == "__main__":
    main()
