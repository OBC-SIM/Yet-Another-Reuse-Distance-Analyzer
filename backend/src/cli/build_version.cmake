if(NOT YARDA_TOOL_VERSION STREQUAL "")
    set(YARDA_EFFECTIVE_TOOL_VERSION "${YARDA_TOOL_VERSION}")
else()
    set(YARDA_EFFECTIVE_TOOL_VERSION "${YARDA_RELEASE_VERSION}")
    find_program(YARDA_VERSION_GIT git)
    if(YARDA_VERSION_GIT)
        execute_process(COMMAND "${YARDA_VERSION_GIT}" rev-parse --show-toplevel
            WORKING_DIRECTORY "${YARDA_SOURCE_ROOT}"
            RESULT_VARIABLE status OUTPUT_VARIABLE repository
            ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        get_filename_component(source_root "${YARDA_SOURCE_ROOT}" REALPATH)
        if(status EQUAL 0 AND repository STREQUAL source_root)
            execute_process(COMMAND "${YARDA_VERSION_GIT}" rev-parse HEAD
                WORKING_DIRECTORY "${YARDA_SOURCE_ROOT}"
                RESULT_VARIABLE status OUTPUT_VARIABLE revision
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
            if(NOT status EQUAL 0)
                message(FATAL_ERROR "cannot read source commit")
            endif()
            string(APPEND YARDA_EFFECTIVE_TOOL_VERSION "+git.${revision}")
            execute_process(COMMAND "${YARDA_VERSION_GIT}" status --porcelain
                WORKING_DIRECTORY "${YARDA_SOURCE_ROOT}"
                RESULT_VARIABLE status OUTPUT_VARIABLE changes ERROR_QUIET)
            if(NOT status EQUAL 0)
                message(FATAL_ERROR "cannot inspect source version")
            endif()
            if(NOT changes STREQUAL "")
                string(APPEND YARDA_EFFECTIVE_TOOL_VERSION "-dirty")
            endif()
        endif()
    endif()
endif()
if(NOT YARDA_EFFECTIVE_TOOL_VERSION MATCHES "^[A-Za-z0-9][A-Za-z0-9._+:/@-]*$")
    message(FATAL_ERROR "YARDA_TOOL_VERSION must be a nonempty version identifier")
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/build_version.hpp.in"
    "${YARDA_VERSION_OUTPUT}" @ONLY)
