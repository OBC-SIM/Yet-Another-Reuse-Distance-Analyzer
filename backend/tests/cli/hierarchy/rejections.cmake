cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
set(diagnostics --export-events "${events_file}" --event-limit 8
    --telemetry "${telemetry_file}")
file(READ "${map}" original)
foreach(version 1 3 "\"2\"")
    string(JSON invalid SET "${original}" schema_version "${version}")
    file(WRITE "${map}" "${invalid}")
    run_error("schema_version 2" ${base} ${diagnostics})
endforeach()
string(JSON invalid REMOVE "${original}" schema_version)
file(WRITE "${map}" "${invalid}")
run_error("schema_version 2" ${base} ${diagnostics})
string(JSON invalid SET "${original}" functions "[]")
file(WRITE "${map}" "${invalid}")
run_error("task|root" ${base} ${diagnostics})
string(REPLACE "global::yarda_mapped_value" "global::missing" invalid "${original}")
file(WRITE "${map}" "${invalid}")
run_error("symbol.*unavailable" ${base} ${diagnostics})
file(WRITE "${map}" "${original}")
run_error("ET_EXEC" ${base} --elf "${YARDA_PIE}" ${diagnostics})
run_error("object file" ${base} --elf "${map}" ${diagnostics})
run_error("open.*SHA|open.*hash|open.*input" ${base}
    --elf "${YARDA_WORK_DIR}/missing.elf" ${diagnostics})

file(READ "${YARDA_CACHE}" cache)
string(REPLACE "replacement: LRU" "replacement: FIFO" invalid_cache "${cache}")
file(WRITE "${YARDA_WORK_DIR}/invalid.yaml" "${invalid_cache}")
run_error("LRU" ${base} --cache "${YARDA_WORK_DIR}/invalid.yaml" ${diagnostics})
string(REPLACE "write_allocate: true" "write_allocate: false" invalid_cache "${cache}")
file(WRITE "${YARDA_WORK_DIR}/invalid.yaml" "${invalid_cache}")
run_error("write_allocate|allocation" ${base} --cache "${YARDA_WORK_DIR}/invalid.yaml" ${diagnostics})

foreach(flag --export --export-events --telemetry)
    run_error("export path.*missing" ${base} ${diagnostics}
        ${flag} "${YARDA_WORK_DIR}/missing/export.json")
endforeach()
run_error("duplicate.*output|output.*alias" ${base} --export-events "${result_file}")
# A failure must not truncate input files, including a path reused for RESULT.
execute_process(COMMAND "${YARDA_CPP}" ${base} --export "${map}"
    RESULT_VARIABLE status ERROR_VARIABLE error OUTPUT_QUIET)
file(READ "${map}" after)
if(NOT status STREQUAL "1" OR NOT error MATCHES "input.*output|output.*input" OR
   NOT after STREQUAL original)
    message(FATAL_ERROR "input alias was not rejected without mutation: ${error}")
endif()
