#!/usr/bin/env python3
"""Replay prepared TACLeBench IR; separate pipeline success from corroboration.

Input is the original tacle_hara.py output (tacle.json and per-case workload.ll,
src/). This preserves the exact entry selection and always-inline pipeline.
The conservative gate checks CFG-derived memory counts and residual calls;
it is not an address/order oracle and does not execute benchmark binaries.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import resource
import shutil
import subprocess

if __package__:
    from .oracles import ORACLES, compare_trace, source_accesses
else:
    from oracles import ORACLES, compare_trace, source_accesses

# LLVM 14 Intrinsics.td declares these IntrNoMem + IntrWillReturn. Do not
# generalize this exception to llvm.*: memcpy and masked loads access memory.
PURE_CALLS = {"llvm.fmuladd.f32", "llvm.fmuladd.f64"}
LIMIT = 200000000


def assess(model, report, entry):
    """Return reasons why pipeline success lacks corroborating IR evidence."""
    tasks = report.get("tasks", [])
    if len(tasks) != 1 or tasks[0].get("task_id") != entry:
        return ["unexpected_tasks"]
    task = tasks[0]
    issues = []
    coverage = task.get("coverage", {})
    if (not coverage.get("complete") or coverage.get("rejected_accesses") != 0
            or coverage.get("resolved_accesses") != coverage.get("source_accesses")):
        issues.append("incomplete_mapping")
    if not task.get("modeled_accesses"):
        issues.append("empty_stream")
    if coverage.get("excluded_opaque_call_sites") != 0:
        issues.append("excluded_calls")
    if not task.get("invariants", {}).get("all_passed"):
        issues.append("cache_invariants_failed")
    ir = report.get("ir_instructions", {})
    if ir.get("status") != "exact":
        issues.append("unproved_control_flow")
    else:
        counts = [f for f in ir.get("functions", []) if f.get("function") == entry]
        if len(counts) != 1:
            issues.append("missing_ir_root")
        else:
            opcodes = counts[0]["opcodes"]
            expected = sum(opcodes.get(op, {}).get("dynamic", 0)
                           for op in ("load", "store"))
            if expected != coverage.get("source_accesses"):
                issues.append("memory_count_mismatch")
    roots = [f for f in model.get("functions", []) if f.get("function") == entry]
    if len(roots) != 1:
        issues.append("missing_map_root")
    else:
        # All helpers were inlined into this root by the original preparation.
        for block in roots[0].get("ir_instructions", {}).get("blocks", []):
            if block["executions"]:
                for call in block.get("calls", []):
                    if call.get("callee") not in PURE_CALLS:
                        issues.append("residual_call:" + str(call.get("callee")))
    return sorted(set(issues))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")


def diagnostic(work, stage, code):
    lines = (work / f"{stage}.stderr").read_text(errors="replace").splitlines()
    return next((s for s in lines if "error:" in s), lines[0] if lines else str(code))


def command(argv, work, stage, timeout):
    """Keep command, exit status and diagnostics, including failed invocations."""
    argv = list(map(str, argv))
    with (work / f"{stage}.stdout").open("w") as out, \
            (work / f"{stage}.stderr").open("w") as err:
        try:
            code = subprocess.run(argv, cwd=work, stdout=out, stderr=err,
                                  timeout=timeout, check=False).returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    write_json(work / f"{stage}.command.json", {"argv": argv, "exit_code": code})
    return code


def yarda_cpp_argv(build, cache, analyses, export):
    """Apply the common budget to each analysis's supported limit options."""
    argv = [build / "backend/yarda_cpp", "workload_ape.json",
            *[argument for analysis in analyses for argument in ("--analysis", analysis)],
            "--cache", cache, "--elf", "workload.elf",
            "--max-single-loop-iterations", str(LIMIT),
            "--max-cumulative-loop-iterations", str(LIMIT), "--export", export]
    if "hierarchy-rd" in analyses:
        argv.extend(["--max-source-accesses", str(LIMIT), "--max-line-references", str(LIMIT)])
    return argv


def verify_source_trace(name, work, build, cache, timeout, identity):
    """Check supported source oracles using a separate ELF symbol reader."""
    expected = source_accesses(name)
    if expected is None:
        return "not_available", None
    tree_hash = hashlib.sha256(json.dumps(identity.get("sources_sha256", {}),
                                         sort_keys=True).encode()).hexdigest()
    if (identity.get("input_sha256"), tree_hash) != ORACLES[name][:2]:
        return "changed_input", "IR/source snapshot differs from the reviewed oracle input"
    code = command(yarda_cpp_argv(build, cache, ["mapping"], "mapping.json"),
                   work, "mapping", timeout)
    if code != 0:
        return "failed", diagnostic(work, "mapping", code)
    code = command(["nm", "-S", "--defined-only", "workload.elf"], work, "symbols", timeout)
    if code != 0:
        return "failed", diagnostic(work, "symbols", code)
    symbols = {}
    for line in (work / "symbols.stdout").read_text().splitlines():
        parts = line.split()
        if len(parts) == 4:
            symbols[parts[3]] = (int(parts[0], 16), int(parts[1], 16))
    mapping = json.loads((work / "mapping.json").read_text())
    reason = compare_trace(mapping["tasks"][0]["resolved_accesses"], expected, symbols)
    return ("mismatch", reason) if reason else ("passed", None)


def replay(prior, row, output, build, cache, timeout):
    """Re-extract one unchanged input and link its original annotated sources."""
    work = output / row["benchmark"]
    work.mkdir()
    old = prior / row["benchmark"]
    source = old / "workload.ll"
    shutil.copy2(source, work / "workload.ll")
    result = {"benchmark": row["benchmark"], "suite": row["suite"],
              "entry": row["entry"], "prior_status": row["status"],
              "input_sha256": sha(source), "status": "frontend_failed"}
    code = command(["opt-14", f"-load-pass-plugin={build / 'libMemoryAccessPatterns.so'}",
        "-passes=function(mem2reg),loop-simplify,loop-annotated-trace",
        "workload.ll", "-disable-output"], work, "frontend", timeout)
    result["frontend_exit"] = code
    if code != 0:
        result["reason"] = diagnostic(work, "frontend", code)
        return result
    sources = sorted((old / "src").rglob("*.c"))
    includes = sorted({p.parent for p in (old / "src").rglob("*.h")} | {old / "src"})
    code = command(["clang-14", "-O0", "-g", "-fno-pie", "-no-pie",
        *[f"-I{p}" for p in includes], *sources, "-lm", "-o", "workload.elf"],
        work, "elf", timeout)
    if code != 0:
        return dict(result, status="elf_failed", elf_exit=code)
    result["elf_sha256"] = sha(work / "workload.elf")
    result["sources_sha256"] = {str(p.relative_to(old)): sha(p)
                                for p in sorted((old / "src").rglob("*")) if p.is_file()}
    code = command(yarda_cpp_argv(build, cache, ["hierarchy-rd", "ir-instructions"],
                                 "report.json"), work, "backend", timeout)
    if code != 0:
        return dict(result, status="backend_failed", backend_exit=code,
                    reason=diagnostic(work, "backend", code))
    model = json.loads((work / "workload_ape.json").read_text())
    report = json.loads((work / "report.json").read_text())
    issues = assess(model, report, row["entry"])
    result.update(status="needs_review" if issues else "corroborated_counts",
                  pipeline_success=True, review_reasons=issues,
                  source_accesses=sum(t["coverage"]["source_accesses"] for t in report["tasks"]),
                  ir_status=report["ir_instructions"]["status"],
                  ir_reason=report["ir_instructions"].get("reason"))
    if not issues:
        oracle, reason = verify_source_trace(row["benchmark"], work, build, cache, timeout, result)
        result.update(trace_oracle=oracle, trace_oracle_reason=reason)
        if oracle == "passed":
            result["status"] = "trace_verified"
        elif oracle in ("failed", "mismatch"):
            result["status"] = "oracle_failed"
        elif oracle == "changed_input":
            result["status"] = "needs_review"
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prior", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--cache", type=Path, default=Path("backend/config/cache.32b.yaml"))
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    prior, output, build, cache = (p.resolve() for p in
                                 (args.prior, args.output, args.build, args.cache))
    output.mkdir(parents=True, exist_ok=False)
    rows = json.loads((prior / "tacle.json").read_text())
    write_json(output / "provenance.json", {
        "prior": str(prior), "baseline_sha256": sha(prior / "tacle.json"),
        "plugin_sha256": sha(build / "libMemoryAccessPatterns.so"),
        "backend_sha256": sha(build / "backend/yarda_cpp"),
        "cache_path": str(cache), "cache_sha256": sha(cache),
        "runner_sha256": sha(Path(__file__)),
        "oracle_sha256": sha(Path(__file__).with_name("oracles.py")),
        "versions": {tool: subprocess.check_output([tool, "--version"], text=True).splitlines()[0]
                     for tool in ("clang-14", "opt-14")},
        "timeout_seconds": args.timeout, "source_access_limit": LIMIT,
        "scope": "unchanged prepared IR; counts corroboration, not trace parity"})
    results = []
    for row in rows:
        result = replay(prior, row, output, build, cache, args.timeout)
        results.append(result)
        write_json(output / "results.json", results)
        print(f"{row['benchmark']}: {result['status']} "
              f"{result.get('reason', result.get('review_reasons', ''))}", flush=True)
    summary = dict(Counter(r["status"] for r in results))
    write_json(output / "summary.json", summary)
    print(json.dumps(summary, sort_keys=True))


if __name__ == "__main__":
    main()
