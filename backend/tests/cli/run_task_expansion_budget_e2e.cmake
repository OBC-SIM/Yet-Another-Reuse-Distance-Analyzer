foreach(required YARDA_CLANG YARDA_OPT YARDA_PLUGIN YARDA_CPP YARDA_SOURCE
                 YARDA_INCLUDE_DIR YARDA_CACHE YARDA_WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

function(run_checked label working_directory)
    execute_process(
        COMMAND ${ARGN}
        WORKING_DIRECTORY "${working_directory}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed (${result})\nstdout:\n${output}\nstderr:\n${error_output}")
    endif()
endfunction()

file(REMOVE_RECURSE "${YARDA_WORK_DIR}")
file(MAKE_DIRECTORY "${YARDA_WORK_DIR}")

set(ir "${YARDA_WORK_DIR}/task_expansion_budget_e2e.ll")
set(lat "${YARDA_WORK_DIR}/task_expansion_budget_e2e_ape.json")
set(elf "${YARDA_WORK_DIR}/task_expansion_budget_e2e.elf")
set(result_file "${YARDA_WORK_DIR}/unexpected_result.json")

run_checked("C to LLVM IR" "${YARDA_WORK_DIR}"
    "${YARDA_CLANG}" -O0 -Xclang -disable-O0-optnone -g
    -I "${YARDA_INCLUDE_DIR}" -emit-llvm -S "${YARDA_SOURCE}" -o "${ir}")
run_checked("LLVM IR to APE/LAT" "${YARDA_WORK_DIR}"
    "${YARDA_OPT}" -load-pass-plugin "${YARDA_PLUGIN}"
    "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
    "${ir}" -o /dev/null)
run_checked("C to ET_EXEC" "${YARDA_WORK_DIR}"
    "${YARDA_CLANG}" -O0 -g -fno-pie -no-pie
    -I "${YARDA_INCLUDE_DIR}" "${YARDA_SOURCE}" -o "${elf}")

file(REMOVE "${result_file}")
execute_process(
    COMMAND "${YARDA_CPP}" "${lat}" --elf "${elf}" --cache "${YARDA_CACHE}"
            --export "${result_file}"
    WORKING_DIRECTORY "${YARDA_WORK_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(result EQUAL 0)
    message(FATAL_ERROR "expansion budget unexpectedly succeeded: ${output}")
endif()
string(FIND "${error_output}"
    "inline call expansion exceeds 100000 nodes" error_position)
if(error_position EQUAL -1)
    message(FATAL_ERROR "unexpected expansion error: ${error_output}")
endif()
if(EXISTS "${result_file}")
    message(FATAL_ERROR "failed expansion left a partial export")
endif()
