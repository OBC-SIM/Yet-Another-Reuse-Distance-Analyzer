cmake_minimum_required(VERSION 3.20)
project(LoopAnnotatedTrace LANGUAGES C CXX)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM_DIR: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ── LLVM Pass (shared library) ────────────────────────────
add_library(LoopAnnotatedTrace MODULE
    src/ArrayMetadata.cpp
    src/IrHelpers.cpp
    src/LoopAnalysisPass.cpp
)
target_include_directories(LoopAnnotatedTrace PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(LoopAnnotatedTrace PROPERTIES
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN NO
)
if(APPLE)
    set_target_properties(LoopAnnotatedTrace PROPERTIES
        MACOSX_RPATH ON
        LINK_FLAGS "-undefined dynamic_lookup"
    )
else()
    target_link_options(LoopAnnotatedTrace PRIVATE "-Wl,--allow-shlib-undefined")
endif()

# ── GTest 단위 테스트 ─────────────────────────────────────
llvm_map_components_to_libnames(LLVM_TEST_LIBS support core analysis)
find_library(GTEST_LIB      gtest      PATHS /usr/lib/x86_64-linux-gnu)
find_library(GTEST_MAIN_LIB gtest_main PATHS /usr/lib/x86_64-linux-gnu)

if(GTEST_LIB AND GTEST_MAIN_LIB)
    enable_testing()
    add_executable(LoopAnnotatedTraceTests
        tests/Statement_test.cpp
        tests/IrHelpers_test.cpp
        tests/ArrayMetadata_test.cpp
        src/ArrayMetadata.cpp
        src/IrHelpers.cpp
    )
    target_include_directories(LoopAnnotatedTraceTests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${LLVM_INCLUDE_DIRS}
    )
    target_link_libraries(LoopAnnotatedTraceTests
        ${GTEST_LIB}
        ${GTEST_MAIN_LIB}
        ${LLVM_TEST_LIBS}
        pthread
    )
    add_test(NAME LoopAnnotatedTraceTests COMMAND LoopAnnotatedTraceTests)
else()
    message(WARNING "GTest not found — skipping unit tests")
endif()
