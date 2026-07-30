#!/usr/bin/env python3
"""Add CASA Pipeline::run timing to the supported CBANA PolyBench chart."""

from __future__ import annotations

import csv
import json
import statistics
import subprocess
import time
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parent.parent
RESULTS = ROOT / "benchmark-results" / "cbana-polybench-supported"
CASA_ROOT = Path("/workspace/CASA")
CASA_BENCHMARK = CASA_ROOT / "build" / "casa_benchmark"
REPETITIONS = 5
TOOLS = ("Cachegrind", "YARDA (Python)", "YARDA (C++)", "CASA")
SOURCE_NAMES = {
    "jacobi_1d_medium": "jacobi-1d",
    "jacobi_1d_small": "jacobi-1d",
    "mvt_mini": "mvt",
    "mvt_small": "mvt",
    "seidel_2d_mini": "seidel-2d",
}
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


def read_csv(path: Path) -> list[dict[str, str]]:
    """Load a header-based experiment CSV."""
    with path.open(newline="") as source:
        return list(csv.DictReader(source))


def write_csv(path: Path, rows: list[dict[str, str]], fields: tuple[str, ...]) -> None:
    """Write experiment rows with a stable schema."""
    with path.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def prepare_casa_lat(label: str) -> Path:
    """Copy YARDA's LAT and mark its sole extracted kernel as CASA's root."""
    source = RESULTS / "generated" / label / f"{SOURCE_NAMES[label]}_ape.json"
    module = json.loads(source.read_text())
    for function in module["functions"]:
        annotations = function.setdefault("annotations", [])
        if "yard.analyze" not in annotations:
            annotations.append("yard.analyze")
    output = RESULTS / "casa-input" / f"{label}_ape.json"
    output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(module, indent=2) + "\n")
    return output


def casa_samples(label: str, cache: Path) -> list[dict[str, str]]:
    """Measure CASA process startup, LAT/YAML loading, and Pipeline::run."""
    lat = prepare_casa_lat(label)
    base = [str(CASA_BENCHMARK), str(lat), "--cache", str(cache)]
    subprocess.run(
        [*base, "--warmup", "1", "--repetitions", "1"], check=True,
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=CASA_ROOT,
    )
    rows = []
    for repetition in range(1, REPETITIONS + 1):
        started = time.perf_counter()
        completed = subprocess.run(
            [*base, "--warmup", "0", "--repetitions", "1"], check=True,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=CASA_ROOT,
        )
        sample = next(csv.DictReader(completed.stdout.splitlines()))
        if int(sample["total_accesses"]) == 0:
            raise RuntimeError(f"CASA emitted no access events for {label}")
        rows.append({
            "workload": label,
            "tool": "CASA",
            "repetition": str(repetition),
            "seconds": f"{time.perf_counter() - started:.9f}",
        })
    return rows


def medians(rows: list[dict[str, str]]) -> dict[tuple[str, str], float]:
    """Calculate per-workload median runtimes."""
    groups: dict[tuple[str, str], list[float]] = {}
    for row in rows:
        groups.setdefault((row["workload"], row["tool"]), []).append(float(row["seconds"]))
    return {key: statistics.median(samples) for key, samples in groups.items()}


def plot(summary: list[dict[str, str]], values: dict[tuple[str, str], float]) -> None:
    """Render the four-tool presentation chart."""
    labels = [row["workload"] for row in summary]
    x = np.arange(len(labels))
    width = 0.19
    colors = ("#dd8452", "#8172b3", "#4c72b0", "#55a868")
    plt.rcParams.update({"font.family": "serif", "font.size": 16})
    figure, axis = plt.subplots(figsize=(18, 8))
    for offset, tool, color in zip((-1.5 * width, -0.5 * width, 0.5 * width, 1.5 * width), TOOLS, colors):
        axis.bar(x + offset, [values[(label, tool)] for label in labels], width,
                 label=tool, color=color, edgecolor="black", linewidth=1.1)
    for index, row in enumerate(summary):
        peak = max(values[(row["workload"], tool)] for tool in TOOLS)
        axis.text(index, peak * 1.15, f"Py/C++ {float(row['python_to_cpp']):.1f}×",
                  ha="center", va="bottom", fontsize=12)
    all_values = list(values.values())
    axis.set_yscale("log")
    axis.set_ylim(min(all_values) * 0.6, max(all_values) * 2.3)
    axis.set_ylabel("Analysis time (s)")
    axis.set_xticks(x, [f"{SOURCE_NAMES[label]}\n{row['dataset']}" for label, row in zip(labels, summary)])
    axis.tick_params(axis="x", rotation=18)
    axis.grid(axis="y", which="both", linestyle=":", alpha=0.3)
    axis.set_axisbelow(True)
    handles, legend_labels = axis.get_legend_handles_labels()
    figure.legend(handles, legend_labels, loc="upper center", ncol=4,
                  frameon=False, bbox_to_anchor=(0.5, 0.995))
    figure.suptitle(
        "Cachegrind D1: 32 KiB, 8-way, 64 B  ·  YARDA: 64 B line unroll  ·  "
        "CASA: LAT/YAML load + Pipeline::run (report I/O excluded)",
        y=0.91, fontsize=14,
    )
    figure.tight_layout(rect=(0, 0, 1, 0.86))
    output = RESULTS / "figures" / "runtime_comparison_with_casa_presentation.png"
    figure.savefig(output, dpi=220, bbox_inches="tight")
    plt.close(figure)


def main() -> None:
    """Measure CASA and create a combined four-tool result set."""
    if not CASA_BENCHMARK.is_file():
        raise FileNotFoundError(f"CASA benchmark binary not found: {CASA_BENCHMARK}")
    cache = RESULTS / "casa_cbana_cache.yaml"
    cache.write_text(CACHE_YAML)
    summary = read_csv(RESULTS / "summary.csv")
    casa_rows = [row for item in summary for row in casa_samples(item["workload"], cache)]
    base_rows = read_csv(RESULTS / "runtime.csv")
    combined = base_rows + casa_rows
    write_csv(RESULTS / "runtime_with_casa.csv", combined,
              ("workload", "tool", "repetition", "seconds"))
    values = medians(combined)
    combined_summary = []
    for row in summary:
        label = row["workload"]
        combined_summary.append({**row, "casa_seconds": f"{values[(label, 'CASA')]:.9f}"})
    write_csv(RESULTS / "summary_with_casa.csv", combined_summary,
              tuple(summary[0]) + ("casa_seconds",))
    plot(combined_summary, values)


if __name__ == "__main__":
    main()
