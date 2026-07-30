cmake_minimum_required(VERSION 3.20)

project(YARDA_CPP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_subdirectory(frontend)

set_target_properties(LoopAnnotatedTrace PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(TARGET LoopAnnotatedTraceTests)
    set_target_properties(LoopAnnotatedTraceTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

add_subdirectory(backend)
