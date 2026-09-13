add_executable(yarda_cli_tests
    artifact_output_test.cpp artifact_write_failure_test.cpp
)
target_include_directories(yarda_cli_tests PRIVATE
    "${YARDA_BACKEND_SOURCE_DIR}/src"
    "${YARDA_BACKEND_SOURCE_DIR}/tests"
)
target_link_libraries(yarda_cli_tests PRIVATE yarda_cli_support GTest::gtest_main)
target_compile_options(yarda_cli_tests PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(yarda_cli_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
)
gtest_discover_tests(yarda_cli_tests WORKING_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}")
