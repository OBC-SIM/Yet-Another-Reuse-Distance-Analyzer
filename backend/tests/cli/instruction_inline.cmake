file(MAKE_DIRECTORY "${WORK_DIR}")
execute_process(COMMAND "${CLANG}" -O0 -g -Xclang -disable-O0-optnone
    -S -emit-llvm "${SOURCE}" -o "${WORK_DIR}/inline.ll"
    RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "IR compilation failed: ${error}")
endif()
execute_process(COMMAND "${CLANG}" -O0 -fno-pie -no-pie
    "${SOURCE}" -o "${WORK_DIR}/inline.elf"
    RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "ELF compilation failed: ${error}")
endif()
execute_process(COMMAND "${OPT}" "-load-pass-plugin=${PLUGIN}"
    "-passes=function(mem2reg),loop-simplify,loop-annotated-trace"
    inline.ll -disable-output WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "MAP extraction failed: ${error}")
endif()
execute_process(COMMAND "${CLI}" "${WORK_DIR}/inline_ape.json"
    --analysis hierarchy-rd --analysis ir-instructions
    --elf "${WORK_DIR}/inline.elf" --cache "${CACHE}"
    --telemetry "${WORK_DIR}/telemetry.json" --export "${WORK_DIR}/result.json"
    RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Combined inline analysis failed: ${error}")
endif()
file(READ "${WORK_DIR}/result.json" result)
file(READ "${WORK_DIR}/telemetry.json" telemetry)
string(JSON loads GET "${result}" ir_instructions total opcodes load dynamic)
string(JSON accesses GET "${telemetry}" source_accesses_emitted)
if(NOT loads EQUAL 21 OR NOT loads EQUAL accesses)
    message(FATAL_ERROR "Expected 7 * 3 loads shared by IR/cache: ${loads}/${accesses}")
endif()
string(JSON phi ERROR_VARIABLE phi_error GET "${result}" ir_instructions total opcodes phi)
if(NOT phi_error)
    message(FATAL_ERROR "PHIs must be excluded")
endif()
string(JSON invocations GET "${result}" ir_instructions functions 0 inline_callees 0 invocations)
if(NOT invocations EQUAL 7)
    message(FATAL_ERROR "Expected seven inline invocations: ${result}")
endif()
