find_package(GTest REQUIRED)
include(GoogleTest)

# ELF tests and CLI tests consume the same generated images.
set(YARDA_ELF_PIE_FIXTURE ${YARDA_BACKEND_BINARY_DIR}/yarda_elf_fixture_pie)
set(YARDA_ELF_EXEC_FIXTURE ${YARDA_BACKEND_BINARY_DIR}/yarda_elf_fixture_exec)
set(YARDA_ELF_RELOCATABLE_FIXTURE ${YARDA_BACKEND_BINARY_DIR}/yarda_elf_fixture.o)
set(YARDA_ELF_SECTIONLESS_FIXTURE
    ${YARDA_BACKEND_BINARY_DIR}/yarda_elf_fixture_sectionless)

if(TARGET yarda_region_lat)
    add_subdirectory(regions)
endif()

add_subdirectory(elf)
add_subdirectory(cli)

add_executable(yarda_backend_tests
    cache/address_test.cpp
    cache/line_mapping_test.cpp
    cache/line_mapping_json_test.cpp
    cache/lru_rd_analysis_test.cpp
    cache/lru_rd_integration_test.cpp
    trace/calls_test.cpp
    reuse/profile_test.cpp
    trace/access_layout_test.cpp
    trace/schema_test.cpp
    trace/expansion_budget_test.cpp
    trace/mapped_trace_test.cpp
    trace/mapped_trace_strict_test.cpp
    trace/resolved_access_test.cpp
    trace/resolved_mapping_test.cpp
    trace/task_access_stream_test.cpp
    trace/task_scope_test.cpp
    trace/task_access_stream_failure_test.cpp
    trace/task_access_stream_budget_test.cpp
    trace/work_limits_test.cpp
    trace/work_limits_default_test.cpp
    trace/work_limits_failure_test.cpp
    trace/work_limits_guard_test.cpp
    trace/work_limits_budget_test.cpp
    trace/prepared_index_test.cpp
    trace/prepared_trace_test.cpp
    trace/prepared_trace_stream_test.cpp
    trace/prepared_trace_failure_test.cpp
    trace/resolved_mapping_stream_test.cpp
    trace/strict_input_test.cpp
    trace/strict_resolution_test.cpp
    trace/strict_unroller_test.cpp
    trace/task_exclusion_test.cpp
    trace/task_mapping_json_test.cpp
    trace/task_trace_test.cpp
    trace/task_trace_validation_test.cpp
    trace/trace_test.cpp
)
target_link_libraries(yarda_backend_tests PRIVATE yarda_backend GTest::gtest_main)
gtest_discover_tests(yarda_backend_tests
    WORKING_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
)

add_subdirectory(cache)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(
        yarda_backend_tests
        yarda_hierarchy_analysis_tests
        yarda_cache_config_tests
        yarda_elf_regions_tests
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
    )
endif()
