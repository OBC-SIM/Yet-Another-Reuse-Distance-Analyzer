#!/usr/bin/env python3
"""Measure shared frontend and backend time for YARDA and CASA."""

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
from matplotlib.patches import Patch

from benchmark_cbana_polybench import CASES, CPP_BACKEND, python_command
from polybench_suite import (
    canonicalize_array_names,
    compile_flags,
    discover_workloads,
    find_kernel_name,
    run,
)


ROOT = Path(__file__).resolve().parent.parent
SUITE = Path("/workspace/PolyBenchC-4.2.1")
FRONTEND = Path("/workspace/Yet-Another-Reuse-Distance-Analyzer/build/libLoopAnnotatedTrace.so")
CASA_BENCHMARK = Path("/workspace/CASA/build/casa_benchmark")
RESULTS = ROOT / "benchmark-results" / "cbana-polybench-e2e"
TOOLS = ("Cachegrind", "YARDA (Python)", "YARDA (C++)", "CASA")
COLORS = ("#dd8452", "#8172b3", "#4c72b0", "#55a868")
REPETITIONS = 5
CACHE_YAML = """cores:
  count: 1
  mapping:
    - id: 0
      l1: L1D0
caches:
  - name: L1D0
    role: L1
    private_to: 0
    size_bytes: 32 KiB
    line_size: 64 B
    associativity: 8
    replacement: LRU
    write_policy: write-back
    write_allocate: true
    delay_cycles: 4
    next: L2
  - name: L2
    role: LLC
    size_bytes: 2 MiB
    line_size: 64 B
    associativity: 8
    replacement: LRU
    write_policy: write-back
    write_allocate: true
    delay_cycles: 12
    next: Memory
memory:
  name: Memory
  delay_cycles: 120
"""


def command_time(command: list[str]) -> tuple[float, str]:
    """Return process wall time and standard output for one backend run."""
    started = time.perf_counter()
    completed = subprocess.run(
        command, check=True, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, cwd=ROOT,
    )
    return time.perf_counter() - started, completed.stdout


def frontend_time(workload, dataset: str, output: Path) -> tuple[float, float, Path]:
    """Measure compilation and YARDA's LLVM-IR-to-LAT work separately."""
    output.mkdir(parents=True, exist_ok=True)
    full_ir = output / "full.ll"
    kernel_ir = output / "kernel.ll"
    clean_ir = output / "kernel_clean.ll"
    lat = output / "kernel_ape.json"
    common = compile_flags(SUITE, workload, dataset)
    started = time.perf_counter()
    run([
        "clang-14", "-O0", "-Xclang", "-disable-O0-optnone", "-g", *common,
        "-emit-llvm", "-S", str(workload.source), "-o", str(full_ir),
    ], cwd=output)
    compilation = time.perf_counter() - started
    started = time.perf_counter()
    kernel = find_kernel_name(full_ir.read_text())
    run(["llvm-extract-14", f"--func={kernel}", "-S", str(full_ir), "-o", str(kernel_ir)], cwd=output)
    run(["opt-14", "-passes=globaldce", str(kernel_ir), "-S", "-o", str(clean_ir)], cwd=output)
    run([
        "opt-14", f"-load-pass-plugin={FRONTEND}",
        "-passes=function(mem2reg),loop-simplify,loop-annotated-trace",
        str(clean_ir), "-o", "/dev/null",
    ], cwd=output)
    raw = output / "kernel_clean_ape.json"
    module = json.loads(raw.read_text())
    canonicalize_array_names(module)
    for function in module["functions"]:
        annotations = function.setdefault("annotations", [])
        if "yard.analyze" not in annotations:
            annotations.append("yard.analyze")
    lat.write_text(json.dumps(module, indent=2) + "\n")
    return compilation, time.perf_counter() - started, lat


def casa_time(lat: Path, cache: Path) -> float:
    """Measure CASA process startup, LAT/YAML loading, and Pipeline::run."""
    seconds, stdout = command_time([
        str(CASA_BENCHMARK), str(lat), "--cache", str(cache),
        "--warmup", "0", "--repetitions", "1",
    ])
    sample = next(csv.DictReader(stdout.splitlines()))
    if int(sample["total_accesses"]) == 0:
        raise RuntimeError("CASA emitted no access events")
    return seconds


def native_binary(workload, dataset: str, output: Path) -> Path:
    """Compile Cachegrind's native binary outside every timed measurement."""
    output.parent.mkdir(parents=True, exist_ok=True)
    run([
        "clang-14", "-O0", *compile_flags(SUITE, workload, dataset),
        str(SUITE / "utilities" / "polybench.c"), str(workload.source),
        "-lm", "-o", str(output),
    ], cwd=output.parent)
    return output


def cachegrind_time(binary: Path, output: Path) -> float:
    """Measure Cachegrind under the matching 32 KiB/8-way/64 B D1 geometry."""
    output.parent.mkdir(parents=True, exist_ok=True)
    seconds, _ = command_time([
        "valgrind", "--tool=cachegrind", "--D1=32768,8,64",
        f"--cachegrind-out-file={output}", str(binary),
    ])
    return seconds


def backend_times(lat: Path, cache: Path, binary: Path, cachegrind: Path) -> dict[str, float]:
    """Measure Cachegrind and all static backends for one workload sample."""
    python_seconds, _ = command_time(python_command(lat))
    cpp_seconds, _ = command_time([
        str(CPP_BACKEND), str(lat), "--mode", "unroll", "--granularity",
        "cache-line", "--cache-line-size", "64",
    ])
    return {
        "Cachegrind": cachegrind_time(binary, cachegrind),
        "YARDA (Python)": python_seconds,
        "YARDA (C++)": cpp_seconds,
        "CASA": casa_time(lat, cache),
    }


def warmup(workload, dataset: str, cache: Path, label: str, binary: Path) -> None:
    """Exercise frontend and all backends before recording measured samples."""
    _, _, lat = frontend_time(workload, dataset, RESULTS / "generated" / label / "warmup")
    backend_times(lat, cache, binary, RESULTS / "cachegrind" / label / "warmup")


def write_csv(path: Path, rows: list[dict], fields: tuple[str, ...]) -> None:
    """Write rows with deterministic columns."""
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def summarize(rows: list[dict]) -> list[dict]:
    """Calculate median frontend, backend, and total times per tool/workload."""
    grouped: dict[tuple[str, str], list[dict]] = {}
    for row in rows:
        grouped.setdefault((row["workload"], row["tool"]), []).append(row)
    result = []
    for (workload, tool), samples in grouped.items():
        compilation = statistics.median(float(row["compilation_seconds"]) for row in samples)
        lat_generation = statistics.median(float(row["lat_generation_seconds"]) for row in samples)
        frontend = compilation + lat_generation
        backend = statistics.median(float(row["backend_seconds"]) for row in samples)
        total = frontend + backend
        result.append({
            "workload": workload, "tool": tool,
            "compilation_seconds": f"{compilation:.9f}",
            "lat_generation_seconds": f"{lat_generation:.9f}",
            "frontend_seconds": f"{frontend:.9f}",
            "backend_seconds": f"{backend:.9f}",
            "total_seconds": f"{total:.9f}",
            "frontend_percent": f"{100 * frontend / total:.1f}",
        })
    return result


def plot(summary: list[dict]) -> None:
    """Draw compilation, LLVM-to-LAT, and backend portions of each runtime."""
    labels = [case.label for case in CASES]
    values = {(row["workload"], row["tool"]): row for row in summary}
    x = np.arange(len(labels))
    width = 0.18
    plt.rcParams.update({"font.family": "serif", "font.size": 14})
    figure, axis = plt.subplots(figsize=(18, 8))
    for offset, tool, color in zip((-1.5 * width, -0.5 * width, 0.5 * width, 1.5 * width), TOOLS, COLORS):
        compilation = [float(values[(label, tool)]["compilation_seconds"]) for label in labels]
        lat_generation = [float(values[(label, tool)]["lat_generation_seconds"]) for label in labels]
        backend = [float(values[(label, tool)]["backend_seconds"]) for label in labels]
        totals = [float(values[(label, tool)]["total_seconds"]) for label in labels]
        axis.bar(x + offset, compilation, width, color="#d9d9d9", hatch="///",
                 edgecolor="black", linewidth=1.0)
        axis.bar(x + offset, lat_generation, width, bottom=compilation, color="#a8a8a8",
                 hatch="...", edgecolor="black", linewidth=1.0)
        frontend = np.add(compilation, lat_generation)
        axis.bar(x + offset, backend, width, bottom=frontend, color=color,
                 edgecolor="black", linewidth=1.0, label=tool)
        for index, total in enumerate(totals):
            axis.text(index + offset, total * 1.02, f"{total:.2f}s",
                      ha="center", va="bottom", fontsize=9, rotation=90)
    axis.set_ylabel("End-to-end analysis time (s)")
    axis.set_xticks(x, [f"{case.workload}\n{case.dataset}" for case in CASES])
    axis.tick_params(axis="x", rotation=18)
    axis.grid(axis="y", linestyle=":", alpha=0.3)
    axis.set_axisbelow(True)
    handles = [
        Patch(facecolor="#d9d9d9", edgecolor="black", hatch="///", label="Shared C → LLVM IR compilation"),
        Patch(facecolor="#a8a8a8", edgecolor="black", hatch="...", label="Shared LLVM IR → LAT (YARDA additions)"),
    ]
    handles += [Patch(facecolor=color, edgecolor="black", label=tool) for tool, color in zip(TOOLS, COLORS)]
    figure.legend(handles=handles, loc="upper center", ncol=3, frameon=False,
                  bbox_to_anchor=(0.5, 0.995))
    figure.suptitle(
        "Shared frontend: clang compile + kernel extraction/DCE/LoopAnnotatedTrace/LAT serialization  ·  "
        "Cachegrind: native binary (no LLVM LAT frontend)  ·  "
        "YARDA/CASA: backend analysis after shared frontend",
        y=0.91, fontsize=14,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.86))
    output = RESULTS / "figures" / "e2e_runtime_breakdown_presentation.png"
    output.parent.mkdir(exist_ok=True)
    figure.savefig(output, dpi=220, bbox_inches="tight")
    plt.close(figure)


def main() -> None:
    """Execute warmups, five end-to-end samples, and the breakdown chart."""
    if not all(path.is_file() for path in (FRONTEND, CPP_BACKEND, CASA_BENCHMARK)):
        raise FileNotFoundError("frontend, YARDA C++, or CASA benchmark binary is missing")
    RESULTS.mkdir(parents=True, exist_ok=True)
    cache = RESULTS / "cache_32k_8way_64b.yaml"
    cache.write_text(CACHE_YAML)
    workloads = {item.name: item for item in discover_workloads(SUITE)}
    rows = []
    for case in CASES:
        workload = workloads[case.workload]
        binary = native_binary(workload, case.dataset, RESULTS / "native" / case.label)
        warmup(workload, case.dataset, cache, case.label, binary)
        for repetition in range(1, REPETITIONS + 1):
            compilation, lat_generation, lat = frontend_time(
                workload, case.dataset,
                RESULTS / "generated" / case.label / f"repetition-{repetition}",
            )
            backends = backend_times(
                lat, cache, binary,
                RESULTS / "cachegrind" / case.label / f"repetition-{repetition}",
            )
            for tool, backend in backends.items():
                compilation_component = 0.0 if tool == "Cachegrind" else compilation
                lat_component = 0.0 if tool == "Cachegrind" else lat_generation
                frontend_component = compilation_component + lat_component
                rows.append({
                    "workload": case.label, "tool": tool, "repetition": repetition,
                    "compilation_seconds": f"{compilation_component:.9f}",
                    "lat_generation_seconds": f"{lat_component:.9f}",
                    "frontend_seconds": f"{frontend_component:.9f}",
                    "backend_seconds": f"{backend:.9f}",
                    "total_seconds": f"{frontend_component + backend:.9f}",
                })
            print(f"{case.label}: repetition {repetition}/{REPETITIONS}", flush=True)
    write_csv(RESULTS / "runtime_breakdown.csv", rows, (
        "workload", "tool", "repetition", "compilation_seconds", "lat_generation_seconds", "frontend_seconds",
        "backend_seconds", "total_seconds",
    ))
    summary = summarize(rows)
    write_csv(RESULTS / "summary_breakdown.csv", summary, (
        "workload", "tool", "compilation_seconds", "lat_generation_seconds", "frontend_seconds", "backend_seconds",
        "total_seconds", "frontend_percent",
    ))
    plot(summary)


if __name__ == "__main__":
    main()
