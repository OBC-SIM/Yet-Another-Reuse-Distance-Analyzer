if(NOT DEFINED YARDA_CPP OR NOT DEFINED YARDA_INPUT)
    message(FATAL_ERROR "YARDA_CPP and YARDA_INPUT are required")
endif()

execute_process(
    COMMAND "${YARDA_CPP}" "${YARDA_INPUT}" --mode predict
    RESULT_VARIABLE result
    ERROR_VARIABLE error_output
)

if(result EQUAL 0)
    message(FATAL_ERROR "predict mode unexpectedly succeeded")
endif()

if(NOT error_output MATCHES "unknown mode: predict")
    message(FATAL_ERROR "unexpected error: ${error_output}")
endif()
