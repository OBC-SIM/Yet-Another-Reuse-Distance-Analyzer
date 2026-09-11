# Keep the existing backend artifact locations unless the caller overrides them.
foreach(kind RUNTIME LIBRARY ARCHIVE)
    if(NOT DEFINED CMAKE_${kind}_OUTPUT_DIRECTORY)
        set(CMAKE_${kind}_OUTPUT_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}")
    endif()
endforeach()

add_library(yarda_backend
    cache/address.cpp
    cache/line_mapping.cpp
    cache/line_mapping_json.cpp
    cache/lru_rd_analysis.cpp
    trace/calls.cpp
    trace/call_substitution.cpp
    reuse/profile.cpp
    trace/schema.cpp
    trace/trace.cpp
    trace/access_layout.cpp
    trace/access_layout_legacy.cpp
    trace/access_layout_structured.cpp
    trace/access_resolver.cpp
    trace/cache_line.cpp
    trace/mapped_trace.cpp
    trace/resolved_access.cpp
    trace/resolved_mapping.cpp
    trace/emission_budget.cpp
    trace/task_access_stream.cpp
    trace/task_scope.cpp
    trace/task_access_support.cpp
    trace/prepared_index.cpp
    trace/loop_iteration.cpp
    trace/prepared_trace.cpp
    trace/unroller.cpp
    trace/task_mapping_json.cpp
)

target_include_directories(yarda_backend PUBLIC "${YARDA_BACKEND_SOURCE_DIR}/include")
target_link_libraries(yarda_backend PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(yarda_backend PRIVATE -Wall -Wextra -Wpedantic)

find_package(yaml-cpp 0.7 REQUIRED CONFIG)
add_library(yarda_cache_config
    cache/cache_config.cpp
    cache/yaml_config_parser.cpp
)
target_include_directories(yarda_cache_config PUBLIC "${YARDA_BACKEND_SOURCE_DIR}/include")
target_compile_options(yarda_cache_config PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(yarda_cache_config
    PUBLIC yarda_backend
    PRIVATE yaml-cpp
)

add_library(yarda_hierarchy_analysis
    cache/streaming_hierarchy.cpp
    cache/streaming_hierarchy_summary.cpp
    cache/streaming_hierarchy_task.cpp
    cache/exact_csrd.cpp
    cache/recency_index.cpp
    cache/hierarchy_model.cpp
    cache/hierarchy_analysis.cpp
    cache/hierarchy_aggregation.cpp
    cache/hierarchy_service_summary.cpp
    cache/hierarchy_level_analysis.cpp
    cache/hierarchy_input_validation.cpp
)
target_link_libraries(yarda_hierarchy_analysis PUBLIC yarda_cache_config)
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

add_executable(yarda_cpp cli/main.cpp)
target_link_libraries(yarda_cpp PRIVATE yarda_cache_config yarda_elf_regions)
