include("${CMAKE_CURRENT_LIST_DIR}/commands.cmake")

macro(select_outputs)
    set(result_file "${case_dir}/result.json")
    set(events_file "${case_dir}/events.json")
    set(telemetry_file "${case_dir}/telemetry.json")
    set(base "${lat}" --analysis hierarchy-rd --elf "${elf}"
        --cache "${YARDA_CACHE}" --export "${result_file}")
    set(diagnostics --export-events "${events_file}" --event-limit 100000
        --telemetry "${telemetry_file}")
endmacro()

macro(prepare_case name)
    set(case_dir "${YARDA_WORK_DIR}/${name}")
    file(MAKE_DIRECTORY "${case_dir}")
    set(ir "${case_dir}/input.ll")
    set(lat "${case_dir}/input_ape.json")
    set(elf "${case_dir}/input.elf")
    file(WRITE "${case_dir}/commands.log" "")
    select_outputs()
    file(REMOVE "${result_file}" "${events_file}" "${telemetry_file}")
endmacro()

macro(generate_legacy name source)
    prepare_case("${name}")
    if(NOT DEFINED debug_flag)
        set(debug_flag -g)
    endif()
    run_checked("C to IR" "${YARDA_CLANG}" -std=c11 -O0
        -Xclang -disable-O0-optnone "${debug_flag}"
        -I "${YARDA_INCLUDE_DIR}" ${ARGN} -emit-llvm -S "${source}" -o "${ir}")
    run_checked("IR to LAT" "${YARDA_OPT}"
        -load-pass-plugin "${YARDA_PLUGIN}"
        "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
        "${ir}" -o /dev/null)
    run_checked("C to ET_EXEC" "${YARDA_CLANG}" -std=c11 -O0 "${debug_flag}"
        -fno-pie -no-pie -I "${YARDA_INCLUDE_DIR}" ${ARGN} "${source}"
        "-Wl,--section-start=.yarda_lines=0x600000"
        "-Wl,--section-start=.yarda_cross=0x60021c" -o "${elf}")
endmacro()

function(check_with_oracles golden)
    run_checked("generated artifact GTest" "${YARDA_VERIFY}"
        "${lat}" "${elf}" "${YARDA_CACHE}" "${result_file}"
        "${events_file}" "${golden}")
endfunction()
