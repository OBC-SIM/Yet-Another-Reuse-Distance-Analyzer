cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/helpers/generate.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/helpers/frontend_failure.cmake")
set(source "${YARDA_REPOSITORY}/frontend/tests/fixtures/legacy_affine_rejected.c")
foreach(index "n*i" "i*i" "i/2" "i%2" "(signed char)(40*i)" "n+1")
    string(MAKE_C_IDENTIFIER "${index}" case)
    prepare_case("${case}")
    run_checked("rejected affine C to IR" "${YARDA_CLANG}" -O0
        -Xclang -disable-O0-optnone -g "-DH2_INDEX=${index}"
        -emit-llvm -S "${source}" -o "${ir}")
    expect_frontend_failure("unsupported affine index"
        "${YARDA_OPT}" "-load-pass-plugin=${YARDA_PLUGIN}"
        "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
        "${ir}" -S -o "${case_dir}/output.ll")
    if(EXISTS "${case_dir}/output.ll")
        message(FATAL_ERROR "rejected affine pass left host output")
    endif()
endforeach()
