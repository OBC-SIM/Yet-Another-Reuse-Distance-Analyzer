cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/helpers/generate.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/json.cmake")
set(source "${CMAKE_CURRENT_LIST_DIR}/fixtures/work_limits.c")
generate_legacy(limits "${source}")
set(flags --max-single-loop-iterations --max-cumulative-loop-iterations
          --max-source-accesses --max-line-references)
set(exact --max-single-loop-iterations 3 --max-cumulative-loop-iterations 5
          --max-source-accesses 5 --max-line-references 10)
set(low 2 4 4 9)
set(reasons "loop iteration count" "cumulative loop iteration count"
            "emitted source accesses" "emitted line references")
run_checked("exact allowances" "${YARDA_CPP}" ${base} ${exact} ${diagnostics})
configure_file("${result_file}" "${case_dir}/expected.json" COPYONLY)
file(READ "${telemetry_file}" payload)
assert_json("5" source_accesses_emitted)
assert_json("10" line_references_emitted)
assert_json("5" loop_iterations_expanded)
foreach(index RANGE 0 3)
    list(GET flags ${index} flag)
    list(GET low ${index} lower)
    list(GET reasons ${index} reason)
    expect_analysis_failure("error: ${reason} exceeds ${lower}(\n|$)"
        ${base} ${exact} ${diagnostics} ${flag} ${lower})
    run_checked("higher ${flag}" "${YARDA_CPP}" ${base} ${exact} ${diagnostics}
        ${flag} 18446744073709551615)
    compare_files("${result_file}" "${case_dir}/expected.json")
endforeach()

generate_legacy(empty "${source}" -DEMPTY_WORK)
expect_analysis_failure("error: loop iteration count exceeds 1000000(\n|$)"
    ${base} ${diagnostics}
    --max-source-accesses 0 --max-line-references 0)
run_checked("explicit empty-loop allowance" "${YARDA_CPP}" ${base} ${diagnostics}
    --max-single-loop-iterations 1000001 --max-cumulative-loop-iterations 1000001
    --max-source-accesses 0 --max-line-references 0)
file(READ "${result_file}" payload)
assert_json("0" tasks 0 ma)
file(READ "${telemetry_file}" payload)
assert_json("1000001" loop_iterations_expanded)
assert_json("0" source_accesses_emitted)

# Structural inline expansion has a separate, non-configurable guard.
generate_legacy(expansion
    "${YARDA_REPOSITORY}/backend/tests/cli/task_expansion_budget_e2e.c")
expect_analysis_failure("error: inline call expansion exceeds 100000 nodes(\n|$)"
    ${base} ${diagnostics})
