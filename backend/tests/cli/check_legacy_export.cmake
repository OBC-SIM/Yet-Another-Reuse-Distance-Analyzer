foreach(required YARDA_CPP YARDA_INPUT YARDA_OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE "${YARDA_OUTPUT}")
execute_process(
    COMMAND "${YARDA_CPP}" "${YARDA_INPUT}" --mode unroll
            --export "${YARDA_OUTPUT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "legacy export failed (${result})\nstdout:\n${output}\nstderr:\n${error_output}")
endif()
if(NOT EXISTS "${YARDA_OUTPUT}")
    message(FATAL_ERROR "legacy export did not create ${YARDA_OUTPUT}")
endif()

file(READ "${YARDA_OUTPUT}" payload)
string(JSON mode GET "${payload}" mode)
string(JSON granularity GET "${payload}" granularity)
string(JSON cold_misses GET "${payload}" program cold_misses)
string(JSON block_count LENGTH "${payload}" blocks)
if(NOT mode STREQUAL "unroll" OR
   NOT granularity STREQUAL "element" OR
   NOT cold_misses STREQUAL "1" OR
   NOT block_count STREQUAL "1")
    message(FATAL_ERROR "unexpected legacy export payload: ${payload}")
endif()
