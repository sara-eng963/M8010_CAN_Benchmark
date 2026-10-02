cmake_minimum_required(VERSION 3.16)
project(M8010_CAN_Benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(m8010_can_model
    src/benchmark/CanFrameTiming.cpp
    src/benchmark/M8010Profile.cpp
    src/benchmark/CanBusSimulator.cpp
)
target_include_directories(m8010_can_model PUBLIC src/benchmark)
target_compile_options(m8010_can_model PRIVATE -Wall -Wextra -Wpedantic)

add_executable(m8010_can_benchmark src/benchmark/main.cpp)
target_link_libraries(m8010_can_benchmark PRIVATE m8010_can_model)
target_compile_options(m8010_can_benchmark PRIVATE -Wall -Wextra -Wpedantic)

add_executable(m8010_error_sweep src/benchmark/error_sweep.cpp)
target_link_libraries(m8010_error_sweep PRIVATE m8010_can_model)
target_compile_options(m8010_error_sweep PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(test_can_timing tests/test_can_timing.cpp)
target_link_libraries(test_can_timing PRIVATE m8010_can_model)
add_test(NAME can_timing COMMAND test_can_timing)
