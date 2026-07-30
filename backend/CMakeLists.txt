add_library(yarda_backend
    src/calls.cpp
    src/dilation.cpp
    src/merger.cpp
    src/predictor.cpp
    src/predictor_3d.cpp
    src/profile.cpp
    src/schema.cpp
    src/trace.cpp
)

target_include_directories(yarda_backend PUBLIC include)
target_link_libraries(yarda_backend PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(yarda_backend PRIVATE -Wall -Wextra -Wpedantic)

add_executable(yarda_cpp src/main.cpp)
target_link_libraries(yarda_cpp PRIVATE yarda_backend)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    add_executable(yarda_backend_tests
        tests/calls_test.cpp
        tests/dilation_test.cpp
        tests/merger_test.cpp
        tests/predictor_test.cpp
        tests/profile_test.cpp
        tests/schema_test.cpp
        tests/trace_test.cpp
    )
    target_link_libraries(yarda_backend_tests PRIVATE yarda_backend GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(yarda_backend_tests)
endif()
