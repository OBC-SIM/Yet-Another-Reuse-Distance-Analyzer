"""Correctness and rejection gates around the existing C++ analyzers."""
import copy
import shutil

from execution import checked, cli_command, read_json, run, sha, write_json
from prepare import HERE, RTEMS


def verify_case(case, binaries, output):
    """Compare independent sources, every event, CLI bytes and diagnostics."""
    row = read_json(case)
    directory = output / "verification" / row["case_id"]
    directory.mkdir(parents=True)
    result = directory / "result.json"
    command = cli_command(binaries["cli"], row, result)
    checked(command, directory / "cli")
    checked([binaries["verify"], case, result], directory / "oracles", timeout=300)
    diagnostics = cli_command(binaries["cli"], row, directory / "diagnostic-result.json")
    diagnostics += ["--export-events", directory / "events.json", "--event-limit", "16",
                    "--telemetry", directory / "telemetry.json"]
    checked(diagnostics, directory / "diagnostics")
    checked(cli_command(binaries["cli"], row, directory / "repeat-result.json"), directory / "repeat")
    for name in ("diagnostic-result.json", "repeat-result.json"):
        if sha(result) != sha(directory / name):
            raise AssertionError(f"non-deterministic RESULT: {row['case_id']}")
    document = read_json(result)
    task = document["tasks"][0]
    if len(document["tasks"]) != 1 or not all(task["invariants"].values()):
        raise AssertionError("incomplete task or invariants")
    coverage = task["coverage"]
    if (coverage["source_accesses"] != row["expected_sources"] or
        coverage["emitted_line_references"] != row["expected_sources"] or
        not coverage["complete"] or coverage["excluded_opaque_call_sites"] or
        coverage["rejected_accesses"]):
        raise AssertionError("unexpected source coverage")
    events = read_json(directory / "events.json")
    telemetry = read_json(directory / "telemetry.json")
    if telemetry["analysis_id"] != document["analysis_id"] or len(events["events"]) != min(16, row["expected_sources"]):
        raise AssertionError("diagnostic identity/count mismatch")
    return result


def negative_cases(case, binaries, output):
    """Require real SPARC input failures, preserved outputs and independent-oracle sensitivity."""
    row = read_json(case)
    directory = output / "rejections"
    directory.mkdir()
    records = []
    for key in row["effective_work_limits"]:
        rejected = copy.deepcopy(row)
        rejected["effective_work_limits"][key] -= 1
        case_file = directory / f"{key}.json"
        write_json(case_file, rejected)
        for mode in ("batch", "streaming", "instrumented"):
            destination = directory / f"{key}-{mode}"
            status = run([binaries["evaluator"], case_file, mode, destination],
                         directory / f"{key}-{mode}-command")
            measurement = read_json(destination / "measurement.json")
            if (status["exit_code"] != 1 or measurement["status"] != "budget_exhaustion" or
                (destination / "result.json").exists()):
                raise AssertionError(f"budget failure contract: {key}/{mode}")
            records.append(dict(case=key, mode=mode, status="expected_budget_exhaustion"))
        target = directory / f"{key}-preserved.json"
        target.write_text("preserve-existing-artifact\n")
        status = run(cli_command(binaries["cli"], rejected, target), directory / f"{key}-cli")
        if status["exit_code"] != 1 or target.read_text() != "preserve-existing-artifact\n":
            raise AssertionError("CLI budget failure clobbered an existing RESULT")
    hardware_cache = directory / "hardware-no-write-allocate.yaml"
    hardware_cache.write_text((output / "cache-model.yaml").read_text().replace("write_allocate: true", "write_allocate: false", 1))
    rejected = dict(row, cache_path=str(hardware_cache))
    destination = directory / "hardware-result.json"
    status = run(cli_command(binaries["cli"], rejected, destination), directory / "hardware-policy")
    error = (directory / "hardware-policy.stderr").read_text()
    if status["exit_code"] != 1 or "write_allocate=true" not in error or destination.exists():
        raise AssertionError("physical L1 policy must be rejected by current model")
    records.append(dict(case="hardware-policy", status="expected_unsupported"))
    stripped = directory / "missing-symbol.elf"
    shutil.copy2(row["elf_path"], stripped)
    checked([RTEMS / "bin/sparc-rtems6-objcopy", "--strip-symbol=A", stripped], directory / "strip")
    destination = directory / "missing-symbol-result.json"
    status = run(cli_command(binaries["cli"], dict(row, elf_path=str(stripped)), destination),
                 directory / "missing-symbol")
    if status["exit_code"] != 1 or destination.exists():
        raise AssertionError("missing ELF symbol was not rejected")
    records.append(dict(case="missing-symbol", status="expected_unsupported"))
    source = (HERE / "kernels/atax.c").read_text().replace(
        '#include "../workload.h"', "extern volatile int runtime_n;")
    dynamic = directory / "dynamic-bound.c"
    dynamic.write_text(source.replace("j < N", "j < runtime_n"))
    destination = directory / "dynamic-map.json"
    status = run([binaries["region"], dynamic, destination, "--", "--target=sparc-unknown-rtems6",
                  "-DM=2", "-DN=3"], directory / "dynamic-bound")
    error = (directory / "dynamic-bound.stderr").read_text()
    if status["exit_code"] != 1 or "loop bound" not in error or destination.exists():
        raise AssertionError("runtime-bound frontend rejection was not observed")
    records.append(dict(case="dynamic-bound", status="expected_unsupported"))
    mutated = read_json(row["map_path"])
    mutated["functions"][0]["body"][0]["body"][0]["op"] = "load"
    mutated_map = directory / "mutated-map.json"
    write_json(mutated_map, mutated)
    mutated_row = dict(row, map_path=str(mutated_map))
    mutated_case = directory / "mutated-case.json"
    write_json(mutated_case, mutated_row)
    destination = directory / "mutated-result.json"
    checked(cli_command(binaries["cli"], mutated_row, destination), directory / "mutated-cli")
    status = run([binaries["verify"], mutated_case, destination,
                  "--gtest_filter=RtemsGr740.SparcElfAndEverySourceMatchIndependentKernelEquations"],
                 directory / "mutated-oracle")
    if status["exit_code"] != 1:
        raise AssertionError("independent source oracle missed the load/store mutation")
    records.append(dict(case="load-store-mutation", status="expected_oracle_failure"))
    write_json(directory / "summary.json", records)
    return records
