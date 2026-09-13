foreach(case result options limits diagnostics rejections)
    add_test(NAME yarda_cpp_hierarchy_${case}
        COMMAND ${CMAKE_COMMAND}
            "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
            "-DYARDA_ELF=${YARDA_ELF_EXEC_FIXTURE}"
            "-DYARDA_PIE=${YARDA_ELF_PIE_FIXTURE}"
            "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
            "-DYARDA_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/${case}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/${case}.cmake"
    )
endforeach()

add_executable(yarda_cli_tests
    artifact_output_test.cpp artifact_write_failure_test.cpp
    command_test.cpp command_publication_test.cpp options_test.cpp
)
add_dependencies(yarda_cli_tests yarda_elf_region_fixtures)
target_include_directories(yarda_cli_tests PRIVATE
    "${YARDA_BACKEND_SOURCE_DIR}/src"
    "${YARDA_BACKEND_SOURCE_DIR}/tests"
    "${YARDA_BACKEND_BINARY_DIR}/src/cli-generated"
)
target_link_libraries(yarda_cli_tests PRIVATE yarda_cli_support GTest::gtest_main)
target_compile_definitions(yarda_cli_tests PRIVATE
    YARDA_ELF_EXEC_FIXTURE_PATH="${YARDA_ELF_EXEC_FIXTURE}"
    YARDA_ELF_PIE_FIXTURE_PATH="${YARDA_ELF_PIE_FIXTURE}"
    YARDA_CLI_CACHE_PATH="${YARDA_CPP_CACHE_32B}"
)
target_compile_options(yarda_cli_tests PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(yarda_cli_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
)
gtest_discover_tests(yarda_cli_tests WORKING_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}")

add_test(NAME yarda_cpp_hierarchy_build_version
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/version"
        "-DYARDA_VERSION_SCRIPT=${YARDA_BACKEND_SOURCE_DIR}/src/cli/build_version.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/version.cmake"
)
