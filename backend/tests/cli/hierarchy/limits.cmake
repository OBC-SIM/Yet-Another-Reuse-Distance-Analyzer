cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
set(flags --max-single-loop-iterations --max-cumulative-loop-iterations
          --max-source-accesses --max-line-references)
set(exact 2 4 4 8)
set(lower 1 3 3 7)
set(all --max-single-loop-iterations 2 --max-cumulative-loop-iterations 4
        --max-source-accesses 4 --max-line-references 8)
run_ok(${base} ${all})
file(READ "${result_file}" expected)
foreach(index RANGE 0 3)
    list(GET flags ${index} flag)
    list(GET lower ${index} low)
    run_error("exceeds|limit|budget" ${base} ${all} ${flag} ${low}
        --export-events "${events_file}" --event-limit 8
        --telemetry "${telemetry_file}")
    run_ok(${base} ${all} ${flag} 18446744073709551615)
    file(READ "${result_file}" actual)
    if(NOT expected STREQUAL actual)
        message(FATAL_ERROR "work allowance changed RESULT: ${flag}")
    endif()
endforeach()
# Empty loops still reserve work, even when emission limits are zero.
file(READ "${map}" raw)
string(JSON empty GET "${raw}" functions 2)
string(JSON empty SET "${empty}" body [=[[
  {"type":"Loop","var":"i","start":0,"bound":1000001,"step":1,"body":[]}
]]=])
string(JSON raw SET "${raw}" functions "[${empty}]")
file(WRITE "${map}" "${raw}")
run_error("exceeds|limit|budget" ${base} --max-source-accesses 0 --max-line-references 0)
run_ok(${base} --max-source-accesses 0 --max-line-references 0
    --max-single-loop-iterations 1000001 --max-cumulative-loop-iterations 1000001)
