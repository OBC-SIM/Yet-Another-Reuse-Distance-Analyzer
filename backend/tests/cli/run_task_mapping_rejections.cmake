foreach(required YARDA_CLANG YARDA_OPT YARDA_PLUGIN YARDA_CPP YARDA_SOURCE
                 YARDA_INCLUDE_DIR YARDA_CACHE YARDA_OTHER_ELF YARDA_WORK_DIR)
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

function(expect_cli_failure label lat elf expected_error)
    execute_process(
        COMMAND "${YARDA_CPP}" "${lat}" --elf "${elf}"
                --cache "${YARDA_CACHE}"
        WORKING_DIRECTORY "${YARDA_WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
    if(result EQUAL 0)
        message(FATAL_ERROR "${label} unexpectedly succeeded: ${output}")
    endif()
    string(FIND "${error_output}" "${expected_error}" error_position)
    if(error_position EQUAL -1)
        message(FATAL_ERROR
            "${label} did not report '${expected_error}': ${error_output}")
    endif()
endfunction()

function(run_rejection case_name definition expected_error)
    set(ir "${YARDA_WORK_DIR}/unsupported_${case_name}.ll")
    set(lat "${YARDA_WORK_DIR}/unsupported_${case_name}_ape.json")
    set(elf "${YARDA_WORK_DIR}/unsupported_${case_name}.elf")

    run_checked("${case_name} C to LLVM IR" "${YARDA_WORK_DIR}"
        "${YARDA_CLANG}" -O0 -Xclang -disable-O0-optnone -g
        -I "${YARDA_INCLUDE_DIR}" "-D${definition}"
        -emit-llvm -S "${YARDA_SOURCE}" -o "${ir}")
    run_checked("${case_name} LLVM IR to LAT" "${YARDA_WORK_DIR}"
        "${YARDA_OPT}" -load-pass-plugin "${YARDA_PLUGIN}"
        "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
        "${ir}" -o /dev/null)
    run_checked("${case_name} C to ET_EXEC" "${YARDA_WORK_DIR}"
        "${YARDA_CLANG}" -O0 -g -fno-pie -no-pie
        -I "${YARDA_INCLUDE_DIR}" "-D${definition}"
        "${YARDA_SOURCE}" -o "${elf}")
    expect_cli_failure("${case_name}" "${lat}" "${elf}" "${expected_error}")
endfunction()

file(REMOVE_RECURSE "${YARDA_WORK_DIR}")
file(MAKE_DIRECTORY "${YARDA_WORK_DIR}")

run_rejection(local YARDA_CASE_LOCAL
    "non-global storage is outside ELF task analysis")
run_rejection(pointer YARDA_CASE_POINTER
    "pointer-backed storage is outside ELF task analysis")
run_rejection(runtime_index YARDA_CASE_RUNTIME_INDEX
    "runtime-dependent index cannot be resolved")
run_rejection(tls YARDA_CASE_TLS "ELF object symbol is unavailable")
run_rejection(heap YARDA_CASE_HEAP
    "non-global storage is outside ELF task analysis")

set(local_lat "${YARDA_WORK_DIR}/unsupported_local_ape.json")
expect_cli_failure(missing_symbol "${local_lat}" "${YARDA_OTHER_ELF}"
    "ELF object symbol is unavailable")

set(pie "${YARDA_WORK_DIR}/unsupported_pie.elf")
run_checked("PIE fixture" "${YARDA_WORK_DIR}"
    "${YARDA_CLANG}" -O0 -g -fPIE -pie -I "${YARDA_INCLUDE_DIR}"
    -DYARDA_CASE_LOCAL "${YARDA_SOURCE}" -o "${pie}")
expect_cli_failure(pie "${local_lat}" "${pie}"
    "--elf task mapping requires an ET_EXEC image")
