if(NOT DEFINED YARDA_CPP OR NOT DEFINED YARDA_INPUT OR
   NOT DEFINED YARDA_ELF OR NOT DEFINED YARDA_CACHE)
    message(FATAL_ERROR "task-mapping test inputs are required")
endif()

set(arguments "${YARDA_INPUT}" --elf "${YARDA_ELF}"
    --cache "${YARDA_CACHE}")
if(DEFINED YARDA_OUTPUT)
    file(REMOVE "${YARDA_OUTPUT}")
    list(APPEND arguments --export "${YARDA_OUTPUT}")
endif()
execute_process(COMMAND "${YARDA_CPP}" ${arguments}
                RESULT_VARIABLE result
                OUTPUT_VARIABLE output
                ERROR_VARIABLE error_output)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "task mapping failed: ${error_output}")
endif()

if(DEFINED YARDA_OUTPUT)
    file(READ "${YARDA_OUTPUT}" payload)
else()
    set(payload "${output}")
endif()
string(JSON schema_version GET "${payload}" schema_version)
string(JSON mode GET "${payload}" mode)
string(JSON basis GET "${payload}" elf address_basis)
string(JSON task_count LENGTH "${payload}" tasks)
string(JSON task_id GET "${payload}" tasks 0 task_id)
string(JSON aggregate_exclusions GET
       "${payload}" exclusions known_non_inline_static_call_sites)
string(JSON task_exclusions GET
       "${payload}" tasks 0 exclusions known_non_inline_static_call_sites)
string(JSON operation GET
       "${payload}" tasks 0 resolved_accesses 0 operation)
string(JSON resolved_address GET
       "${payload}" tasks 0 resolved_accesses 0 linked_byte_address)
string(JSON line_count LENGTH
       "${payload}" tasks 0 mapped_line_references)
string(JSON first_tag GET
       "${payload}" tasks 0 mapped_line_references 0 tag)
string(JSON first_set GET
       "${payload}" tasks 0 mapped_line_references 0 set_index)
string(JSON first_offset GET
       "${payload}" tasks 0 mapped_line_references 0 line_offset)
string(JSON second_address GET
       "${payload}" tasks 0 mapped_line_references 1 linked_byte_address)
string(JSON second_set GET
       "${payload}" tasks 0 mapped_line_references 1 set_index)
string(JSON second_span GET
       "${payload}" tasks 0 mapped_line_references 1 line_span_ordinal)
string(JSON complete GET "${payload}" coverage complete)

if(NOT schema_version STREQUAL "1" OR
   NOT mode STREQUAL "elf-task-mapping" OR
   NOT basis STREQUAL "linked_absolute" OR
   NOT task_count STREQUAL "1" OR
   NOT task_id STREQUAL "mapped_kernel" OR
   NOT aggregate_exclusions STREQUAL "1" OR
   NOT task_exclusions STREQUAL "1" OR
   NOT operation STREQUAL "store" OR
   NOT resolved_address STREQUAL "6291484" OR
   NOT line_count STREQUAL "2" OR
   NOT first_tag STREQUAL "768" OR
   NOT first_set STREQUAL "0" OR
   NOT first_offset STREQUAL "28" OR
   NOT second_address STREQUAL "6291488" OR
   NOT second_set STREQUAL "1" OR
   NOT second_span STREQUAL "1" OR
   NOT complete)
    message(FATAL_ERROR "unexpected task-mapping payload: ${payload}")
endif()
