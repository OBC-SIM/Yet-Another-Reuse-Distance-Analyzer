cmake_minimum_required(VERSION 3.20)

foreach(required YARDA_CPP YARDA_ELF YARDA_CACHE YARDA_MODE YARDA_WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
file(MAKE_DIRECTORY "${YARDA_WORK_DIR}")
set(map "${YARDA_WORK_DIR}/input.json")
set(result_file "${YARDA_WORK_DIR}/result.json")
set(raw [=[{
  "schema_version": 2,
  "metadata": {"objects": {"global::yarda_mapped_value": {
    "kind": "array", "shape": [1], "elem_size": 8
  }}},
  "functions": [
    {"function": "first", "annotations": ["ape.analyze"], "body": [
      {"type": "Loop", "var": "i", "start": 0, "bound": 2, "step": 1,
       "body": [
        {"type": "Loop", "var": "j", "start": 0, "bound": 3, "step": 1,
         "body": [{"type": "Array", "name": "yarda_mapped_value",
                   "object": "global::yarda_mapped_value", "indices": ["0"],
                   "op": "load"}]}
       ]}
    ]},
    {"function": "second", "annotations": ["ape.analyze"], "body": [
      {"type": "Loop", "var": "i", "start": 0, "bound": 2, "step": 1,
       "body": [
        {"type": "Loop", "var": "j", "start": 0, "bound": 3, "step": 1,
         "body": [{"type": "Array", "name": "yarda_mapped_value",
                   "object": "global::yarda_mapped_value", "indices": ["0"],
                   "op": "store"}]}
       ]}
    ]}
  ]
}]=])
file(WRITE "${map}" "${raw}")

set(mode_options --mode unroll)
if(YARDA_MODE STREQUAL "cache-line")
    list(APPEND mode_options --granularity cache-line --cache "${YARDA_CACHE}")
elseif(YARDA_MODE STREQUAL "implicit-mapping" OR YARDA_MODE STREQUAL "mapping")
    list(APPEND mode_options --elf "${YARDA_ELF}" --cache "${YARDA_CACHE}")
    if(YARDA_MODE STREQUAL "mapping")
        list(APPEND mode_options --analysis mapping)
    endif()
endif()

function(run_ok)
    execute_process(COMMAND "${YARDA_CPP}" "${map}" ${mode_options}
        --export "${result_file}" ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "CLI failed (${status}): ${error}\n${output}")
    endif()
endfunction()

function(run_error expected)
    file(READ "${result_file}" before)
    execute_process(COMMAND "${YARDA_CPP}" "${map}" ${mode_options}
        --export "${result_file}" ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT status STREQUAL "1" OR NOT error MATCHES "${expected}")
        message(FATAL_ERROR "expected '${expected}', status=${status}: ${error}")
    endif()
    file(READ "${result_file}" after)
    if(NOT before STREQUAL after)
        message(FATAL_ERROR "failed invocation changed the existing export")
    endif()
endfunction()

run_ok()
file(READ "${result_file}" baseline)
# Two tasks each reserve 2 + 2*3 iterations, including the outer loops.
run_ok(--max-single-loop-iterations 3 --max-cumulative-loop-iterations 16)
file(READ "${result_file}" bounded)
if(NOT baseline STREQUAL bounded)
    message(FATAL_ERROR "loop overrides changed successful output bytes")
endif()
run_error("loop iteration count exceeds 2"
    --max-single-loop-iterations 2 --max-cumulative-loop-iterations 16)
run_error("cumulative loop iteration count exceeds 15"
    --max-single-loop-iterations 3 --max-cumulative-loop-iterations 15)
run_error("loop iteration count exceeds 0" --max-single-loop-iterations 0)
run_error("cumulative loop iteration count exceeds 0"
    --max-cumulative-loop-iterations 0)
run_error("require --analysis hierarchy-rd" --max-source-accesses 1)
run_error("require --analysis hierarchy-rd" --max-line-references 1)

# An empty body exercises raising both ceilings without materializing a large trace.
string(JSON wide SET "${raw}" functions [=[[
  {"function": "wide", "annotations": ["ape.analyze"], "body": [
    {"type": "Loop", "var": "i", "start": 0, "bound": 1000001,
     "step": 1, "body": []}
  ]}
]]=])
file(WRITE "${map}" "${wide}")
run_error("loop iteration count exceeds 1000000")
run_ok(--max-single-loop-iterations 1000001
    --max-cumulative-loop-iterations 1000001)
