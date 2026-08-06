add_library(yarda_backend
    src/cache/address.cpp
    src/cache/line_mapping.cpp
    src/cache/line_mapping_json.cpp
    src/cache/lru_rd_analysis.cpp
    src/calls.cpp
    src/dilation.cpp
    src/merger.cpp
    src/predictor.cpp
    src/predictor_3d.cpp
    src/profile.cpp
    src/schema.cpp
    src/trace.cpp
    src/trace/cache_line.cpp
    src/trace/mapped_trace.cpp
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

find_package(LLVM 14 REQUIRED CONFIG)
llvm_map_components_to_libnames(YARDA_ELF_LLVM_LIBS object support)

add_library(yarda_elf_regions
    src/elf_data_regions.cpp
    src/elf_data_regions_json.cpp
    src/elf_object_addresses.cpp
    src/elf_program_regions.cpp
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

add_executable(yarda_elf_regions_cli src/elf_regions_main.cpp)
set_target_properties(yarda_elf_regions_cli PROPERTIES OUTPUT_NAME yarda_elf_regions)
target_link_libraries(yarda_elf_regions_cli PRIVATE yarda_elf_regions)

add_executable(yarda_cpp src/main.cpp)
target_link_libraries(yarda_cpp PRIVATE yarda_backend)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    find_program(YARDA_CLANGXX_EXECUTABLE NAMES clang++-14 REQUIRED)
    find_program(YARDA_LLVM_OBJCOPY_EXECUTABLE NAMES llvm-objcopy-14 REQUIRED)
    set(YARDA_ELF_FIXTURE_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/elf_data_fixture.cpp)
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
        tests/elf_object_addresses_test.cpp
        tests/elf_data_regions_test.cpp
        tests/elf_data_regions_json_test.cpp
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

    add_executable(yarda_backend_tests
        tests/cache/address_test.cpp
        tests/cache/line_mapping_test.cpp
        tests/cache/line_mapping_json_test.cpp
        tests/cache/lru_rd_analysis_test.cpp
        tests/cache/lru_rd_integration_test.cpp
        tests/calls_test.cpp
        tests/dilation_test.cpp
        tests/merger_test.cpp
        tests/predictor_test.cpp
        tests/profile_test.cpp
        tests/schema_test.cpp
        tests/trace/mapped_trace_test.cpp
        tests/trace_test.cpp
    )
    target_link_libraries(yarda_backend_tests PRIVATE yarda_backend GTest::gtest_main)
    gtest_discover_tests(yarda_backend_tests)

    add_executable(yarda_cache_config_tests
        tests/cache_config_test.cpp
        tests/yaml_config_parser_test.cpp
    )
    target_link_libraries(yarda_cache_config_tests PRIVATE
        yarda_cache_config GTest::gtest_main
    )
    target_compile_definitions(yarda_cache_config_tests PRIVATE
        YARDA_CACHE_CONFIG_EXAMPLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/config/cache.example.yaml"
    )
    gtest_discover_tests(yarda_cache_config_tests)
endif()
