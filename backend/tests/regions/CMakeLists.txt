get_filename_component(REGION_REPOSITORY "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(REGION_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
file(MAKE_DIRECTORY "${REGION_FIXTURE_DIR}")

foreach(name boundary global_values)
    set(original "${REGION_REPOSITORY}/frontend/experiments/analysis_regions/fixtures/${name}.c")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}")
    file(READ "${original}" source)
    file(WRITE "${REGION_FIXTURE_DIR}/${name}.c" "${source}\nint main(void) { return 0; }\n")
endforeach()
foreach(name atax tasks)
    configure_file("${REGION_REPOSITORY}/frontend/tests/fixtures/analysis_region_${name}.c"
                   "${REGION_FIXTURE_DIR}/${name}.c" COPYONLY)
endforeach()

set(region_outputs)
foreach(name boundary global_values atax tasks)
    set(source "${REGION_FIXTURE_DIR}/${name}.c")
    set(lat "${REGION_FIXTURE_DIR}/${name}.json")
    set(elf "${REGION_FIXTURE_DIR}/${name}.elf")
    add_custom_command(OUTPUT "${lat}" "${elf}"
        COMMAND yarda_region_lat "${source}" "${lat}"
        COMMAND "${YARDA_REGION_CLANG}" -std=c11 -O0 -g -Wno-unknown-pragmas
                -no-pie "${source}" -o "${elf}"
        DEPENDS yarda_region_lat "${source}"
        VERBATIM)
    list(APPEND region_outputs "${lat}" "${elf}")
endforeach()
add_custom_target(yarda_region_fixtures DEPENDS ${region_outputs})

add_executable(yarda_region_integration_tests
    main.cpp
    ../trace/analysis_region_test.cpp
    ../cache/streaming_hierarchy_region_test.cpp
    ../cache/exact_csrd_oracle.cpp
    ../cache/hierarchy_lru_oracle.cpp)
add_dependencies(yarda_region_integration_tests yarda_region_fixtures)
target_link_libraries(yarda_region_integration_tests PRIVATE
    yarda_hierarchy_analysis yarda_elf_regions GTest::gtest)
target_include_directories(yarda_region_integration_tests SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(yarda_region_integration_tests PRIVATE
    YARDA_REGION_FIXTURE_DIR="${REGION_FIXTURE_DIR}")
include(GoogleTest)
gtest_discover_tests(yarda_region_integration_tests)
