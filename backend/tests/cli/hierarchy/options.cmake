cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
run_error("unknown analysis" "${lat}" --analysis invalid)
foreach(flag --elf --cache --export)
    set(arguments ${base})
    list(FIND arguments "${flag}" position)
    list(REMOVE_AT arguments ${position})
    list(REMOVE_AT arguments ${position})
    run_error("${flag}.*required" ${arguments})
endforeach()
run_error("LAT input path is required" --analysis hierarchy-rd)
run_error("--event-limit.*--export-events" ${base} --event-limit 0)
run_error("hierarchy-rd" "${lat}" --telemetry "${telemetry_file}")
run_error("hierarchy-rd" "${lat}" --analysis mapping --elf "${YARDA_ELF}"
    --cache "${YARDA_CACHE}" --export-events "${events_file}")
run_error("--elf.*required" "${lat}" --analysis mapping --cache "${YARDA_CACHE}")
run_error("unknown option" ${base} --core 1)
foreach(flag --max-single-loop-iterations --max-cumulative-loop-iterations
             --max-source-accesses --max-line-references --event-limit)
    foreach(value -1 +1 1x 1.0 18446744073709551616)
        run_error("${flag}.*unsigned" ${base} --export-events "${events_file}"
            ${flag} "${value}")
    endforeach()
    run_error("${flag} requires a value" ${base} ${flag})
endforeach()
run_error("hierarchy-rd" "${lat}" --max-source-accesses 0)
run_ok(${base} --export-events "${events_file}" --event-limit 0 --event-limit 1)
file(READ "${events_file}" payload)
assert_json("1" event_limit)
# The old and explicit mapping dispatches must produce the same bytes.
run_ok("${lat}" --elf "${YARDA_ELF}" --cache "${YARDA_CACHE}"
    --export "${YARDA_WORK_DIR}/implicit.json")
run_ok("${lat}" --analysis mapping --elf "${YARDA_ELF}" --cache "${YARDA_CACHE}"
    --export "${YARDA_WORK_DIR}/explicit.json")
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${YARDA_WORK_DIR}/implicit.json" "${YARDA_WORK_DIR}/explicit.json"
    RESULT_VARIABLE mismatch)
if(mismatch)
    message(FATAL_ERROR "explicit mapping changed legacy bytes")
endif()
