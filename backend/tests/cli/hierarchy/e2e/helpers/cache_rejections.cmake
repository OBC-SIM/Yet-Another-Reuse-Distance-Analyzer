file(READ "${YARDA_CACHE}" cache)
string(FIND "${cache}" "  - name: LLC" boundary)
if(boundary LESS 0)
    message(FATAL_ERROR "cache fixture lost its LLC section")
endif()
string(SUBSTRING "${cache}" 0 ${boundary} l1_part)
string(SUBSTRING "${cache}" ${boundary} -1 llc_part)

function(reject_cache_change part from to reason)
    string(REPLACE "${from}" "${to}" changed "${${part}}")
    if(changed STREQUAL "${${part}}")
        message(FATAL_ERROR "cache rejection did not modify ${from}")
    endif()
    if(part STREQUAL "l1_part")
        set(invalid "${changed}${llc_part}")
    else()
        set(invalid "${l1_part}${changed}")
    endif()
    file(WRITE "${case_dir}/invalid.yaml" "${invalid}")
    expect_analysis_failure("${reason}" ${base} ${diagnostics}
        --cache "${case_dir}/invalid.yaml")
endfunction()

foreach(entry "l1_part|L1" "llc_part|LLC")
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 part)
    list(GET fields 1 cache_name)
    reject_cache_change(${part} "replacement: LRU" "replacement: FIFO"
        "error: analysis requires replacement=LRU for cache: ${cache_name}(\n|$)")
    reject_cache_change(${part} "write_allocate: true" "write_allocate: false"
        "error: analysis requires write_allocate=true for cache: ${cache_name}(\n|$)")
endforeach()
reject_cache_change(llc_part "line_size: 32 B" "line_size: 64 B"
    "error: analysis requires matching line_size for L1 and LLC(\n|$)")
reject_cache_change(llc_part "size_bytes: 256 B"
    "private_to: 0\n    size_bytes: 256 B"
    "error: analysis LLC must be shared with private_to omitted: LLC(\n|$)")
reject_cache_change(l1_part "next: LLC" "next: Memory"
    "error: analysis L1 next must reference an LLC cache: L1(\n|$)")
reject_cache_change(l1_part "l1: L1" "l1: LLC"
    "error: core mapping must reference its private L1: LLC(\n|$)")
reject_cache_change(l1_part "size_bytes: 128 B" "size_bytes: 96 B"
    "error: cache line size, line count, and associativity must be powers of two(\n|$)")
