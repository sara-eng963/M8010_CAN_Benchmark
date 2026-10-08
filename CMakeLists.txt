cmake_minimum_required(VERSION 3.20)
project(M8010_CAN_Benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SILKIT_NETSIM "Build the SIL Kit network-simulator adapter" OFF)

add_library(can_benchmark_core
    src/Can/CanFrameTiming.cpp
    src/Can/DeterministicCanBus.cpp
    src/benchmark/BenchmarkRunner.cpp
)
target_include_directories(can_benchmark_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(can_benchmark_core PRIVATE /W4)
else()
    target_compile_options(can_benchmark_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(can_benchmark can_benchmark.cpp)
target_link_libraries(can_benchmark PRIVATE can_benchmark_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(can_bus_tests tests/CanBusTests.cpp)
    target_link_libraries(can_bus_tests PRIVATE can_benchmark_core)
    add_test(NAME can_bus_tests COMMAND can_bus_tests)
endif()

if(BUILD_SILKIT_NETSIM)
    find_package(SilKit CONFIG REQUIRED)
    add_executable(SilKitDemoNetSim
        NetSimDemo.cpp
        src/MySimulatedNetwork.cpp
        src/Scheduler.cpp
        src/Can/MySimulatedCanController.cpp
        src/Can/CanFrameTiming.cpp
    )
    target_include_directories(SilKitDemoNetSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(SilKitDemoNetSim PRIVATE SilKit::SilKit)
endif()
