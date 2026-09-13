function(run_checked label)
    execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${case_dir}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(APPEND "${case_dir}/commands.log"
        "${label}: ${ARGN}\nexit=${status}\n${output}\n${error}\n")
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${label}: exit=${status}\n${error}\n${output}")
    endif()
endfunction()

function(compare_files first second)
    run_checked("byte comparison" "${CMAKE_COMMAND}" -E compare_files
        "${first}" "${second}")
endfunction()
