set(YARDA_BACKEND_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(YARDA_BACKEND_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

find_package(LLVM 14 REQUIRED CONFIG)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
