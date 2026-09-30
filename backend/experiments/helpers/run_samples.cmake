include("${CMAKE_CURRENT_LIST_DIR}/json.cmake")

function(run_evaluation case_file mode destination returned)
    if(EXISTS "${destination}")
        message(FATAL_ERROR "sample directory already exists: ${destination}")
    endif()
    execute_process(COMMAND sh "${YARDA_EXPERIMENTS}/helpers/limit_memory.sh"
        "${YARDA_MEMORY_KIB}" "${YARDA_EVALUATOR}" "${case_file}" "${mode}" "${destination}"
        RESULT_VARIABLE exit_status OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
        TIMEOUT "${YARDA_TIMEOUT}")
    file(MAKE_DIRECTORY "${destination}")
    file(WRITE "${destination}/command.log"
        "sh;limit_memory.sh;${YARDA_MEMORY_KIB};${YARDA_EVALUATOR};${case_file};${mode};${destination}\nexit=${exit_status}\n${stdout}\n${stderr}\n")
    if(NOT EXISTS "${destination}/measurement.json")
        if("${exit_status}" MATCHES "timeout")
            set(status timeout)
        else()
            set(status error)
        endif()
        file(READ "${case_file}" row)
        string(JSON id GET "${row}" case_id)
        set(measurement "{}")
        foreach(key case_id mode status)
            if(key STREQUAL "case_id")
                set(value "${id}")
            else()
                set(value "${${key}}")
            endif()
            set_json_string(measurement "${measurement}" "${key}" "${value}")
        endforeach()
        set_json_string(measurement "${measurement}" error "${exit_status}: ${stderr}")
        file(WRITE "${destination}/measurement.json" "${measurement}\n")
    else()
        file(READ "${destination}/measurement.json" measurement)
        string(JSON status GET "${measurement}" status)
        if((status STREQUAL "success" AND NOT exit_status STREQUAL "0") OR
           (NOT status STREQUAL "success" AND NOT exit_status STREQUAL "1"))
            message(FATAL_ERROR "measurement and exit status disagree: ${destination}")
        endif()
    endif()
    if(NOT status STREQUAL "success" AND EXISTS "${destination}/result.json")
        message(FATAL_ERROR "failed analysis published a RESULT: ${destination}")
    endif()
    set(${returned} "${status}" PARENT_SCOPE)
endfunction()

function(compare_result expected actual)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${expected}" "${actual}"
        RESULT_VARIABLE status)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "RESULT bytes differ: ${expected} / ${actual}")
    endif()
endfunction()

function(verify_cli case_file directory)
    file(READ "${case_file}" row)
    foreach(key map_path elf_path cache_path)
        string(JSON ${key} GET "${row}" "${key}")
    endforeach()
    foreach(key single_loop cumulative_loop source_accesses line_references)
        string(JSON ${key} GET "${row}" effective_work_limits "${key}")
    endforeach()
    execute_process(COMMAND sh "${YARDA_EXPERIMENTS}/helpers/limit_memory.sh"
        "${YARDA_MEMORY_KIB}" "${YARDA_CPP}" "${map_path}" --analysis hierarchy-rd
        --elf "${elf_path}" --cache "${cache_path}" --export "${directory}/cli-result.json"
        --max-single-loop-iterations "${single_loop}"
        --max-cumulative-loop-iterations "${cumulative_loop}"
        --max-source-accesses "${source_accesses}" --max-line-references "${line_references}"
        RESULT_VARIABLE status OUTPUT_FILE "${directory}/cli.stdout"
        ERROR_FILE "${directory}/cli.stderr" TIMEOUT "${YARDA_TIMEOUT}")
    file(WRITE "${directory}/cli-command.log"
        "${YARDA_CPP};${map_path};--analysis;hierarchy-rd;--elf;${elf_path};--cache;${cache_path};--export;${directory}/cli-result.json;--max-single-loop-iterations;${single_loop};--max-cumulative-loop-iterations;${cumulative_loop};--max-source-accesses;${source_accesses};--max-line-references;${line_references}\nexit=${status}\n")
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "actual CLI failed: ${case_file}")
    endif()
    compare_result("${directory}/result.json" "${directory}/cli-result.json")
endfunction()

macro(append_sample path)
    json_quote(encoded "${path}")
    string(JSON count LENGTH "${sample_index}")
    string(JSON sample_index SET "${sample_index}" "${count}" "${encoded}")
endmacro()
