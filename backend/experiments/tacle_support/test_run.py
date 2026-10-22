"""Conservative acceptance gates for replaying the original TACLeBench inputs."""
from copy import deepcopy
import hashlib
import json
import sys

import pytest

from backend.experiments.tacle_support import run
from backend.experiments.tacle_support.run import assess, verify_source_trace


def evidence():
    model = {"functions": [{"function": "kernel", "ir_instructions": {
        "blocks": [{"executions": 1, "calls": []}]}}]}
    report = {"tasks": [{"task_id": "kernel", "modeled_accesses": 4,
        "coverage": {"complete": True, "source_accesses": 4,
                     "resolved_accesses": 4, "rejected_accesses": 0,
                     "excluded_opaque_call_sites": 0},
        "invariants": {"all_passed": True}}],
        "ir_instructions": {"status": "exact", "functions": [
            {"function": "kernel", "opcodes": {
                "load": {"dynamic": 3}, "store": {"dynamic": 1}}}]}}
    return model, report


def test_corroborates_exact_memory_count_without_claiming_trace_parity():
    model, report = evidence()
    assert assess(model, report, "kernel") == []


def test_complete_map_does_not_hide_missing_ir_load():
    model, report = evidence()
    report["ir_instructions"]["functions"][0]["opcodes"]["load"]["dynamic"] = 4
    assert "memory_count_mismatch" in assess(model, report, "kernel")


def test_successful_combined_command_can_have_unproved_control_flow():
    model, report = evidence()
    report["ir_instructions"] = {"status": "error", "reason": "data-dependent branch"}
    assert "unproved_control_flow" in assess(model, report, "kernel")


def test_omitted_recursive_call_is_visible_in_raw_instruction_metadata():
    model, report = evidence()
    model["functions"][0]["ir_instructions"]["blocks"][0]["calls"] = [
        {"callee": "fib", "inline": False}]
    assert "residual_call:fib" in assess(model, report, "kernel")


@pytest.mark.parametrize("field,value", [("complete", False),
    ("rejected_accesses", 1), ("resolved_accesses", 3)])
def test_incomplete_coverage_is_not_corroborated(field, value):
    model, report = evidence()
    report["tasks"][0]["coverage"][field] = value
    assert "incomplete_mapping" in assess(model, report, "kernel")


def test_wrong_or_additional_task_is_not_accepted():
    model, report = evidence()
    report["tasks"].append(deepcopy(report["tasks"][0]))
    assert "unexpected_tasks" in assess(model, report, "kernel")


def test_empty_stream_is_not_counted_as_a_supported_benchmark():
    model, report = evidence()
    report["tasks"][0]["modeled_accesses"] = 0
    assert "empty_stream" in assess(model, report, "kernel")


def test_arithmetic_intrinsic_does_not_add_memory_accesses():
    model, report = evidence()
    model["functions"][0]["ir_instructions"]["blocks"][0]["calls"] = [
        {"callee": "llvm.fmuladd.f32", "inline": False}]
    assert assess(model, report, "kernel") == []


def test_memory_intrinsic_requires_review_even_when_named_llvm():
    model, report = evidence()
    model["functions"][0]["ir_instructions"]["blocks"][0]["calls"] = [
        {"callee": "llvm.memcpy.p0i8.p0i8.i64", "inline": False}]
    assert "residual_call:llvm.memcpy.p0i8.p0i8.i64" in assess(model, report, "kernel")


def test_failed_cache_invariants_require_review():
    model, report = evidence()
    report["tasks"][0]["invariants"]["all_passed"] = False
    assert "cache_invariants_failed" in assess(model, report, "kernel")


@pytest.mark.parametrize("value", [None, 1])
def test_missing_or_nonzero_hierarchy_call_exclusions_require_review(value):
    model, report = evidence()
    coverage = report["tasks"][0]["coverage"]
    if value is None:
        del coverage["excluded_opaque_call_sites"]
    else:
        coverage["excluded_opaque_call_sites"] = value
    assert "excluded_calls" in assess(model, report, "kernel")


def test_known_benchmark_name_does_not_verify_unreviewed_input(tmp_path):
    status, _ = verify_source_trace("matrix1", tmp_path, tmp_path,
                                    tmp_path / "cache.yaml", 1, {})
    assert status == "changed_input"


@pytest.mark.parametrize("corrupt,status", [(False, "passed"), (True, "mismatch")])
def test_source_oracle_checks_mapping_against_independent_elf_symbols(
        tmp_path, monkeypatch, corrupt, status):
    identity = {"input_sha256": "reviewed-ir", "sources_sha256": {"src/example.c": "reviewed-c"}}
    pin = (identity["input_sha256"], hashlib.sha256(json.dumps(
        identity["sources_sha256"], sort_keys=True).encode()).hexdigest())
    # Source: value = array[0]; array[1] = value; each element is 4 bytes.
    def source_formula(emit):
        emit("array", 0, "load")
        emit("array", 1, "store")

    monkeypatch.setitem(run.ORACLES, "example", (*pin, source_formula))
    actual = [
        {"object_id": "global::example_array", "object_byte_offset": 0, "access_size": 4,
         "operation": "load", "source_access_ordinal": 0, "linked_byte_address": 4096},
        {"object_id": "global::example_array", "object_byte_offset": 4, "access_size": 4,
         "operation": "store", "source_access_ordinal": 1, "linked_byte_address": 4100}]
    if corrupt:
        actual[1]["linked_byte_address"] = 4096
    calls = {}

    def command(argv, work, stage, timeout):
        calls[stage] = argv
        if stage == "mapping":
            run.write_json(work / "mapping.json", {"tasks": [{"resolved_accesses": actual}]})
        elif stage == "symbols":
            (work / "symbols.stdout").write_text("00001000 00000008 B example_array\n")
        else:
            pytest.fail("unexpected oracle command: " + stage)
        return 0

    monkeypatch.setattr(run, "command", command)
    monkeypatch.setattr(run, "LIMIT", 17)
    observed, reason = verify_source_trace("example", tmp_path, tmp_path,
                                          tmp_path / "cache.yaml", 1, identity)
    assert observed == status
    assert reason is None if not corrupt else "linked_byte_address" in reason
    argv = calls["mapping"]
    for flag in ("--max-single-loop-iterations", "--max-cumulative-loop-iterations"):
        assert argv[argv.index(flag) + 1] == "17"
    assert "--max-source-accesses" not in argv
    assert "--max-line-references" not in argv
    assert calls["symbols"] == ["nm", "-S", "--defined-only", "workload.elf"]


@pytest.mark.parametrize("oracle,status", [("passed", "trace_verified"),
    ("mismatch", "oracle_failed"), ("failed", "oracle_failed"),
    ("changed_input", "needs_review"), ("not_available", "corroborated_counts")])
def test_replay_preserves_oracle_status_and_reason(tmp_path, monkeypatch, oracle, status):
    prior, output = tmp_path / "prior", tmp_path / "output"
    source = prior / "example" / "src"
    source.mkdir(parents=True)
    output.mkdir()
    (source / "example.c").write_text("int value;\n")
    (source.parent / "workload.ll").write_text("; prepared IR\n")
    model, report = evidence()
    calls = {}

    def command(argv, work, stage, timeout):
        calls[stage] = argv
        if stage == "frontend":
            run.write_json(work / "workload_ape.json", model)
        elif stage == "elf":
            (work / "workload.elf").write_bytes(b"ELF fixture")
        elif stage == "backend":
            run.write_json(work / "report.json", report)
        else:
            pytest.fail("unexpected replay command: " + stage)
        return 0

    reason = None if oracle in ("passed", "not_available") else "oracle evidence"
    monkeypatch.setattr(run, "command", command)
    monkeypatch.setattr(run, "verify_source_trace", lambda *args: (oracle, reason))
    monkeypatch.setattr(run, "LIMIT", 17)
    result = run.replay(prior, {"benchmark": "example", "suite": "test",
        "entry": "kernel", "status": "frontend_failed"}, output, tmp_path,
        tmp_path / "cache.yaml", 1)
    assert result["status"] == status
    assert result["trace_oracle"] == oracle
    assert result["trace_oracle_reason"] == reason
    assert result["pipeline_success"] is True
    assert result["source_accesses"] == 4
    argv = calls["backend"]
    for flag in ("--max-source-accesses", "--max-line-references",
                 "--max-single-loop-iterations", "--max-cumulative-loop-iterations"):
        assert argv[argv.index(flag) + 1] == "17"


def test_provenance_records_the_execution_limit(tmp_path, monkeypatch):
    prior, output, build = tmp_path / "prior", tmp_path / "output", tmp_path / "build"
    prior.mkdir()
    (build / "backend").mkdir(parents=True)
    run.write_json(prior / "tacle.json", [])
    (build / "libMemoryAccessPatterns.so").write_bytes(b"plugin")
    (build / "backend/yarda_cpp").write_bytes(b"backend")
    cache = tmp_path / "cache.yaml"
    cache.write_text("cache fixture\n")
    monkeypatch.setattr(sys, "argv", ["run.py", str(prior), str(output),
        "--build", str(build), "--cache", str(cache)])
    monkeypatch.setattr(run.resource, "setrlimit", lambda *args: None)
    monkeypatch.setattr(run.subprocess, "check_output", lambda *args, **kwargs: "tool version\n")
    monkeypatch.setattr(run, "LIMIT", 17)
    run.main()
    provenance = json.loads((output / "provenance.json").read_text())
    assert provenance["source_access_limit"] == 17
