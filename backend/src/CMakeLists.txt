# Keep the existing backend artifact locations unless the caller overrides them.
foreach(kind RUNTIME LIBRARY ARCHIVE)
    if(NOT DEFINED CMAKE_${kind}_OUTPUT_DIRECTORY)
        set(CMAKE_${kind}_OUTPUT_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}")
    endif()
endforeach()

add_library(yarda_backend
    cache/mapping/address.cpp
    cache/mapping/line_mapping.cpp
    cache/mapping/line_mapping_json.cpp
    cache/rd/lru_rd_analysis.cpp
    trace/module/calls.cpp
    trace/module/call_substitution.cpp
    reuse/profile.cpp
    trace/module/schema.cpp
    trace/execution/trace.cpp
    trace/layout/access_layout.cpp
    trace/layout/access_layout_legacy.cpp
    trace/layout/access_layout_structured.cpp
    trace/layout/access_layout_structured_type.cpp
    trace/access/access_resolver.cpp
    trace/access/cache_line.cpp
    trace/output/mapped_trace.cpp
    trace/output/resolved_access.cpp
    trace/output/resolved_mapping.cpp
    trace/execution/emission_budget.cpp
    trace/output/task_access_stream.cpp
    trace/module/task_scope.cpp
    trace/output/task_access_support.cpp
    trace/index/prepared_index.cpp
    trace/index/affine_expression.cpp
    trace/layout/prepared_layout.cpp
    trace/access/prepared_access.cpp
    trace/execution/loop_iteration.cpp
    trace/execution/prepared_trace.cpp
    trace/execution/unroller.cpp
    trace/output/task_mapping_json.cpp
)

target_include_directories(yarda_backend
    PUBLIC "${YARDA_BACKEND_SOURCE_DIR}/include"
    PRIVATE "${YARDA_BACKEND_SOURCE_DIR}/src"
)
target_link_libraries(yarda_backend PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(yarda_backend PRIVATE -Wall -Wextra -Wpedantic)

find_package(yaml-cpp 0.7 REQUIRED CONFIG)
add_library(yarda_cache_config
    cache/config/cache_config.cpp
    cache/config/yaml_config_parser.cpp
)
target_include_directories(yarda_cache_config PUBLIC "${YARDA_BACKEND_SOURCE_DIR}/include")
target_compile_options(yarda_cache_config PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(yarda_cache_config
    PUBLIC yarda_backend
    PRIVATE yaml-cpp
)

add_library(yarda_hierarchy_analysis
    cache/output/artifact_identity.cpp
    cache/output/hierarchy_result_json.cpp
    cache/output/hierarchy_result_validation.cpp
    cache/output/hierarchy_json_fields.cpp
    cache/output/hierarchy_events_json.cpp
    cache/output/hierarchy_event_validation.cpp
    cache/output/analysis_telemetry.cpp
    cache/output/analysis_telemetry_validation.cpp
    cache/output/hierarchy_telemetry_json.cpp
    cache/output/runtime_measurement.cpp
    cache/streaming/streaming_hierarchy.cpp
    cache/streaming/streaming_hierarchy_summary.cpp
    cache/streaming/streaming_hierarchy_task.cpp
    cache/rd/exact_csrd.cpp
    cache/rd/csrd_statistics.cpp
    cache/rd/recency_index.cpp
    cache/rd/recency_statistics.cpp
    cache/hierarchy/hierarchy_model.cpp
    cache/hierarchy/hierarchy_analysis.cpp
    cache/hierarchy/hierarchy_aggregation.cpp
    cache/hierarchy/hierarchy_service_summary.cpp
    cache/hierarchy/hierarchy_level_analysis.cpp
    cache/hierarchy/hierarchy_input_validation.cpp
)
target_include_directories(yarda_hierarchy_analysis PRIVATE
    "${YARDA_BACKEND_SOURCE_DIR}/src"
)
target_link_libraries(yarda_hierarchy_analysis PUBLIC yarda_cache_config)
llvm_map_components_to_libnames(YARDA_ARTIFACT_LLVM_LIBS support)
target_include_directories(yarda_hierarchy_analysis SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(yarda_hierarchy_analysis PRIVATE ${YARDA_ARTIFACT_LLVM_LIBS})
target_compile_options(yarda_hierarchy_analysis PRIVATE -Wall -Wextra -Wpedantic)

llvm_map_components_to_libnames(YARDA_ELF_LLVM_LIBS object support)

add_library(yarda_elf_regions
    elf/data_regions.cpp
    elf/data_regions_json.cpp
    elf/object_addresses.cpp
    elf/program_regions.cpp
)
target_include_directories(yarda_elf_regions
    PUBLIC "${YARDA_BACKEND_SOURCE_DIR}/include"
)
target_include_directories(yarda_elf_regions SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_options(yarda_elf_regions PRIVATE
    -Wall -Wextra -Wpedantic
)
target_link_libraries(yarda_elf_regions
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE ${YARDA_ELF_LLVM_LIBS}
)

add_executable(yarda_elf_regions_cli cli/elf_regions_main.cpp)
set_target_properties(yarda_elf_regions_cli PROPERTIES OUTPUT_NAME yarda_elf_regions)
target_link_libraries(yarda_elf_regions_cli PRIVATE yarda_elf_regions)

set(YARDA_TOOL_VERSION "" CACHE STRING "Fixed release identifier (default: source commit)")
set(YARDA_CLI_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/cli-generated")
add_custom_target(yarda_cli_version
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_SOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        "-DYARDA_RELEASE_VERSION=${PROJECT_VERSION}"
        "-DYARDA_TOOL_VERSION=${YARDA_TOOL_VERSION}"
        "-DYARDA_VERSION_OUTPUT=${YARDA_CLI_GENERATED}/build_version.hpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cli/build_version.cmake"
    BYPRODUCTS "${YARDA_CLI_GENERATED}/build_version.hpp"
    VERBATIM
)
add_library(yarda_cli_support
    cli/artifact_output.cpp
    cli/options.cpp cli/json_output.cpp
    cli/hierarchy_command.cpp
)
add_dependencies(yarda_cli_support yarda_cli_version)
target_include_directories(yarda_cli_support PRIVATE "${YARDA_CLI_GENERATED}")
target_link_libraries(yarda_cli_support PUBLIC yarda_hierarchy_analysis yarda_elf_regions)
target_compile_options(yarda_cli_support PRIVATE -Wall -Wextra -Wpedantic)
add_executable(yarda_cpp cli/main.cpp)
target_link_libraries(yarda_cpp PRIVATE yarda_cli_support)
target_compile_options(yarda_cpp PRIVATE -Wall -Wextra -Wpedantic)
