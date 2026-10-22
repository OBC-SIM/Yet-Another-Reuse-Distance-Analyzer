set(static_dir "${CMAKE_CURRENT_BINARY_DIR}/static-access-fixtures")
file(MAKE_DIRECTORY "${static_dir}")
set(static_outputs)
foreach(fixture static_accesses access_widths)
    set(static_source "${CMAKE_CURRENT_SOURCE_DIR}/../../frontend/tests/access/fixtures/${fixture}.c")
    if(fixture STREQUAL "access_widths")
        set(suffix "_widths")
    else()
        set(suffix "")
    endif()
    foreach(mode debug nodebug)
        if(mode STREQUAL "debug")
            set(debug_flag -g)
        else()
            set(debug_flag -g0)
        endif()
        set(stem "${static_dir}/${mode}${suffix}")
        add_custom_command(OUTPUT "${stem}_ape.json" "${stem}.elf"
            COMMAND "${YARDA_AFFINE_CLANG}" -O0 -Xclang -disable-O0-optnone
                "${debug_flag}" -emit-llvm -S "${static_source}" -o "${stem}.ll"
            COMMAND "${YARDA_AFFINE_OPT}"
                "-load-pass-plugin=$<TARGET_FILE:MemoryAccessPatterns>"
                "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
                "${stem}.ll" -o /dev/null
            COMMAND "${YARDA_AFFINE_CLANG}" -O0 "${debug_flag}" -fno-pie -no-pie
                "${static_source}" -o "${stem}.elf"
            DEPENDS MemoryAccessPatterns "${static_source}"
            WORKING_DIRECTORY "${static_dir}" VERBATIM)
        list(APPEND static_outputs "${stem}_ape.json" "${stem}.elf")
    endforeach()
endforeach()
add_custom_target(yarda_static_access_fixtures DEPENDS ${static_outputs})
add_dependencies(yarda_affine_index_tests yarda_static_access_fixtures)
target_sources(yarda_affine_index_tests PRIVATE
    trace/integration/static_access_contract_test.cpp)
target_compile_definitions(yarda_affine_index_tests PRIVATE
    YARDA_STATIC_FIXTURE_DIR="${static_dir}")
