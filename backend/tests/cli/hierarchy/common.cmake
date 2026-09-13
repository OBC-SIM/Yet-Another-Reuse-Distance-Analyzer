foreach(required YARDA_CPP YARDA_ELF YARDA_CACHE YARDA_WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(MAKE_DIRECTORY "${YARDA_WORK_DIR}")
set(lat "${YARDA_WORK_DIR}/input.json")
set(result_file "${YARDA_WORK_DIR}/result.json")
set(events_file "${YARDA_WORK_DIR}/events.json")
set(telemetry_file "${YARDA_WORK_DIR}/telemetry.json")
file(WRITE "${lat}" [=[{
  "schema_version": 2,
  "metadata": {"objects": {"global::yarda_mapped_value": {
    "kind": "array", "shape": [1], "elem_size": 8
  }}},
  "functions": [
    {"function": "first", "annotations": ["ape.analyze"], "body": [
      {"type": "Loop", "var": "i", "start": 0, "bound": 2, "step": 1,
       "body": [{"type": "Array", "object": "global::yarda_mapped_value",
                 "indices": ["0"], "op": "store"}]}
    ]},
    {"function": "second", "annotations": ["ape.analyze"], "body": [
      {"type": "Loop", "var": "i", "start": 0, "bound": 2, "step": 1,
       "body": [{"type": "Array", "object": "global::yarda_mapped_value",
                 "indices": ["0"], "op": "load"}]}
    ]},
    {"function": "empty", "annotations": ["ape.analyze"], "body": []}
  ]
}
]=])
set(base "${lat}" --analysis hierarchy-rd --elf "${YARDA_ELF}"
    --cache "${YARDA_CACHE}" --export "${result_file}")

function(run_ok)
    execute_process(COMMAND "${YARDA_CPP}" ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "CLI failed (${status}): ${error}\n${output}")
    endif()
endfunction()

function(run_error expected)
    file(REMOVE "${result_file}" "${events_file}" "${telemetry_file}")
    execute_process(COMMAND "${YARDA_CPP}" ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT status STREQUAL "1" OR NOT error MATCHES "${expected}")
        message(FATAL_ERROR "expected '${expected}', status=${status}: ${error}")
    endif()
    foreach(path "${result_file}" "${events_file}" "${telemetry_file}")
        if(EXISTS "${path}")
            message(FATAL_ERROR "failed invocation published ${path}")
        endif()
    endforeach()
endfunction()

function(assert_json expected)
    string(JSON actual ERROR_VARIABLE error GET "${payload}" ${ARGN})
    if(error OR NOT actual STREQUAL expected)
        message(FATAL_ERROR "${ARGN}: expected '${expected}', got '${actual}': ${error}")
    endif()
endfunction()
