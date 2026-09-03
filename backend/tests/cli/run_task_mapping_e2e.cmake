foreach(required YARDA_CLANG YARDA_OPT YARDA_PLUGIN YARDA_CPP YARDA_SOURCE
                 YARDA_INCLUDE_DIR YARDA_CACHE YARDA_WORK_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

function(run_checked label working_directory)
    execute_process(
        COMMAND ${ARGN}
        WORKING_DIRECTORY "${working_directory}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed (${result})\nstdout:\n${output}\nstderr:\n${error_output}")
    endif()
endfunction()

function(assert_json expected)
    string(JSON actual ERROR_VARIABLE json_error GET "${payload}" ${ARGN})
    if(json_error)
        string(JOIN "." path ${ARGN})
        message(FATAL_ERROR "cannot read ${path}: ${json_error}")
    endif()
    if(NOT actual STREQUAL expected)
        string(JOIN "." path ${ARGN})
        message(FATAL_ERROR
            "unexpected ${path}: expected '${expected}', got '${actual}'")
    endif()
endfunction()

function(assert_json_length expected)
    string(JSON actual ERROR_VARIABLE json_error LENGTH "${payload}" ${ARGN})
    if(json_error)
        string(JOIN "." path ${ARGN})
        message(FATAL_ERROR "cannot read length of ${path}: ${json_error}")
    endif()
    if(NOT actual STREQUAL expected)
        string(JOIN "." path ${ARGN})
        message(FATAL_ERROR
            "unexpected ${path} length: expected ${expected}, got ${actual}")
    endif()
endfunction()

function(assert_resolved task access object offset size address operation ordinal)
    assert_json("${object}" tasks ${task} resolved_accesses ${access} object_id)
    assert_json("${offset}" tasks ${task} resolved_accesses ${access}
        object_byte_offset)
    assert_json("${size}" tasks ${task} resolved_accesses ${access} access_size)
    assert_json("${address}" tasks ${task} resolved_accesses ${access}
        linked_byte_address)
    assert_json("linked_absolute" tasks ${task} resolved_accesses ${access}
        address_basis)
    assert_json("${operation}" tasks ${task} resolved_accesses ${access} operation)
    assert_json("${ordinal}" tasks ${task} resolved_accesses ${access}
        source_access_ordinal)
endfunction()

function(assert_mapping task mapping object object_offset address operation
         source_ordinal source_size source_address source_offset block set tag
         line_offset line_span)
    set(path tasks ${task} mapped_line_references ${mapping})
    assert_json("${object}" ${path} object_id)
    assert_json("${object_offset}" ${path} object_byte_offset)
    assert_json("${address}" ${path} linked_byte_address)
    assert_json("linked_absolute" ${path} address_basis)
    assert_json("${operation}" ${path} operation)
    assert_json("${source_ordinal}" ${path} source_access_ordinal)
    assert_json("${source_size}" ${path} source_access_size)
    assert_json("${source_address}" ${path} source_linked_byte_address)
    assert_json("${source_offset}" ${path} source_object_byte_offset)
    assert_json("${block}" ${path} block_number)
    assert_json("${set}" ${path} set_index)
    assert_json("${tag}" ${path} tag)
    assert_json("${line_offset}" ${path} line_offset)
    assert_json("${line_span}" ${path} line_span_ordinal)
endfunction()

file(REMOVE_RECURSE "${YARDA_WORK_DIR}")
file(MAKE_DIRECTORY "${YARDA_WORK_DIR}")

set(ir "${YARDA_WORK_DIR}/task_mapping_e2e.ll")
set(lat "${YARDA_WORK_DIR}/task_mapping_e2e_ape.json")
set(elf "${YARDA_WORK_DIR}/task_mapping_e2e.elf")
set(first_result "${YARDA_WORK_DIR}/task_mapping_first.json")
set(second_result "${YARDA_WORK_DIR}/task_mapping_second.json")

run_checked("C to LLVM IR" "${YARDA_WORK_DIR}"
    "${YARDA_CLANG}" -O0 -Xclang -disable-O0-optnone -g
    -I "${YARDA_INCLUDE_DIR}" -emit-llvm -S "${YARDA_SOURCE}" -o "${ir}")
run_checked("LLVM IR to LAT" "${YARDA_WORK_DIR}"
    "${YARDA_OPT}" -load-pass-plugin "${YARDA_PLUGIN}"
    "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
    "${ir}" -o /dev/null)
run_checked("C to ET_EXEC" "${YARDA_WORK_DIR}"
    "${YARDA_CLANG}" -O0 -g -fno-pie -no-pie
    -I "${YARDA_INCLUDE_DIR}" "${YARDA_SOURCE}"
    "-Wl,--section-start=.yarda_scalar=0x600000"
    "-Wl,--section-start=.yarda_array=0x600020"
    "-Wl,--section-start=.yarda_record=0x60004c"
    -o "${elf}")

run_checked("first task mapping" "${YARDA_WORK_DIR}"
    "${YARDA_CPP}" "${lat}" --elf "${elf}" --cache "${YARDA_CACHE}"
    --export "${first_result}")
run_checked("second task mapping" "${YARDA_WORK_DIR}"
    "${YARDA_CPP}" "${lat}" --elf "${elf}" --cache "${YARDA_CACHE}"
    --export "${second_result}")

file(READ "${first_result}" payload)
file(READ "${second_result}" rerun_payload)
if(NOT payload STREQUAL rerun_payload)
    message(FATAL_ERROR "task-mapping output is not deterministic")
endif()

assert_json("1" schema_version)
assert_json("elf-task-mapping" mode)
assert_json("ET_EXEC" elf type)
assert_json("linked_absolute" elf address_basis)
assert_json("ON" coverage complete)
assert_json("6" coverage source_accesses)
assert_json("6" coverage resolved_accesses)
assert_json("0" coverage rejected_accesses)
assert_json("7" coverage emitted_line_references)
assert_json("0" exclusions known_non_inline_static_call_sites)
assert_json_length("2" tasks)

assert_json("alpha_task" tasks 0 task_id)
assert_json("ON" tasks 0 coverage complete)
assert_json("4" tasks 0 coverage source_accesses)
assert_json("4" tasks 0 coverage resolved_accesses)
assert_json("0" tasks 0 coverage rejected_accesses)
assert_json("5" tasks 0 coverage emitted_line_references)
assert_resolved(0 0 global::e2e_scalar 0 4 6291456 load 0)
assert_resolved(0 1 global::e2e_array 0 4 6291488 store 1)
assert_resolved(0 2 global::e2e_array 4 4 6291492 load 2)
assert_resolved(0 3 global::e2e_record 16 8 6291548 store 3)

assert_json_length("5" tasks 0 mapped_line_references)
assert_mapping(0 0 global::e2e_scalar 0 6291456 load 0 4 6291456 0
    196608 0 768 0 0)
assert_mapping(0 1 global::e2e_array 0 6291488 store 1 4 6291488 0
    196609 1 768 0 0)
assert_mapping(0 2 global::e2e_array 4 6291492 load 2 4 6291492 4
    196609 1 768 4 0)
assert_mapping(0 3 global::e2e_record 16 6291548 store 3 8 6291548 16
    196610 2 768 28 0)
assert_mapping(0 4 global::e2e_record 20 6291552 store 3 8 6291548 16
    196611 3 768 0 1)

assert_json("beta_task" tasks 1 task_id)
assert_json("ON" tasks 1 coverage complete)
assert_json("2" tasks 1 coverage source_accesses)
assert_json("2" tasks 1 coverage resolved_accesses)
assert_json("0" tasks 1 coverage rejected_accesses)
assert_json("2" tasks 1 coverage emitted_line_references)
assert_resolved(1 0 global::e2e_array 0 4 6291488 load 0)
assert_resolved(1 1 global::e2e_scalar 0 4 6291456 store 1)

assert_json_length("2" tasks 1 mapped_line_references)
assert_mapping(1 0 global::e2e_array 0 6291488 load 0 4 6291488 0
    196609 1 768 0 0)
assert_mapping(1 1 global::e2e_scalar 0 6291456 store 1 4 6291456 0
    196608 0 768 0 0)
