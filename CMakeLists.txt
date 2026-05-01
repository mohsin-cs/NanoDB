cmake_minimum_required(VERSION 3.16)
project(NanoDB CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include headers
include_directories(include)

# Main NanoDB executable
add_executable(nanodb
    src/main.cpp
)

# Test runner executable
add_executable(test_runner
    tests/test_runner.cpp
)

# Windows: disable deprecation warnings for fopen etc.
if(MSVC)
    target_compile_definitions(nanodb    PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(test_runner PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
