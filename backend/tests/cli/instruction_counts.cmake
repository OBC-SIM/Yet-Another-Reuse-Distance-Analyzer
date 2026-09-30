cmake_policy(SET CMP0054 NEW)

file(MAKE_DIRECTORY "${WORK_DIR}")
file(WRITE "${WORK_DIR}/count.ll" [=[
define void @kernel() {
entry: br label %header
header:
  %i = phi i64 [0, %entry], [%next, %body]
  %test = icmp slt i64 %i, 3
  br i1 %test, label %body, label %exit
body:
  %next = add nsw i64 %i, 1
  br label %header
exit: ret void
}
]=])
execute_process(COMMAND "${OPT}" "-load-pass-plugin=${PLUGIN}"
    -passes=loop-annotated-trace count.ll -disable-output
    WORKING_DIRECTORY "${WORK_DIR}" RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "IR extraction failed: ${error}")
endif()
execute_process(COMMAND "${CLI}" "${WORK_DIR}/count_ape.json"
    --analysis ir-instructions
    RESULT_VARIABLE status OUTPUT_VARIABLE result ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "IR counting failed: ${error}")
endif()
string(JSON count GET "${result}" total dynamic_instructions)
if(NOT count EQUAL 16)
    message(FATAL_ERROR "Expected 16 dynamic instructions: ${result}")
endif()
execute_process(COMMAND "${CLI}" "${WORK_DIR}/count_ape.json"
    --analysis ir-instructions --export "${WORK_DIR}/result.json"
    RESULT_VARIABLE status ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "IR export failed: ${error}")
endif()
file(READ "${WORK_DIR}/result.json" exported)
string(STRIP "${result}" result)
string(STRIP "${exported}" exported)
if(NOT result STREQUAL exported)
    message(FATAL_ERROR "stdout and exported IR counts differ")
endif()
file(WRITE "${WORK_DIR}/legacy.json" "[{\"function\":\"old\",\"body\":[]}]")
execute_process(COMMAND "${CLI}" "${WORK_DIR}/legacy.json"
    --analysis ir-instructions RESULT_VARIABLE status ERROR_VARIABLE error)
if(status EQUAL 0 OR NOT error MATCHES "regenerate MAP")
    message(FATAL_ERROR "Legacy MAP must request regeneration: ${error}")
endif()
foreach(flag --elf --cache --granularity --max-source-accesses)
    execute_process(COMMAND "${CLI}" "${WORK_DIR}/count_ape.json"
        --analysis ir-instructions "${flag}" 1
        RESULT_VARIABLE status ERROR_VARIABLE error)
    if(status EQUAL 0)
        message(FATAL_ERROR "Accepted incompatible flag ${flag}")
    endif()
endforeach()

file(READ "${WORK_DIR}/count_ape.json" map)
string(JSON map SET "${map}" functions 0 annotations "[\"ape.analyze\"]")
file(WRITE "${WORK_DIR}/count_ape.json" "${map}")
foreach(mode mapping hierarchy-rd)
    foreach(ir_first TRUE FALSE)
        if(ir_first)
            set(analyses --analysis ir-instructions --analysis "${mode}")
        else()
            set(analyses --analysis "${mode}" --analysis ir-instructions)
        endif()
        execute_process(COMMAND "${CLI}" "${WORK_DIR}/count_ape.json"
            ${analyses} --elf "${ELF}" --cache "${CACHE}"
            --export "${WORK_DIR}/combined.json"
            RESULT_VARIABLE status ERROR_VARIABLE error)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "Combined ${mode} failed: ${error}")
        endif()
        file(READ "${WORK_DIR}/combined.json" combined)
        string(JSON count GET "${combined}" ir_instructions total dynamic_instructions)
        if(NOT count EQUAL 16)
            message(FATAL_ERROR "Combined result lost IR counts: ${combined}")
        endif()
    endforeach()
endforeach()

# Optional IR failures must preserve the cache document in both combined modes.
foreach(failure missing malformed unsupported overflow)
    if(failure STREQUAL "missing")
        string(JSON invalid REMOVE "${map}" functions 0 ir_instructions)
    elseif(failure STREQUAL "malformed")
        string(JSON invalid SET "${map}" functions 0 ir_instructions blocks 0 name "null")
    elseif(failure STREQUAL "unsupported")
        string(JSON invalid SET "${map}" functions 0 ir_instructions status "\"unsupported\"")
        string(JSON invalid SET "${invalid}" functions 0 ir_instructions reason "\"unproved trip count\"")
    else()
        string(JSON invalid SET "${map}" functions 0 ir_instructions blocks 1 executions "18446744073709551615")
    endif()
    file(WRITE "${WORK_DIR}/invalid.json" "${invalid}")
    execute_process(COMMAND "${CLI}" "${WORK_DIR}/invalid.json"
        --analysis ir-instructions RESULT_VARIABLE status ERROR_VARIABLE error)
    if(status EQUAL 0)
        message(FATAL_ERROR "Standalone IR must reject ${failure}")
    endif()
    foreach(mode mapping hierarchy-rd)
        execute_process(COMMAND "${CLI}" "${WORK_DIR}/invalid.json"
            --analysis "${mode}" --elf "${ELF}" --cache "${CACHE}"
            --export "${WORK_DIR}/cache.json" RESULT_VARIABLE status)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "Cache baseline failed")
        endif()
        file(READ "${WORK_DIR}/cache.json" baseline)
        execute_process(COMMAND "${CLI}" "${WORK_DIR}/invalid.json"
            --analysis "${mode}" --analysis ir-instructions
            --elf "${ELF}" --cache "${CACHE}"
            --export "${WORK_DIR}/partial.json"
            RESULT_VARIABLE status ERROR_VARIABLE error)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "${mode} discarded cache results on ${failure}: ${error}")
        endif()
        file(READ "${WORK_DIR}/partial.json" partial)
        string(JSON ir_status GET "${partial}" ir_instructions status)
        string(JSON reason GET "${partial}" ir_instructions reason)
        string(JSON total ERROR_VARIABLE no_total GET "${partial}" ir_instructions total)
        if(NOT ir_status STREQUAL "error" OR reason STREQUAL "" OR NOT no_total)
            message(FATAL_ERROR "Invalid failure report: ${partial}")
        endif()
        string(JSON partial REMOVE "${partial}" ir_instructions)
        string(JSON baseline GET "${baseline}")
        if(NOT partial STREQUAL baseline)
            message(FATAL_ERROR "${mode} cache result changed on ${failure}")
        endif()
    endforeach()
endforeach()
