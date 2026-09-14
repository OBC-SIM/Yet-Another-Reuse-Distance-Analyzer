add_executable(yarda_hierarchy_e2e_tests
    oracle/main.cpp oracle/fixture.cpp oracle/artifacts_test.cpp
    ../../../cache/oracle/exact_csrd_oracle.cpp
    ../../../cache/oracle/hierarchy_lru_oracle.cpp)
target_include_directories(yarda_hierarchy_e2e_tests PRIVATE
    "${YARDA_BACKEND_SOURCE_DIR}/src" "${YARDA_BACKEND_SOURCE_DIR}/tests")
target_include_directories(yarda_hierarchy_e2e_tests SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(yarda_hierarchy_e2e_tests PRIVATE
    yarda_hierarchy_analysis yarda_elf_regions GTest::gtest)
target_compile_options(yarda_hierarchy_e2e_tests PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(yarda_hierarchy_e2e_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${YARDA_BACKEND_BINARY_DIR}")

function(add_hierarchy_e2e case)
    add_test(NAME yarda_cpp_hierarchy_e2e_${case}
        COMMAND ${CMAKE_COMMAND}
            "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
            "-DYARDA_VERIFY=$<TARGET_FILE:yarda_hierarchy_e2e_tests>"
            "-DYARDA_CLANG=${YARDA_CLANG_EXECUTABLE}"
            "-DYARDA_OPT=${YARDA_OPT_EXECUTABLE}"
            "-DYARDA_PLUGIN=$<TARGET_FILE:LoopAnnotatedTrace>"
            "-DYARDA_INCLUDE_DIR=${YARDA_FRONTEND_INCLUDE_DIR}"
            "-DYARDA_REPOSITORY=${CMAKE_SOURCE_DIR}"
            "-DYARDA_CACHE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cache.yaml"
            "-DYARDA_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/${case}"
            ${ARGN} -P "${CMAKE_CURRENT_SOURCE_DIR}/${case}.cmake")
    set_tests_properties(yarda_cpp_hierarchy_e2e_${case} PROPERTIES TIMEOUT 120)
endfunction()

add_hierarchy_e2e(positive)
add_hierarchy_e2e(affine)
add_hierarchy_e2e(affine_rejections)
add_hierarchy_e2e(limits)
add_hierarchy_e2e(diagnostics)
add_hierarchy_e2e(rejections)
add_hierarchy_e2e(tasks)
if(TARGET yarda_region_lat)
    add_hierarchy_e2e(regions "-DYARDA_REGION=$<TARGET_FILE:yarda_region_lat>")
    add_hierarchy_e2e(region_rejections "-DYARDA_REGION=$<TARGET_FILE:yarda_region_lat>")
endif()
