if(NOT DEFINED YARDA_CPP OR NOT DEFINED YARDA_INPUT OR
   NOT DEFINED EXPECTED_ERROR)
    message(FATAL_ERROR "YARDA_CPP, YARDA_INPUT, and EXPECTED_ERROR are required")
endif()

set(arguments "${YARDA_INPUT}")
if(DEFINED YARDA_ELF)
    list(APPEND arguments --elf "${YARDA_ELF}")
endif()
if(DEFINED YARDA_CACHE)
    list(APPEND arguments --cache "${YARDA_CACHE}")
endif()
if(DEFINED YARDA_GRANULARITY)
    list(APPEND arguments --granularity "${YARDA_GRANULARITY}")
endif()
if(DEFINED YARDA_EXPORT)
    list(APPEND arguments --export "${YARDA_EXPORT}")
endif()

execute_process(
    COMMAND "${YARDA_CPP}" ${arguments}
    RESULT_VARIABLE result
    ERROR_VARIABLE error_output
)
if(result EQUAL 0)
    message(FATAL_ERROR "CLI unexpectedly succeeded")
endif()
if(NOT error_output MATCHES "${EXPECTED_ERROR}")
    message(FATAL_ERROR "unexpected error: ${error_output}")
endif()
