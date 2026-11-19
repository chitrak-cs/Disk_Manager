cmake_minimum_required(VERSION 3.20)
project(sysmon VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(sysmon_core
    src/utils.cpp src/format.cpp src/memory.cpp src/disk.cpp
    src/gpu.cpp src/gpu_amd.cpp src/gpu_nvidia.cpp)
target_include_directories(sysmon_core PUBLIC include)
target_compile_options(sysmon_core PUBLIC -Wall -Wextra)
target_link_libraries(sysmon_core PUBLIC ${CMAKE_DL_LIBS})

add_executable(sysmon src/main.cpp)
target_link_libraries(sysmon PRIVATE sysmon_core)

add_executable(test_parsers tests/test_parsers.cpp)
target_link_libraries(test_parsers PRIVATE sysmon_core)
enable_testing()
add_test(NAME parsers COMMAND test_parsers)
