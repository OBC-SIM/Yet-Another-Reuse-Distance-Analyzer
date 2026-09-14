"""Prepare real SPARC/RTEMS images and capture toolchain/source provenance."""
from pathlib import Path
import os
import platform
import shutil
import struct
import subprocess

from execution import checked, sha, write_json

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
RTEMS = Path("/opt/rtems/6")


def prepare(row, output, binaries):
    """Build twice from the same C source and require identical ELF/LAT bytes."""
    directory = output / "inputs" / row["case_id"]
    directory.mkdir(parents=True)
    defines = [f"-D{k.upper()}={row[k]}" for k in ("m", "n", "domain", "repeats")]
    make = ["make", "-C", HERE, f"KERNEL={row['kernel']}", f"DEFINES={' '.join(defines)}"]
    timing = checked(make + [f"OUT={directory}", "all", "native"], directory / "build")
    source = HERE / "kernels" / f"{row['kernel']}.c"
    frontend = [binaries["region"], source, directory / "input.json", "--",
                "--target=sparc-unknown-rtems6", *defines]
    timing_lat = checked(frontend, directory / "frontend")
    checked(make + [f"OUT={directory / 'rebuild'}", "all"], directory / "rebuild")
    rebuilt_lat = frontend.copy()
    rebuilt_lat[2] = directory / "rebuild" / "input.json"
    checked(rebuilt_lat, directory / "frontend-rebuild")
    for name in ("input.json", "input.elf"):
        if sha(directory / name) != sha(directory / "rebuild" / name):
            raise AssertionError(f"non-reproducible rebuild: {row['case_id']}/{name}")
    elf = directory / "input.elf"
    with elf.open("rb") as stream:
        header = stream.read(20)
    if header[:6] != b"\x7fELF\x01\x02" or struct.unpack(">HH", header[16:20]) != (2, 2):
        raise AssertionError("expected big-endian ELF32 ET_EXEC EM_SPARC")
    checked([RTEMS / "bin/sparc-rtems6-readelf", "-h", "-l", "-S", elf], directory / "readelf")
    checked([RTEMS / "bin/sparc-rtems6-objdump", "-d", "--disassemble=benchmark_kernel", elf],
            directory / "kernel-disassembly")
    checked([RTEMS / "bin/sparc-rtems6-nm", "-S", "--defined-only", elf], directory / "nm")
    symbols = {}
    for line in (directory / "nm.stdout").read_text().splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] in row["symbol_sizes"]:
            base, size = int(parts[0], 16), int(parts[1], 16)
            if base % 32 or size != row["symbol_sizes"][parts[3]] or base + size > 64 * 1024**2:
                raise AssertionError(f"unexpected linked object: {line}")
            symbols[parts[3]] = dict(base=base, size=size)
    if symbols.keys() != row["symbol_sizes"].keys():
        raise AssertionError("RTEMS ELF lost an analyzed object")
    row.update(symbols=symbols, preparation_in_timing=True, expected_status="success",
               compiler_pipeline="clang14-o0-region-v1/target=sparc-unknown-rtems6",
               gcc_pipeline="sparc-rtems6-gcc/GR740/-O0/-mcpu=leon3/-mfpu/-mhard-float",
               source_path=str(source), source_sha256=sha(source),
               preparation_wall_ns=timing["wall_time_ns"] + timing_lat["wall_time_ns"],
               defines=defines, rebuild_elf_identical=True, rebuild_lat_identical=True)
    for kind, path in (("lat", directory / "input.json"), ("elf", elf),
                       ("cache", output / "cache-model.yaml")):
        row[f"{kind}_path"], row[f"{kind}_sha256"] = str(path), sha(path)
    write_json(directory / "case.json", row)
    return directory / "case.json"


def provenance(output, build, binaries):
    """Archive source hashes, binary bytes, BSP libraries and host controls."""
    destination = output / "binaries"
    destination.mkdir()
    for name, path in binaries.items():
        shutil.copy2(path, destination / name)
    tracked = subprocess.check_output(["git", "ls-files", "backend", "CMakeLists.txt"],
                                      cwd=ROOT, text=True).splitlines()
    sources = {ROOT / p for p in tracked if (ROOT / p).is_file()}
    for base in (HERE, ROOT / "frontend/src", ROOT / "frontend/include", ROOT / "frontend/cmake"):
        sources.update(p for p in base.rglob("*") if p.is_file() and "__pycache__" not in p.parts)
    write_json(output / "sources.json", {str(p): sha(p) for p in sorted(sources)})
    shutil.copytree(HERE, output / "experiment-source", ignore=shutil.ignore_patterns("__pycache__"))
    for name in ("CMakeCache.txt", "compile_commands.json"):
        shutil.copy2(build / name, output / name)
    versions = {}
    for tool in ("sparc-rtems6-gcc", "sparc-rtems6-ld", "clang-14", "cmake", "c++"):
        checked([tool, "--version"], output / "provenance" / tool)
        versions[tool] = (output / "provenance" / f"{tool}.stdout").read_text().splitlines()[0]
    bsp = RTEMS / "sparc-rtems6/gr740/lib"
    library_paths = list(bsp.glob("*.a")) + list(bsp.glob("linkcmds*")) + list(bsp.glob("start*.o"))
    pc = RTEMS / "lib/pkgconfig/sparc-rtems6-gr740.pc"
    shutil.copy2(pc, output / pc.name)
    for library in ("libc.a", "libm.a", "libgcc.a"):
        library_paths.append(Path(subprocess.check_output(
            ["sparc-rtems6-gcc", "-mcpu=leon3", f"-print-file-name={library}"], text=True).strip()))
    original = Path("/workspace/PolyBenchC-4.2.1/linear-algebra/kernels")
    references = [original / kernel / f"{kernel}.{suffix}"
                  for kernel in ("atax", "bicg", "mvt") for suffix in ("c", "h")]
    result = dict(host=platform.uname()._asdict(), versions=versions,
                  analyzer_flags="-O2 -DNDEBUG", input_flags="-O0 -g",
                  cpu_affinity=sorted(os.sched_getaffinity(0)), governor="uncontrolled",
                  measurement_scope="host analysis of prepared SPARC/RTEMS input; not GR740 runtime",
                  source_head=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                  frontend_head=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT / "frontend", text=True).strip(),
                  binary_hashes={str(p): sha(p) for p in binaries.values()},
                  compiler_hashes={tool: sha(shutil.which(tool)) for tool in versions},
                  polybench_reference_hashes={str(p): sha(p) for p in references if p.is_file()},
                  bsp_hashes={str(p): sha(p) for p in library_paths + [pc]})
    write_json(output / "provenance.json", result)
    return result
