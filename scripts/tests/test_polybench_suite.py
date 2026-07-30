from pathlib import Path

from scripts.polybench_suite import (
    canonicalize_array_names,
    compile_flags,
    discover_workloads,
    find_kernel_name,
)


SUITE = Path("/workspace/PolyBenchC-4.2.1")


def test_discovers_supported_and_excluded_workloads():
    workloads = discover_workloads(SUITE)

    assert len(workloads) == 30
    assert sum(workload.supported for workload in workloads) == 21
    assert {workload.name for workload in workloads if not workload.supported} == {
        "symm",
        "syr2k",
        "syrk",
        "cholesky",
        "durbin",
        "lu",
        "ludcmp",
        "trisolv",
        "nussinov",
    }


def test_finds_single_kernel_function_in_llvm_ir():
    llvm_ir = """
define internal void @init_array() {
  ret void
}
define internal void @kernel_fdtd_2d(i32 %tmax) {
  ret void
}
"""

    assert find_kernel_name(llvm_ir) == "kernel_fdtd_2d"


def test_canonicalizes_nested_array_names_from_object_metadata():
    module = {
        "metadata": {
            "objects": {
                "function:kernel::param:A": {"name": "A"},
            }
        },
        "functions": [
            {
                "function": "kernel",
                "body": [
                    {
                        "type": "Loop",
                        "body": [
                            {
                                "type": "Array",
                                "name": "A[i+1][j]",
                                "object": "function:kernel::param:A",
                            }
                        ],
                    }
                ],
            }
        ],
    }

    canonicalize_array_names(module)

    array = module["functions"][0]["body"][0]["body"][0]
    assert array["name"] == "A"


def test_rejects_ambiguous_kernel_functions():
    llvm_ir = """
define void @kernel_first() {
  ret void
}
define void @kernel_second() {
  ret void
}
"""

    try:
        find_kernel_name(llvm_ir)
    except ValueError as error:
        assert "exactly one kernel" in str(error)
    else:
        raise AssertionError("ambiguous kernel functions must be rejected")


def test_compile_flags_select_requested_dataset():
    workload = next(
        workload for workload in discover_workloads(SUITE)
        if workload.name == "mvt"
    )

    flags = compile_flags(SUITE, workload, "small")

    assert "-DSMALL_DATASET" in flags
    assert "-DMINI_DATASET" not in flags
