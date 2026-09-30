find_program(YARDA_CLANGXX_EXECUTABLE NAMES clang++-14 REQUIRED)
find_program(YARDA_LLVM_OBJCOPY_EXECUTABLE NAMES llvm-objcopy-14 REQUIRED)
set(YARDA_ELF_FIXTURE_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/data_fixture.cpp)

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
    object_addresses_test.cpp
    data_regions_test.cpp
    data_regions_json_test.cpp
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
gtest_discover_tests(yarda_elf_regions_tests
    WORKING_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
)
add_test(NAME yarda_elf_regions_cli_smoke
    COMMAND yarda_elf_regions_cli ${YARDA_ELF_PIE_FIXTURE}
)
set_tests_properties(yarda_elf_regions_cli_smoke PROPERTIES
    WORKING_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}"
)
