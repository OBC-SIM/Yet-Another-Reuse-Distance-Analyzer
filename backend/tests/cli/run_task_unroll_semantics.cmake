foreach(required YARDA_CLANG YARDA_OPT YARDA_PLUGIN YARDA_CPP YARDA_SOURCE
                 YARDA_INCLUDE_DIR YARDA_CACHE YARDA_CASE YARDA_GRANULARITY
                 YARDA_WORK_DIR)
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

function(assert_json expected)
    string(JSON actual ERROR_VARIABLE json_error GET "${payload}" ${ARGN})
    if(json_error)
        string(JOIN "." path ${ARGN})
        message(FATAL_ERROR "cannot read ${path}: ${json_error}")
    endif()
    if(NOT actual STREQUAL expected)
        string(JOIN "." path ${ARGN})
        message(FATAL_ERROR
            "unexpected ${path}: expected '${expected}', got '${actual}'")
    endif()
endfunction()

function(assert_json_length expected)
    string(JSON actual ERROR_VARIABLE json_error LENGTH "${payload}" ${ARGN})
    if(json_error)
        string(JOIN "." path ${ARGN})
        message(FATAL_ERROR "cannot read length of ${path}: ${json_error}")
    endif()
    if(NOT actual STREQUAL expected)
        string(JOIN "." path ${ARGN})
        message(FATAL_ERROR
            "unexpected ${path} length: expected ${expected}, got ${actual}")
    endif()
endfunction()

function(assert_profile cold_misses total_reuses histogram_size)
    set(path ${ARGN})
    assert_json("${cold_misses}" ${path} cold_misses)
    assert_json("${total_reuses}" ${path} total_reuses)
    assert_json_length("${histogram_size}" ${path} histogram)
endfunction()

file(REMOVE_RECURSE "${YARDA_WORK_DIR}")
file(MAKE_DIRECTORY "${YARDA_WORK_DIR}")

set(ir "${YARDA_WORK_DIR}/${YARDA_CASE}.ll")
set(lat "${YARDA_WORK_DIR}/${YARDA_CASE}_ape.json")
set(first_result "${YARDA_WORK_DIR}/${YARDA_CASE}_first.json")
set(second_result "${YARDA_WORK_DIR}/${YARDA_CASE}_second.json")

run_checked("C to LLVM IR" "${YARDA_WORK_DIR}"
    "${YARDA_CLANG}" -O0 -Xclang -disable-O0-optnone -g
    -I "${YARDA_INCLUDE_DIR}" -emit-llvm -S "${YARDA_SOURCE}" -o "${ir}")
run_checked("LLVM IR to APE/LAT" "${YARDA_WORK_DIR}"
    "${YARDA_OPT}" -load-pass-plugin "${YARDA_PLUGIN}"
    "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
    "${ir}" -o /dev/null)

file(READ "${lat}" lat_payload)
string(FIND "${lat_payload}" "\"ape.analyze\"" analyze_position)
if(analyze_position EQUAL -1)
    message(FATAL_ERROR "generated LAT has no ape.analyze root")
endif()
string(FIND "${lat_payload}" "\"yard." yard_position)
if(NOT yard_position EQUAL -1)
    message(FATAL_ERROR "generated LAT unexpectedly contains yard annotation")
endif()

set(cli_arguments
    "${lat}" --mode unroll --granularity "${YARDA_GRANULARITY}")
if(YARDA_GRANULARITY STREQUAL "cache-line")
    list(APPEND cli_arguments --cache "${YARDA_CACHE}")
endif()

run_checked("first yarda_cpp unroll" "${YARDA_WORK_DIR}"
    "${YARDA_CPP}" ${cli_arguments} --export "${first_result}")
run_checked("second yarda_cpp unroll" "${YARDA_WORK_DIR}"
    "${YARDA_CPP}" ${cli_arguments} --export "${second_result}")

file(READ "${first_result}" payload)
file(READ "${second_result}" rerun_payload)
if(NOT payload STREQUAL rerun_payload)
    message(FATAL_ERROR "unroll output is not deterministic")
endif()

assert_json("unroll" mode)
assert_json("${YARDA_GRANULARITY}" granularity)
if(YARDA_GRANULARITY STREQUAL "cache-line")
    assert_json("32" cache_line_size)
endif()

if(YARDA_CASE STREQUAL "call_element")
    assert_profile("16" "16" "1" program)
    assert_json("16" program histogram 0)
    assert_json_length("1" blocks)
    assert_profile("16" "16" "1" blocks 0 profile)
    assert_json("16" blocks 0 profile histogram 0)
elseif(YARDA_CASE STREQUAL "constant_access_element")
    assert_profile("3" "5" "3" program)
    assert_json("2" program histogram 0)
    assert_json("2" program histogram 1)
    assert_json("1" program histogram 2)
    assert_json_length("1" blocks)
    assert_profile("3" "5" "3" blocks 0 profile)
elseif(YARDA_CASE STREQUAL "constant_access_cache_line")
    assert_profile("1" "7" "1" program)
    assert_json("7" program histogram 0)
    assert_json_length("1" blocks)
    assert_profile("1" "7" "1" blocks 0 profile)
    assert_json("7" blocks 0 profile histogram 0)
elseif(YARDA_CASE STREQUAL "stencil_cache_line")
    assert_profile("26" "366" "3" program)
    assert_json("172" program histogram 0)
    assert_json("146" program histogram 1)
    assert_json("48" program histogram 2)
elseif(YARDA_CASE STREQUAL "regular_block_element")
    assert_profile("98" "100" "3" program)
    assert_json("98" program histogram 0)
    assert_json("1" program histogram 32)
    assert_json("1" program histogram 64)
    assert_json_length("5" blocks)
    assert_profile("1" "0" "0" blocks 0 profile)
    assert_profile("32" "32" "1" blocks 1 profile)
    assert_json("32" blocks 1 profile histogram 0)
    assert_profile("2" "1" "1" blocks 2 profile)
    assert_json("1" blocks 2 profile histogram 0)
    assert_profile("64" "64" "1" blocks 3 profile)
    assert_json("64" blocks 3 profile histogram 0)
    assert_profile("1" "1" "1" blocks 4 profile)
    assert_json("1" blocks 4 profile histogram 0)
else()
    message(FATAL_ERROR "unknown semantic test case: ${YARDA_CASE}")
endif()
