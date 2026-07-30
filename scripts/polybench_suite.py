"""PolyBench/C discovery and artifact preparation for YARDA benchmarks."""

from __future__ import annotations

import json
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path


UNSUPPORTED = {
    "symm": "dynamic triangular loop bound is emitted as zero",
    "syr2k": "dynamic triangular loop bounds are emitted as zero",
    "syrk": "dynamic triangular loop bounds are emitted as zero",
    "cholesky": "dynamic triangular loop bounds are emitted as zero",
    "durbin": "dynamic triangular loop bounds are emitted as zero",
    "lu": "dynamic triangular loop bounds are emitted as zero",
    "ludcmp": "dynamic triangular loop bounds are emitted as zero",
    "trisolv": "dynamic triangular loop bound is emitted as zero",
    "nussinov": "dynamic triangular loop bound is emitted as zero",
}
COMMON_DEFINES = (
    "-DPOLYBENCH_USE_SCALAR_LB",
    "-DPOLYBENCH_STACK_ARRAYS",
)
DATASETS = {"MINI", "SMALL", "MEDIUM", "LARGE", "EXTRALARGE"}


@dataclass(frozen=True)
class Workload:
    """One PolyBench source and its experiment classification."""

    name: str
    category: str
    source: Path
    supported: bool
    reason: str


@dataclass(frozen=True)
class PreparedWorkload:
    """Native and LAT artifacts prepared outside the timed region."""

    workload: Workload
    binary: Path
    lat: Path
    kernel: str


def category_for(relative_source: Path) -> str:
    """Map the PolyBench directory layout to presentation categories."""
    top = relative_source.parts[0]
    if top == "datamining":
        return "Data mining"
    if top == "linear-algebra":
        if relative_source.parts[1] == "kernels":
            return "Linear algebra kernels"
        return "BLAS & solvers"
    if top == "medley":
        return "Medley"
    if top == "stencils":
        return "Stencils"
    raise ValueError(f"unknown PolyBench category: {relative_source}")


def discover_workloads(suite: Path) -> list[Workload]:
    """Read PolyBench's benchmark list and classify supported kernels."""
    workloads = []
    benchmark_list = suite / "utilities" / "benchmark_list"
    for line in benchmark_list.read_text().splitlines():
        relative = Path(line.removeprefix("./"))
        name = relative.stem
        reason = UNSUPPORTED.get(name, "")
        workloads.append(
            Workload(
                name=name,
                category=category_for(relative),
                source=(suite / relative).resolve(),
                supported=not reason,
                reason=reason,
            )
        )
    return workloads


def find_kernel_name(llvm_ir: str) -> str:
    """Return the sole `kernel_*` definition from textual LLVM IR."""
    definitions = re.findall(r"^define.*@([^ (]+)", llvm_ir, re.MULTILINE)
    kernels = [name for name in definitions if name.startswith("kernel_")]
    if len(kernels) != 1:
        raise ValueError(f"expected exactly one kernel function, found {kernels}")
    return kernels[0]


def canonicalize_array_names(module: dict) -> None:
    """Use stable object metadata names instead of indexed display names."""
    objects = module.get("metadata", {}).get("objects", {})

    def visit(nodes: list[dict]) -> None:
        for node in nodes:
            if node.get("type") == "Array":
                metadata = objects.get(node.get("object"), {})
                if metadata.get("name"):
                    node["name"] = metadata["name"]
            if isinstance(node.get("body"), list):
                visit(node["body"])

    for function in module.get("functions", []):
        visit(function.get("body", []))


def run(command: list[str], *, cwd: Path, timeout: int = 120) -> None:
    """Run an artifact preparation command and retain useful failure output."""
    subprocess.run(
        command,
        cwd=cwd,
        check=True,
        timeout=timeout,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def compile_flags(
    suite: Path, workload: Workload, dataset: str = "MINI"
) -> list[str]:
    """Return flags shared by native and LLVM PolyBench builds."""
    dataset = dataset.upper()
    if dataset not in DATASETS:
        raise ValueError(f"unsupported PolyBench dataset: {dataset}")
    return [
        f"-D{dataset}_DATASET",
        *COMMON_DEFINES,
        "-I",
        str(suite / "utilities"),
        "-I",
        str(workload.source.parent),
    ]


def prepare_workload(
    suite: Path,
    workload: Workload,
    plugin: Path,
    output: Path,
    dataset: str = "MINI",
) -> PreparedWorkload:
    """Build one dataset's native binary and canonical kernel-only LAT."""
    if not workload.supported:
        raise ValueError(f"{workload.name} is excluded: {workload.reason}")
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    full_ir = output / f"{workload.name}_full.ll"
    kernel_ir = output / f"{workload.name}_kernel.ll"
    clean_ir = output / f"{workload.name}_kernel_clean.ll"
    binary = output / workload.name

    common = compile_flags(suite, workload, dataset)
    run(
        [
            "clang-14",
            "-O0",
            "-Xclang",
            "-disable-O0-optnone",
            "-g",
            *common,
            "-emit-llvm",
            "-S",
            str(workload.source),
            "-o",
            str(full_ir),
        ],
        cwd=output,
    )
    kernel = find_kernel_name(full_ir.read_text())
    run(
        [
            "llvm-extract-14",
            f"--func={kernel}",
            "-S",
            str(full_ir),
            "-o",
            str(kernel_ir),
        ],
        cwd=output,
    )
    run(
        [
            "opt-14",
            "-passes=globaldce",
            str(kernel_ir),
            "-S",
            "-o",
            str(clean_ir),
        ],
        cwd=output,
    )
    run(
        [
            "opt-14",
            f"-load-pass-plugin={plugin}",
            "-passes=function(mem2reg),loop-simplify,loop-annotated-trace",
            str(clean_ir),
            "-o",
            "/dev/null",
        ],
        cwd=output,
    )

    raw_lat = output / f"{clean_ir.stem}_ape.json"
    lat = output / f"{workload.name}_ape.json"
    module = json.loads(raw_lat.read_text())
    canonicalize_array_names(module)
    lat.write_text(json.dumps(module, indent=2) + "\n")

    run(
        [
            "clang-14",
            "-O0",
            *common,
            str(suite / "utilities" / "polybench.c"),
            str(workload.source),
            "-lm",
            "-o",
            str(binary),
        ],
        cwd=output,
    )
    return PreparedWorkload(workload, binary, lat, kernel)
