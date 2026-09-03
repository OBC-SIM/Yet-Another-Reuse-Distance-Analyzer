find_program(YARDA_CLANG_EXECUTABLE NAMES clang-14 REQUIRED)
find_program(YARDA_OPT_EXECUTABLE NAMES opt-14 REQUIRED)

get_filename_component(YARDA_BACKEND_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(YARDA_FRONTEND_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/frontend/tasks")
set(YARDA_TASK_SOURCE_DIR "${CMAKE_SOURCE_DIR}/tasks")
set(YARDA_CPP_CACHE_32B
    "${YARDA_BACKEND_SOURCE_DIR}/config/cache.32b.yaml")

set(YARDA_CPP_CLI_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/unroll_fixture.json")
add_test(NAME yarda_cpp_unroll_smoke
    COMMAND yarda_cpp "${YARDA_CPP_CLI_FIXTURE}" --mode unroll
)
add_test(NAME yarda_cpp_cache_config_smoke
    COMMAND yarda_cpp "${YARDA_CPP_CLI_FIXTURE}"
        --mode unroll
        --granularity cache-line
        --cache "${YARDA_BACKEND_SOURCE_DIR}/config/cache.example.yaml"
)
add_test(NAME yarda_cpp_requires_cache_config
    COMMAND yarda_cpp "${YARDA_CPP_CLI_FIXTURE}"
        --mode unroll
        --granularity cache-line
)
set_tests_properties(yarda_cpp_requires_cache_config PROPERTIES
    WILL_FAIL TRUE
)
add_test(NAME yarda_cpp_rejects_predict_mode
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_CLI_FIXTURE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/reject_predict_mode.cmake"
)

set(YARDA_CPP_TASK_MAPPING_FIXTURE
    "${CMAKE_CURRENT_SOURCE_DIR}/task_mapping_fixture.json")
set(YARDA_CPP_MISSING_SYMBOL_FIXTURE
    "${CMAKE_CURRENT_SOURCE_DIR}/missing_symbol_fixture.json")
set(YARDA_CPP_TASK_MAPPING_OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/task_mapping_result.json")
add_test(NAME yarda_cpp_maps_elf_tasks
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
        "-DYARDA_ELF=${YARDA_ELF_EXEC_FIXTURE}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        "-DYARDA_OUTPUT=${YARDA_CPP_TASK_MAPPING_OUTPUT}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_task_mapping.cmake"
)
add_test(NAME yarda_cpp_maps_elf_tasks_to_stdout
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
        "-DYARDA_ELF=${YARDA_ELF_EXEC_FIXTURE}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_task_mapping.cmake"
)
add_test(NAME yarda_cpp_elf_requires_cache
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
        "-DYARDA_ELF=${YARDA_ELF_EXEC_FIXTURE}"
        -DEXPECTED_ERROR=--cache.is.required.with.--elf
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_cli_error.cmake"
)
add_test(NAME yarda_cpp_rejects_pie_elf
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
        "-DYARDA_ELF=${YARDA_ELF_PIE_FIXTURE}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        -DEXPECTED_ERROR=--elf.task.mapping.requires.an.ET_EXEC.image
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_cli_error.cmake"
)
add_test(NAME yarda_cpp_rejects_malformed_elf
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
        "-DYARDA_ELF=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        -DEXPECTED_ERROR=input.is.not.a.valid.object.file
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_cli_error.cmake"
)
add_test(NAME yarda_cpp_rejects_missing_elf
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
        "-DYARDA_ELF=${CMAKE_CURRENT_BINARY_DIR}/missing.elf"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        -DEXPECTED_ERROR=ELF.input.does.not.exist
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_cli_error.cmake"
)
add_test(NAME yarda_cpp_rejects_missing_elf_symbol
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_MISSING_SYMBOL_FIXTURE}"
        "-DYARDA_ELF=${YARDA_ELF_EXEC_FIXTURE}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        -DEXPECTED_ERROR=ELF.object.symbol.is.unavailable
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_cli_error.cmake"
)
add_test(NAME yarda_cpp_rejects_elf_element_granularity
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
        "-DYARDA_ELF=${YARDA_ELF_EXEC_FIXTURE}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        -DYARDA_GRANULARITY=element
        -DEXPECTED_ERROR=incompatible.with.--elf
        -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_cli_error.cmake"
)
if(EXISTS "/dev/full")
    add_test(NAME yarda_cpp_rejects_failed_export
        COMMAND ${CMAKE_COMMAND}
            "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
            "-DYARDA_INPUT=${YARDA_CPP_TASK_MAPPING_FIXTURE}"
            "-DYARDA_ELF=${YARDA_ELF_EXEC_FIXTURE}"
            "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
            -DYARDA_EXPORT=/dev/full
            -DEXPECTED_ERROR=failed.to.write.export.path
            -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_cli_error.cmake"
    )
endif()

add_test(NAME yarda_cpp_generated_task_mapping_e2e
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CLANG=${YARDA_CLANG_EXECUTABLE}"
        "-DYARDA_OPT=${YARDA_OPT_EXECUTABLE}"
        "-DYARDA_PLUGIN=$<TARGET_FILE:LoopAnnotatedTrace>"
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/task_mapping_e2e.c"
        "-DYARDA_INCLUDE_DIR=${YARDA_FRONTEND_INCLUDE_DIR}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        "-DYARDA_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated_mapping"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_task_mapping_e2e.cmake"
)
add_test(NAME yarda_cpp_generated_task_mapping_rejections
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CLANG=${YARDA_CLANG_EXECUTABLE}"
        "-DYARDA_OPT=${YARDA_OPT_EXECUTABLE}"
        "-DYARDA_PLUGIN=$<TARGET_FILE:LoopAnnotatedTrace>"
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/task_mapping_unsupported_e2e.c"
        "-DYARDA_INCLUDE_DIR=${YARDA_FRONTEND_INCLUDE_DIR}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        "-DYARDA_OTHER_ELF=${YARDA_ELF_EXEC_FIXTURE}"
        "-DYARDA_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated_rejections"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_task_mapping_rejections.cmake"
)
add_test(NAME yarda_cpp_generated_task_expansion_budget
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CLANG=${YARDA_CLANG_EXECUTABLE}"
        "-DYARDA_OPT=${YARDA_OPT_EXECUTABLE}"
        "-DYARDA_PLUGIN=$<TARGET_FILE:LoopAnnotatedTrace>"
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/task_expansion_budget_e2e.c"
        "-DYARDA_INCLUDE_DIR=${YARDA_FRONTEND_INCLUDE_DIR}"
        "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
        "-DYARDA_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated_expansion_budget"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_task_expansion_budget_e2e.cmake"
)

add_test(NAME yarda_cpp_legacy_export
    COMMAND ${CMAKE_COMMAND}
        "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
        "-DYARDA_INPUT=${YARDA_CPP_CLI_FIXTURE}"
        "-DYARDA_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/legacy_export.json"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_legacy_export.cmake"
)

function(add_task_unroll_semantics_test test_name source case_name granularity)
    add_test(NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
            "-DYARDA_CLANG=${YARDA_CLANG_EXECUTABLE}"
            "-DYARDA_OPT=${YARDA_OPT_EXECUTABLE}"
            "-DYARDA_PLUGIN=$<TARGET_FILE:LoopAnnotatedTrace>"
            "-DYARDA_CPP=$<TARGET_FILE:yarda_cpp>"
            "-DYARDA_SOURCE=${YARDA_TASK_SOURCE_DIR}/${source}"
            "-DYARDA_INCLUDE_DIR=${YARDA_TASK_SOURCE_DIR}"
            "-DYARDA_CACHE=${YARDA_CPP_CACHE_32B}"
            "-DYARDA_CASE=${case_name}"
            "-DYARDA_GRANULARITY=${granularity}"
            "-DYARDA_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/task_semantics/${case_name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_task_unroll_semantics.cmake"
    )
endfunction()

add_task_unroll_semantics_test(
    yarda_cpp_unroll_task_call_element_semantics
    test_call.c call_element element)
add_task_unroll_semantics_test(
    yarda_cpp_unroll_task_constant_access_element_semantics
    test_constant_access2.c constant_access_element element)
add_task_unroll_semantics_test(
    yarda_cpp_unroll_task_constant_access_relative_line_semantics
    test_constant_access2.c constant_access_cache_line cache-line)
add_task_unroll_semantics_test(
    yarda_cpp_unroll_task_stencil_relative_line_semantics
    test_stencil.c stencil_cache_line cache-line)
add_task_unroll_semantics_test(
    yarda_cpp_unroll_task_regular_block_element_semantics
    test_regular_block.c regular_block_element element)
