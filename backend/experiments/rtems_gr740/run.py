#!/usr/bin/env python3
"""Rebuild, verify and measure memory workloads linked against RTEMS/GR740."""
import argparse
from pathlib import Path
import shutil
import shlex
import sys

from cases import matrix
from execution import checked, read_json, run, sha, write_json
from prepare import HERE, provenance, prepare
from validation import negative_cases, verify_case


def sample(case, mode, destination, binaries, baseline):
    """Measure one fresh evaluator process and check RESULT bytes."""
    prefix = destination.parent / (destination.name + "-command")
    status = run(["sh", HERE.parent / "helpers/limit_memory.sh", "4194304",
                  binaries["evaluator"], case, mode, destination], prefix)
    measurement_path = destination / "measurement.json"
    if not measurement_path.exists():
        destination.mkdir(exist_ok=True)
        write_json(measurement_path, dict(case_id=read_json(case)["case_id"], mode=mode,
                                         status=status["status"] if status["status"] == "timeout" else "error",
                                         process_exit=status["exit_code"]))
    measurement = read_json(measurement_path)
    if measurement["status"] != "success" or status["exit_code"] != 0:
        raise RuntimeError(f"sample failure: {destination}")
    if sha(destination / "result.json") != sha(baseline):
        raise AssertionError(f"RESULT mismatch: {destination}")
    return measurement_path


def target_runs(simulator, cases, output):
    """Attempt real RTEMS execution; stop and record the first environment failure."""
    records = []
    if simulator is None:
        write_json(output / "target-runtime.json", dict(status="not_requested", runs=[]))
        return
    for case in cases:
        row = read_json(case)
        prefix = output / "target" / row["case_id"]
        command = [str(simulator), "-r", "-core0", row["elf_path"]]
        if shutil.which("script"):
            command = ["script", "-q", "-e", "-c", shlex.join(command), str(prefix) + ".terminal.log"]
        record = run(command, prefix, timeout=120)
        text = Path(str(prefix) + ".stdout").read_text()
        complete = ("YARDA_RTEMS_COMPLETE" in text and text.count("valid=1") == 11
                    and "valid=0" not in text)
        records.append(dict(case_id=row["case_id"], command=record, complete=complete))
        if record["exit_code"] != 0 or not complete:
            write_json(output / "target-runtime.json", dict(status="unavailable", runs=records))
            return
    write_json(output / "target-runtime.json", dict(status="success", runs=records))


def main():
    """Require fresh output and keep compilation/verification outside timed samples."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--repeats", type=int, default=10)
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--simulator", type=Path)
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error("repeats must be positive")
    build, output = args.build.resolve(), args.output.resolve()
    binaries = dict(cli=build / "backend/yarda_cpp", region=build / "frontend/yarda_region_map",
                    evaluator=build / "backend/experiments/yarda_hierarchy_evaluate",
                    verify=build / "backend/experiments/rtems_gr740/yarda_rtems_gr740_verify")
    for path in binaries.values():
        if not path.is_file():
            parser.error(f"missing executable: {path}")
    output.mkdir(parents=True, exist_ok=False)
    shutil.copy2(HERE / "cache-model.yaml", output / "cache-model.yaml")
    identity = provenance(output, build, binaries)
    rows = matrix(args.smoke)
    write_json(output / "manifest.json", dict(cases=rows, repeats=args.repeats,
        warmups=1, memory_limit_kib=4194304, timeout_seconds=180,
        formal_measurement=args.repeats >= 10 and not args.smoke))
    prepared, baselines = [], {}
    for row in rows:
        print(f"prepare/verify {row['case_id']}", flush=True)
        case = prepare(row, output, binaries)
        prepared.append(case)
        baselines[row["case_id"]] = verify_case(case, binaries, output)
    negative_cases(prepared[0], binaries, output)
    target_runs(args.simulator, prepared, output)
    samples = []
    for case in prepared:
        row = read_json(case)
        directory = output / "runs" / row["case_id"]
        directory.mkdir(parents=True)
        print(f"measure {row['case_id']} ({args.repeats} repeats per mode)", flush=True)
        for mode in ("batch", "streaming", "instrumented"):
            sample(case, mode, directory / f"{mode}-warmup", binaries, baselines[row["case_id"]])
        for repeat in range(args.repeats):
            modes = ("batch", "streaming", "instrumented")
            if repeat % 2:
                modes = tuple(reversed(modes))
            for mode in modes:
                path = sample(case, mode, directory / f"{mode}-{repeat:02}", binaries,
                              baselines[row["case_id"]])
                samples.append(str(path))
                write_json(output / "samples.json", samples)
    checked([binaries["evaluator"], "summarize", output / "samples.json", output / "summary.json"],
            output / "summarize")
    for path, expected in identity["binary_hashes"].items():
        if sha(path) != expected:
            raise AssertionError(f"binary changed during measurement: {path}")
    for path, expected in read_json(output / "sources.json").items():
        if sha(path) != expected:
            raise AssertionError(f"source changed during measurement: {path}")
    for case in prepared:
        row = read_json(case)
        for kind in ("map", "elf", "cache"):
            if sha(row[f"{kind}_path"]) != row[f"{kind}_sha256"]:
                raise AssertionError(f"input changed during measurement: {case}")
    write_json(output / "completion.json", dict(status="success", cases=len(prepared),
        measured_results_identical=len(samples), rebuilt_elf_and_map_identical=len(prepared),
        target_runtime=read_json(output / "target-runtime.json")["status"]))
    print(f"completed: {output} ({len(samples)} identical measured RESULTS)", flush=True)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"RTEMS experiment failed: {error}", file=sys.stderr)
        raise
