if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    add_executable(yarda_rtems_gr740_verify
        verify.cpp expectations.cpp
        ../../tests/cache/oracle/exact_csrd_oracle.cpp
        ../../tests/cache/oracle/hierarchy_lru_oracle.cpp)
    target_include_directories(yarda_rtems_gr740_verify PRIVATE
        "${YARDA_BACKEND_SOURCE_DIR}/src" "${YARDA_BACKEND_SOURCE_DIR}/tests")
    target_link_libraries(yarda_rtems_gr740_verify PRIVATE
        yarda_evaluation_support GTest::gtest)
    target_compile_options(yarda_rtems_gr740_verify PRIVATE -Wall -Wextra -Wpedantic)
    find_program(YARDA_RTEMS_GCC NAMES sparc-rtems6-gcc)
    if(YARDA_RTEMS_GCC)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        add_test(NAME yarda_rtems_gr740_e2e_smoke
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/smoke.py"
                "${CMAKE_BINARY_DIR}")
        set_tests_properties(yarda_rtems_gr740_e2e_smoke PROPERTIES TIMEOUT 180)
    endif()
endif()
