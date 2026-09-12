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
    cache/mapping/address_test.cpp
    cache/mapping/line_mapping_test.cpp
    cache/mapping/line_mapping_json_test.cpp
    cache/rd/lru_rd_analysis_test.cpp
    cache/rd/lru_rd_integration_test.cpp
    trace/module/calls_test.cpp
    reuse/profile_test.cpp
    trace/layout/access_layout_test.cpp
    trace/module/schema_test.cpp
    trace/execution/expansion_budget_test.cpp
    trace/output/mapped_trace_test.cpp
    trace/output/mapped_trace_strict_test.cpp
    trace/output/resolved_access_test.cpp
    trace/output/resolved_mapping_test.cpp
    trace/output/task_access_stream_test.cpp
    trace/module/task_scope_test.cpp
    trace/output/task_access_stream_failure_test.cpp
    trace/output/task_access_stream_budget_test.cpp
    trace/execution/work_limits_test.cpp
    trace/execution/work_limits_default_test.cpp
    trace/execution/work_limits_failure_test.cpp
    trace/execution/work_limits_guard_test.cpp
    trace/execution/work_limits_budget_test.cpp
    trace/index/prepared_index_test.cpp
    trace/index/affine_evaluation_test.cpp
    trace/index/affine_expression_test.cpp
    trace/index/affine_failure_test.cpp
    trace/access/prepared_access_test.cpp
    trace/access/prepared_access_binding_test.cpp
    trace/access/prepared_access_failure_test.cpp
    trace/layout/prepared_layout_failure_test.cpp
    trace/execution/prepared_trace_test.cpp
    trace/execution/prepared_trace_stream_test.cpp
    trace/execution/prepared_trace_failure_test.cpp
    trace/output/resolved_mapping_stream_test.cpp
    trace/access/strict_input_test.cpp
    trace/access/strict_resolution_test.cpp
    trace/execution/strict_unroller_test.cpp
    trace/output/task_exclusion_test.cpp
    trace/output/task_mapping_json_test.cpp
    trace/output/task_trace_test.cpp
    trace/output/task_trace_validation_test.cpp
    trace/execution/trace_test.cpp
)
target_include_directories(yarda_backend_tests PRIVATE
    "${YARDA_BACKEND_SOURCE_DIR}/src"
    "${YARDA_BACKEND_SOURCE_DIR}/tests"
)
target_link_libraries(yarda_backend_tests PRIVATE yarda_backend GTest::gtest_main)
gtest_discover_tests(yarda_backend_tests
    WORKING_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
)

add_subdirectory(cache)
include(trace/integration/affine_index_tests.cmake)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(
        yarda_backend_tests
        yarda_hierarchy_analysis_tests
        yarda_cache_config_tests
        yarda_elf_regions_tests
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
    )
endif()
