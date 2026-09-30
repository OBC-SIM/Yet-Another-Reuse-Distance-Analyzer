if(NOT TARGET yarda_region_map)
    message(FATAL_ERROR "Hierarchy experiments require YARDA_BUILD_REGION_FRONTEND=ON")
endif()

add_library(yarda_evaluation_support
    driver/inputs.cpp driver/batch_adapter.cpp driver/report.cpp
    driver/verification.cpp driver/source_expectations.cpp driver/measurements.cpp)
target_include_directories(yarda_evaluation_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(yarda_evaluation_support PRIVATE
    "${YARDA_BACKEND_SOURCE_DIR}/src"
    "${YARDA_BACKEND_BINARY_DIR}/src/cli-generated")
target_link_libraries(yarda_evaluation_support PUBLIC yarda_cli_support)
target_compile_options(yarda_evaluation_support PRIVATE -Wall -Wextra -Wpedantic)
add_dependencies(yarda_evaluation_support yarda_cli_version)

add_executable(yarda_hierarchy_evaluate driver/main.cpp)
target_include_directories(yarda_hierarchy_evaluate PRIVATE "${YARDA_BACKEND_SOURCE_DIR}/src")
target_link_libraries(yarda_hierarchy_evaluate PRIVATE yarda_evaluation_support)
target_compile_options(yarda_hierarchy_evaluate PRIVATE -Wall -Wextra -Wpedantic)

find_program(YARDA_EVALUATION_CLANG NAMES clang-14 REQUIRED)
string(TOUPPER "${CMAKE_BUILD_TYPE}" YARDA_EVALUATION_CONFIG)
set(YARDA_EVALUATION_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${YARDA_EVALUATION_CONFIG}}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/environment.cmake" CONTENT
"set(YARDA_EVALUATOR [==[$<TARGET_FILE:yarda_hierarchy_evaluate>]==])
set(YARDA_CPP [==[$<TARGET_FILE:yarda_cpp>]==])
set(YARDA_REGION [==[$<TARGET_FILE:yarda_region_map>]==])
set(YARDA_CLANG [==[${YARDA_EVALUATION_CLANG}]==])
set(YARDA_ROOT [==[${CMAKE_SOURCE_DIR}]==])
set(YARDA_COMPILER [==[${CMAKE_CXX_COMPILER}]==])
set(YARDA_BUILD_TYPE [==[${CMAKE_BUILD_TYPE}]==])
set(YARDA_BUILD_FLAGS [==[${YARDA_EVALUATION_FLAGS}]==])
")

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    add_executable(yarda_evaluation_tests
        ../tests/experiments/evaluation_test.cpp ../tests/experiments/report_test.cpp)
    target_include_directories(yarda_evaluation_tests PRIVATE
        "${YARDA_BACKEND_SOURCE_DIR}/src" "${YARDA_BACKEND_SOURCE_DIR}/tests")
    target_link_libraries(yarda_evaluation_tests PRIVATE
        yarda_evaluation_support GTest::gtest_main)
    target_compile_options(yarda_evaluation_tests PRIVATE -Wall -Wextra -Wpedantic)
    include(GoogleTest)
    gtest_discover_tests(yarda_evaluation_tests)
    foreach(case smoke failures)
        add_test(NAME yarda_hierarchy_evaluation_${case}
            COMMAND ${CMAKE_COMMAND}
                "-DYARDA_BUILD_DIR=${CMAKE_BINARY_DIR}"
                "-DYARDA_EXPERIMENTS=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DYARDA_WORK=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts"
                -P "${YARDA_BACKEND_SOURCE_DIR}/tests/experiments/${case}.cmake")
        set_tests_properties(yarda_hierarchy_evaluation_${case} PROPERTIES TIMEOUT 120)
    endforeach()
endif()

add_subdirectory(rtems_gr740)
