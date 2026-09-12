set(affine_source "${CMAKE_CURRENT_SOURCE_DIR}/../../frontend/tests/fixtures/legacy_affine_supported.c")
set(affine_dir "${CMAKE_CURRENT_BINARY_DIR}/affine-fixtures")
file(MAKE_DIRECTORY "${affine_dir}")
find_program(YARDA_AFFINE_CLANG NAMES clang-14 REQUIRED)
find_program(YARDA_AFFINE_OPT NAMES opt-14 REQUIRED)
set(affine_outputs)
foreach(mode debug nodebug)
    if(mode STREQUAL "debug")
        set(debug_flag -g)
    else()
        set(debug_flag -g0)
    endif()
    set(stem "${affine_dir}/${mode}")
    add_custom_command(OUTPUT "${stem}_ape.json" "${stem}.elf"
        COMMAND "${YARDA_AFFINE_CLANG}" -O0 -Xclang -disable-O0-optnone
            "${debug_flag}" -emit-llvm -S "${affine_source}" -o "${stem}.ll"
        COMMAND "${YARDA_AFFINE_OPT}"
            "-load-pass-plugin=$<TARGET_FILE:LoopAnnotatedTrace>"
            "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
            "${stem}.ll" -o /dev/null
        COMMAND "${YARDA_AFFINE_CLANG}" -O0 "${debug_flag}" -fno-pie -no-pie
            "${affine_source}" -o "${stem}.elf"
        DEPENDS LoopAnnotatedTrace "${affine_source}"
        WORKING_DIRECTORY "${affine_dir}" VERBATIM)
    list(APPEND affine_outputs "${stem}_ape.json" "${stem}.elf")
endforeach()
add_custom_target(yarda_affine_fixtures DEPENDS ${affine_outputs})
add_executable(yarda_affine_index_tests
    regions/main.cpp trace/integration/affine_index_contract_test.cpp
    cache/oracle/exact_csrd_oracle.cpp cache/oracle/hierarchy_lru_oracle.cpp)
add_dependencies(yarda_affine_index_tests yarda_affine_fixtures)
target_include_directories(yarda_affine_index_tests PRIVATE
    "${YARDA_BACKEND_SOURCE_DIR}/src"
    "${YARDA_BACKEND_SOURCE_DIR}/tests"
)
target_link_libraries(yarda_affine_index_tests PRIVATE
    yarda_hierarchy_analysis yarda_elf_regions GTest::gtest)
target_include_directories(yarda_affine_index_tests SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(yarda_affine_index_tests PRIVATE
    YARDA_AFFINE_FIXTURE_DIR="${affine_dir}")
set_target_properties(yarda_affine_index_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}")
gtest_discover_tests(yarda_affine_index_tests)
