add_library(yarda_backend
    src/cache/address.cpp
    src/cache/line_mapping.cpp
    src/cache/line_mapping_json.cpp
    src/cache/lru_rd_analysis.cpp
    src/trace/calls.cpp
    src/trace/call_substitution.cpp
    src/reuse/profile.cpp
    src/trace/schema.cpp
    src/trace/trace.cpp
    src/trace/access_layout.cpp
    src/trace/access_layout_legacy.cpp
    src/trace/access_layout_structured.cpp
    src/trace/access_resolver.cpp
    src/trace/cache_line.cpp
    src/trace/mapped_trace.cpp
    src/trace/resolved_access.cpp
    src/trace/resolved_mapping.cpp
    src/trace/emission_budget.cpp
    src/trace/task_access_stream.cpp
    src/trace/task_scope.cpp
    src/trace/task_access_support.cpp
    src/trace/unroller.cpp
    src/trace/task_mapping_json.cpp
)

target_include_directories(yarda_backend PUBLIC include)
target_link_libraries(yarda_backend PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(yarda_backend PRIVATE -Wall -Wextra -Wpedantic)

find_package(yaml-cpp 0.7 REQUIRED CONFIG)
add_library(yarda_cache_config
    src/cache/cache_config.cpp
    src/cache/yaml_config_parser.cpp
)
target_include_directories(yarda_cache_config PUBLIC include)
target_compile_options(yarda_cache_config PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(yarda_cache_config
    PUBLIC yarda_backend
    PRIVATE yaml-cpp
)

add_library(yarda_hierarchy_analysis
    src/cache/streaming_hierarchy.cpp
    src/cache/streaming_hierarchy_summary.cpp
    src/cache/streaming_hierarchy_task.cpp
    src/cache/exact_csrd.cpp
    src/cache/recency_index.cpp
    src/cache/hierarchy_model.cpp
    src/cache/hierarchy_analysis.cpp
    src/cache/hierarchy_aggregation.cpp
    src/cache/hierarchy_service_summary.cpp
    src/cache/hierarchy_level_analysis.cpp
    src/cache/hierarchy_input_validation.cpp
)
target_link_libraries(yarda_hierarchy_analysis PUBLIC yarda_cache_config)
target_compile_options(yarda_hierarchy_analysis PRIVATE -Wall -Wextra -Wpedantic)

find_package(LLVM 14 REQUIRED CONFIG)
llvm_map_components_to_libnames(YARDA_ELF_LLVM_LIBS object support)

add_library(yarda_elf_regions
    src/elf/data_regions.cpp
    src/elf/data_regions_json.cpp
    src/elf/object_addresses.cpp
    src/elf/program_regions.cpp
)
target_include_directories(yarda_elf_regions
    PUBLIC include
)
target_include_directories(yarda_elf_regions SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_options(yarda_elf_regions PRIVATE
    -Wall -Wextra -Wpedantic
)
target_link_libraries(yarda_elf_regions
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE ${YARDA_ELF_LLVM_LIBS}
)

add_executable(yarda_elf_regions_cli src/cli/elf_regions_main.cpp)
set_target_properties(yarda_elf_regions_cli PROPERTIES OUTPUT_NAME yarda_elf_regions)
target_link_libraries(yarda_elf_regions_cli PRIVATE yarda_elf_regions)

add_executable(yarda_cpp src/cli/main.cpp)
target_link_libraries(yarda_cpp PRIVATE yarda_cache_config yarda_elf_regions)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    if(TARGET yarda_region_lat)
        add_subdirectory(tests/regions)
    endif()

    find_program(YARDA_CLANGXX_EXECUTABLE NAMES clang++-14 REQUIRED)
    find_program(YARDA_LLVM_OBJCOPY_EXECUTABLE NAMES llvm-objcopy-14 REQUIRED)
    set(YARDA_ELF_FIXTURE_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/elf/data_fixture.cpp)
    set(YARDA_ELF_PIE_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/yarda_elf_fixture_pie)
    set(YARDA_ELF_EXEC_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/yarda_elf_fixture_exec)
    set(YARDA_ELF_RELOCATABLE_FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/yarda_elf_fixture.o)
    set(YARDA_ELF_SECTIONLESS_FIXTURE
        ${CMAKE_CURRENT_BINARY_DIR}/yarda_elf_fixture_sectionless)

    add_custom_command(
        OUTPUT ${YARDA_ELF_PIE_FIXTURE}
        COMMAND ${YARDA_CLANGXX_EXECUTABLE} -g -fPIE -pie
                ${YARDA_ELF_FIXTURE_SOURCE} -o ${YARDA_ELF_PIE_FIXTURE}
        DEPENDS ${YARDA_ELF_FIXTURE_SOURCE}
    )
    add_custom_command(
        OUTPUT ${YARDA_ELF_EXEC_FIXTURE}
        COMMAND ${YARDA_CLANGXX_EXECUTABLE} -g -fno-pie -no-pie
                -Wl,--section-start=.yarda_map=0x60001c
                ${YARDA_ELF_FIXTURE_SOURCE} -o ${YARDA_ELF_EXEC_FIXTURE}
        DEPENDS ${YARDA_ELF_FIXTURE_SOURCE}
    )
    add_custom_command(
        OUTPUT ${YARDA_ELF_RELOCATABLE_FIXTURE}
        COMMAND ${YARDA_CLANGXX_EXECUTABLE} -g -c
                ${YARDA_ELF_FIXTURE_SOURCE} -o ${YARDA_ELF_RELOCATABLE_FIXTURE}
        DEPENDS ${YARDA_ELF_FIXTURE_SOURCE}
    )
    add_custom_command(
        OUTPUT ${YARDA_ELF_SECTIONLESS_FIXTURE}
        COMMAND ${YARDA_LLVM_OBJCOPY_EXECUTABLE} --strip-sections
                ${YARDA_ELF_PIE_FIXTURE} ${YARDA_ELF_SECTIONLESS_FIXTURE}
        DEPENDS ${YARDA_ELF_PIE_FIXTURE}
    )
    add_custom_target(yarda_elf_region_fixtures ALL
        DEPENDS
            ${YARDA_ELF_PIE_FIXTURE}
            ${YARDA_ELF_EXEC_FIXTURE}
            ${YARDA_ELF_RELOCATABLE_FIXTURE}
            ${YARDA_ELF_SECTIONLESS_FIXTURE}
    )

    add_executable(yarda_elf_regions_tests
        tests/elf/object_addresses_test.cpp
        tests/elf/data_regions_test.cpp
        tests/elf/data_regions_json_test.cpp
    )
    add_dependencies(yarda_elf_regions_tests yarda_elf_region_fixtures)
    target_link_libraries(yarda_elf_regions_tests PRIVATE
        yarda_elf_regions GTest::gtest_main
    )
    target_compile_definitions(yarda_elf_regions_tests PRIVATE
        YARDA_ELF_PIE_FIXTURE_PATH="${YARDA_ELF_PIE_FIXTURE}"
        YARDA_ELF_EXEC_FIXTURE_PATH="${YARDA_ELF_EXEC_FIXTURE}"
        YARDA_ELF_RELOCATABLE_FIXTURE_PATH="${YARDA_ELF_RELOCATABLE_FIXTURE}"
        YARDA_ELF_SECTIONLESS_FIXTURE_PATH="${YARDA_ELF_SECTIONLESS_FIXTURE}"
    )
    include(GoogleTest)
    gtest_discover_tests(yarda_elf_regions_tests)
    add_test(NAME yarda_elf_regions_cli_smoke
        COMMAND yarda_elf_regions_cli ${YARDA_ELF_PIE_FIXTURE}
    )

    add_subdirectory(tests/cli)

    add_executable(yarda_backend_tests
        tests/cache/address_test.cpp
        tests/cache/line_mapping_test.cpp
        tests/cache/line_mapping_json_test.cpp
        tests/cache/lru_rd_analysis_test.cpp
        tests/cache/lru_rd_integration_test.cpp
        tests/trace/calls_test.cpp
        tests/reuse/profile_test.cpp
        tests/trace/access_layout_test.cpp
        tests/trace/schema_test.cpp
        tests/trace/expansion_budget_test.cpp
        tests/trace/mapped_trace_test.cpp
        tests/trace/mapped_trace_strict_test.cpp
        tests/trace/resolved_access_test.cpp
        tests/trace/resolved_mapping_test.cpp
        tests/trace/task_access_stream_test.cpp
        tests/trace/task_scope_test.cpp
        tests/trace/task_access_stream_failure_test.cpp
        tests/trace/task_access_stream_budget_test.cpp
        tests/trace/resolved_mapping_stream_test.cpp
        tests/trace/strict_input_test.cpp
        tests/trace/strict_resolution_test.cpp
        tests/trace/strict_unroller_test.cpp
        tests/trace/task_exclusion_test.cpp
        tests/trace/task_mapping_json_test.cpp
        tests/trace/task_trace_test.cpp
        tests/trace/task_trace_validation_test.cpp
        tests/trace/trace_test.cpp
    )
    target_link_libraries(yarda_backend_tests PRIVATE yarda_backend GTest::gtest_main)
    gtest_discover_tests(yarda_backend_tests)

    add_executable(yarda_hierarchy_analysis_tests
        tests/cache/streaming_hierarchy_test.cpp
        tests/cache/streaming_hierarchy_differential_test.cpp
        tests/cache/streaming_hierarchy_events_test.cpp
        tests/cache/streaming_hierarchy_budget_test.cpp
        tests/cache/streaming_hierarchy_failure_test.cpp
        tests/cache/streaming_hierarchy_summary_test.cpp
        tests/cache/streaming_hierarchy_task_test.cpp
        tests/cache/exact_csrd_test.cpp
        tests/cache/exact_csrd_differential_test.cpp
        tests/cache/recency_index_test.cpp
        tests/cache/exact_csrd_oracle.cpp
        tests/cache/exact_csrd_oracle_test.cpp
        tests/cache/hierarchy_lru_oracle.cpp
        tests/cache/hierarchy_lru_oracle_contract_test.cpp
        tests/cache/hierarchy_lru_oracle_differential_support.cpp
        tests/cache/hierarchy_lru_oracle_differential_support_test.cpp
        tests/cache/hierarchy_lru_oracle_differential_test.cpp
        tests/cache/hierarchy_lru_oracle_operation_test.cpp
        tests/cache/hierarchy_lru_oracle_test.cpp
        tests/cache/hierarchy_model_test.cpp
        tests/cache/hierarchy_model_validation_test.cpp
        tests/cache/hierarchy_analysis_validation_test.cpp
        tests/cache/hierarchy_analysis_l1_test.cpp
        tests/cache/hierarchy_analysis_llc_test.cpp
        tests/cache/hierarchy_analysis_differential_test.cpp
        tests/cache/hierarchy_level_analysis_test.cpp
        tests/cache/hierarchy_aggregation_test.cpp
        tests/cache/hierarchy_aggregation_validation_test.cpp
        tests/cache/hierarchy_service_summary_test.cpp
    )
    target_include_directories(yarda_hierarchy_analysis_tests PRIVATE src/cache)
    target_link_libraries(yarda_hierarchy_analysis_tests PRIVATE
        yarda_hierarchy_analysis GTest::gtest_main
    )
    target_compile_definitions(yarda_hierarchy_analysis_tests PRIVATE
        YARDA_CACHE_CONFIG_32B_PATH="${CMAKE_CURRENT_SOURCE_DIR}/config/cache.32b.yaml"
        YARDA_CACHE_CONFIG_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/config/cache.example.yaml"
    )
    gtest_discover_tests(yarda_hierarchy_analysis_tests)

    add_executable(yarda_cache_config_tests
        tests/cache/cache_config_test.cpp
        tests/cache/yaml_config_parser_test.cpp
    )
    target_link_libraries(yarda_cache_config_tests PRIVATE
        yarda_cache_config GTest::gtest_main
    )
    target_compile_definitions(yarda_cache_config_tests PRIVATE
        YARDA_CACHE_CONFIG_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/config/cache.example.yaml"
    )
    gtest_discover_tests(yarda_cache_config_tests)
endif()
