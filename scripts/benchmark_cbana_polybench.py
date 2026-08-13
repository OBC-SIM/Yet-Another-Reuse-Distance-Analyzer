#!/usr/bin/env python3
"""Benchmark the currently supported PolyBench rows from CBANA Table IV."""

from __future__ import annotations

import argparse
import csv
import json
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from polybench_suite import PreparedWorkload, discover_workloads, prepare_workload


ROOT = Path(__file__).resolve().parent.parent
CPP_BACKEND = ROOT / "build" / "backend" / "yarda_cpp"
CACHE_CONFIG = ROOT / "backend" / "config" / "cache.example.yaml"
RESULTS = ROOT / "benchmark-results" / "cbana-polybench-supported"
TOOLS = ("Cachegrind", "YARDA (Python)", "YARDA (C++)")
CACHE_LINE_SIZE = 64
CACHEGRIND_CACHE = "--D1=32768,8,64"


@dataclass(frozen=True)
class Case:
    """One dataset-qualified row from the CBANA comparison table."""

    label: str
    workload: str
    dataset: str
    paper_valgrind_seconds: float


CASES = (
    Case("jacobi_1d_medium", "jacobi-1d", "MEDIUM", 4.82),
    Case("jacobi_1d_small", "jacobi-1d", "SMALL", 1.38),
    Case("mvt_mini", "mvt", "MINI", 0.74),
    Case("mvt_small", "mvt", "SMALL", 3.48),
    Case("seidel_2d_mini", "seidel-2d", "MINI", 4.09),
)
EXCLUDED = (
    ("durbin_mini", "dynamic loop bound is not represented by the current LAT"),
    ("lu_mini", "dynamic loop bounds are not represented by the current LAT"),
    ("trisolv_small", "dynamic loop bound is not represented by the current LAT"),
)


def run_quiet(command: list[str], timeout: int = 600) -> None:
    """Run a benchmark command while suppressing tool diagnostics."""
    subprocess.run(
        command,
        cwd=ROOT,
        check=True,
        timeout=timeout,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def python_command(lat: Path) -> list[str]:
    """Build Python YARDA's exact 64-byte cache-line command."""
    source = (
        "import json,sys;"
        f"sys.path.insert(0,{str(ROOT / 'backend')!r});"
        "from block_trace import block_trace_results;"
        "from lru_sim import LRUProfiler;"
        "raw=json.load(open(sys.argv[1]));trace=[];"
        "[trace.extend(item[2]) for item in "
        "block_trace_results(raw,'cache-line',64)];"
        "LRUProfiler.calculate(trace)"
    )
    return [sys.executable, "-c", source, str(lat)]


def cpp_command(lat: Path) -> list[str]:
    """Build C++ YARDA's exact 64-byte cache-line command."""
    return [
        str(CPP_BACKEND),
        str(lat),
        "--mode",
        "unroll",
        "--granularity",
        "cache-line",
        "--cache",
        str(CACHE_CONFIG),
    ]


def cachegrind_command(case: Case, artifact: PreparedWorkload) -> list[str]:
    """Build Cachegrind's matching L1 data-cache command."""
    output = RESULTS / "cachegrind" / case.label
    output.parent.mkdir(parents=True, exist_ok=True)
    return [
        "valgrind",
        "--tool=cachegrind",
        CACHEGRIND_CACHE,
        f"--cachegrind-out-file={output}",
        str(artifact.binary),
    ]


def measure(command: list[str], repetitions: int) -> list[float]:
    """Warm up once and return wall-clock subprocess samples."""
    run_quiet(command)
    samples = []
    for _ in range(repetitions):
        started = time.perf_counter()
        run_quiet(command)
        samples.append(time.perf_counter() - started)
    return samples


def python_profile(lat: Path) -> tuple[dict[int, int], int, int]:
    """Return Python YARDA's reuse histogram, cold count, and trace length."""
    sys.path.insert(0, str(ROOT / "backend"))
    from block_trace import block_trace_results
    from lru_sim import LRUProfiler

    raw = json.loads(lat.read_text())
    trace: list[str] = []
    for _, _, block in block_trace_results(raw, "cache-line", CACHE_LINE_SIZE):
        trace.extend(block)
    profile = LRUProfiler.calculate(trace)
    return profile.histogram, len(profile.cold_misses), len(trace)


def verify_parity(case: Case, artifact: PreparedWorkload) -> tuple[int, int]:
    """Require exact Python/C++ reuse-distance parity before timing."""
    histogram, cold, accesses = python_profile(artifact.lat)
    export = artifact.lat.with_name(f"{case.label}_cpp.json")
    run_quiet([*cpp_command(artifact.lat), "--export", str(export)])
    program = json.loads(export.read_text())["program"]
    cpp_histogram = {
        int(distance): frequency
        for distance, frequency in program["histogram"].items()
    }
    if cpp_histogram != histogram or program["cold_misses"] != cold:
        raise RuntimeError(f"{case.label}: Python/C++ RDH parity mismatch")
    return accesses, cold


def write_csv(path: Path, rows: list[dict], fields: tuple[str, ...]) -> None:
    """Write dictionaries using deterministic column ordering."""
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def benchmark(
    suite: Path, plugin: Path, repetitions: int
) -> tuple[list[dict], list[dict]]:
    """Prepare, validate, and measure the five supported table rows."""
    discovered = {item.name: item for item in discover_workloads(suite)}
    runtime_rows: list[dict] = []
    summary_rows: list[dict] = []
    for case in CASES:
        generated = RESULTS / "generated" / case.label
        artifact = prepare_workload(
            suite, discovered[case.workload], plugin, generated, case.dataset
        )
        accesses, cold = verify_parity(case, artifact)
        commands = {
            "Cachegrind": cachegrind_command(case, artifact),
            "YARDA (Python)": python_command(artifact.lat),
            "YARDA (C++)": cpp_command(artifact.lat),
        }
        medians = {}
        for tool in TOOLS:
            samples = measure(commands[tool], repetitions)
            medians[tool] = statistics.median(samples)
            print(
                f"{case.label:>20}  {tool:<16}  {medians[tool]:.6f} s",
                flush=True,
            )
            for repetition, seconds in enumerate(samples, start=1):
                runtime_rows.append(
                    {
                        "workload": case.label,
                        "tool": tool,
                        "repetition": repetition,
                        "seconds": f"{seconds:.9f}",
                    }
                )
        summary_rows.append(
            {
                "workload": case.label,
                "dataset": case.dataset,
                "accesses": accesses,
                "cold_lines": cold,
                "cachegrind_seconds": f"{medians['Cachegrind']:.9f}",
                "python_seconds": f"{medians['YARDA (Python)']:.9f}",
                "cpp_seconds": f"{medians['YARDA (C++)']:.9f}",
                "python_to_cpp": f"{medians['YARDA (Python)'] / medians['YARDA (C++)']:.3f}",
                "cpp_to_cachegrind": f"{medians['Cachegrind'] / medians['YARDA (C++)']:.3f}",
                "paper_valgrind_seconds": case.paper_valgrind_seconds,
            }
        )
    return runtime_rows, summary_rows


def plot(summary: list[dict], path: Path, repetitions: int) -> None:
    """Render a presentation-style grouped runtime chart."""
    labels = [row["workload"] for row in summary]
    values = {
        "Cachegrind": [float(row["cachegrind_seconds"]) for row in summary],
        "YARDA (Python)": [float(row["python_seconds"]) for row in summary],
        "YARDA (C++)": [float(row["cpp_seconds"]) for row in summary],
    }
    colors = ("#dd8452", "#8172b3", "#4c72b0")
    x = np.arange(len(labels))
    width = 0.24
    plt.rcParams.update({"font.family": "serif", "font.size": 17})
    figure, axis = plt.subplots(figsize=(18, 8))
    for offset, tool, color in zip((-width, 0, width), TOOLS, colors):
        axis.bar(
            x + offset,
            values[tool],
            width,
            label=tool,
            color=color,
            edgecolor="black",
            linewidth=1.1,
        )
    for index, row in enumerate(summary):
        peak = max(values[tool][index] for tool in TOOLS)
        speedup = float(row["python_to_cpp"])
        axis.text(index, peak * 1.16, f"Py/C++ {speedup:.1f}×",
                  ha="center", va="bottom", fontsize=13)
    axis.set_yscale("log")
    all_values = [value for tool in TOOLS for value in values[tool]]
    axis.set_ylim(min(all_values) * 0.65, max(all_values) * 2.1)
    axis.set_ylabel("Analysis time (s)")
    display_labels = [
        f"{case.workload}\n{case.dataset}" for case in CASES
    ]
    axis.set_xticks(x, display_labels)
    axis.tick_params(axis="x", rotation=18)
    axis.grid(axis="y", which="both", linestyle=":", alpha=0.3)
    axis.set_axisbelow(True)
    handles, legend_labels = axis.get_legend_handles_labels()
    figure.legend(handles, legend_labels, loc="upper center", ncol=3,
                  frameon=False, bbox_to_anchor=(0.5, 0.995))
    figure.suptitle(
        "Cachegrind D1: 32 KB, 8-way, 64 B  ·  YARDA: 64 B line unroll  ·  "
        f"median of {repetitions} runs after warm-up  ·  "
        "durbin / lu / trisolv excluded",
        y=0.91,
        fontsize=15,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.86))
    figure.savefig(path, dpi=220, bbox_inches="tight")
    plt.close(figure)


def main() -> None:
    """Run the supported CBANA-table experiment and save data plus figure."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--suite", type=Path, default=Path("/workspace/PolyBenchC-4.2.1")
    )
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=5)
    args = parser.parse_args()
    if not CPP_BACKEND.is_file() or not args.plugin.is_file():
        raise FileNotFoundError("C++ backend and frontend plugin must be built")
    RESULTS.mkdir(parents=True, exist_ok=True)
    runtime, summary = benchmark(
        args.suite.resolve(), args.plugin.resolve(), args.repetitions
    )
    write_csv(
        RESULTS / "runtime.csv",
        runtime,
        ("workload", "tool", "repetition", "seconds"),
    )
    write_csv(
        RESULTS / "summary.csv",
        summary,
        (
            "workload", "dataset", "accesses", "cold_lines",
            "cachegrind_seconds", "python_seconds", "cpp_seconds",
            "python_to_cpp", "cpp_to_cachegrind", "paper_valgrind_seconds",
        ),
    )
    exclusions = [{"workload": name, "reason": reason} for name, reason in EXCLUDED]
    write_csv(RESULTS / "excluded.csv", exclusions, ("workload", "reason"))
    figure = RESULTS / "figures" / "runtime_comparison_presentation.png"
    figure.parent.mkdir(exist_ok=True)
    plot(summary, figure, args.repetitions)


if __name__ == "__main__":
    main()
