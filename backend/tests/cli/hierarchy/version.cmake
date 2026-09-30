cmake_minimum_required(VERSION 3.20)

file(MAKE_DIRECTORY "${YARDA_WORK_DIR}")
foreach(version unit-release "")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DYARDA_SOURCE_ROOT=${YARDA_WORK_DIR}"
        -DYARDA_RELEASE_VERSION=0.1.0 "-DYARDA_TOOL_VERSION=${version}"
        "-DYARDA_VERSION_OUTPUT=${YARDA_WORK_DIR}/version.hpp"
        -P "${YARDA_VERSION_SCRIPT}" RESULT_VARIABLE status)
    file(READ "${YARDA_WORK_DIR}/version.hpp" header)
    if(version STREQUAL "")
        set(expected 0.1.0)
    else()
        set(expected "${version}")
    endif()
    string(FIND "${header}" "kToolVersion[] = \"${expected}\"" found)
    if(NOT status EQUAL 0 OR found EQUAL -1)
        message(FATAL_ERROR "version override/fallback was not materialized: ${header}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DYARDA_SOURCE_ROOT=${YARDA_WORK_DIR}"
    -DYARDA_RELEASE_VERSION=0.1.0 "-DYARDA_TOOL_VERSION=bad\"version"
    "-DYARDA_VERSION_OUTPUT=${YARDA_WORK_DIR}/version.hpp"
    -P "${YARDA_VERSION_SCRIPT}" RESULT_VARIABLE status ERROR_VARIABLE error)
if(status EQUAL 0 OR NOT error MATCHES "version identifier")
    message(FATAL_ERROR "invalid version was not rejected: ${error}")
endif()
