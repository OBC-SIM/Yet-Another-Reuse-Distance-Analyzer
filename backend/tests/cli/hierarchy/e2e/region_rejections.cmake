cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/helpers/generate.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/frontend_failure.cmake")
set(source "${CMAKE_CURRENT_LIST_DIR}/fixtures/region_rejections.c")
foreach(entry
        "MISSING_END|expected one ordered begin/end pair per function"
        "REVERSED|expected one ordered begin/end pair per function"
        "NESTED|expected one ordered begin/end pair per function"
        "PARTIAL_LOOP|region boundaries must be top-level complete statements"
        "INLINE_REGION|region conflicts with inline annotation"
        "SCALED|unsupported scaled region index"
        "RUNTIME_BOUND|unresolved or unsupported region loop bound"
        "RUNTIME_INDEX|unresolved or unsupported region index"
        "CONTROL_FLOW|unsupported control flow in selected body"
        "RETURN_INSIDE|unsupported control flow in selected body")
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 kind)
    list(GET fields 1 reason)
    prepare_case("${kind}")
    expect_frontend_failure("yarda_region_lat: ${reason}(\n|$)"
        "${YARDA_REGION}" "${source}" "${lat}"
        -- -I "${YARDA_INCLUDE_DIR}" "-D${kind}")
endforeach()
prepare_case(pipeline)
expect_frontend_failure("unsupported region compiler option"
    "${YARDA_REGION}" "${source}" "${lat}" -- -O2)

generate_region(opaque_region "${source}" -DOPAQUE_REGION)
expect_analysis_failure("error: hierarchy task has opaque-call exclusions:"
    ${base} ${diagnostics})
generate_region(scope "${CMAKE_CURRENT_LIST_DIR}/fixtures/regions.c")
expect_analysis_failure("legacy module expansion rejects analysis_scope"
    "${lat}" --mode unroll --export "${result_file}")
file(READ "${lat}" raw)
string(JSON count LENGTH "${raw}" functions)
math(EXPR last "${count}-1")
set(found OFF)
foreach(index RANGE 0 ${last})
    string(JSON name GET "${raw}" functions ${index} function)
    if(name STREQUAL "selected")
        string(JSON invalid SET "${raw}" functions ${index} analysis_scope name
            "\"missing-region\"")
        set(found ON)
    endif()
endforeach()
if(NOT found)
    message(FATAL_ERROR "selected function missing from region LAT")
endif()
file(WRITE "${lat}" "${invalid}")
expect_analysis_failure("error: invalid analysis_scope: expected APE_ANALYZE region(\n|$)"
    ${base} ${diagnostics})
generate_region(opaque_function
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/rejections.c" -DOPAQUE)
expect_analysis_failure("error: hierarchy task has opaque-call exclusions:"
    ${base} ${diagnostics})
