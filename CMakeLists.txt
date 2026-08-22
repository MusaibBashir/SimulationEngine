# ============================================================================
# CMakeLists.txt
# ============================================================================
cmake_minimum_required(VERSION 3.10)
project(des CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Sources are LISTED, never file(GLOB)'d. CMake only re-runs a glob when it
# happens to reconfigure, so a newly added .cpp gets silently left out of the
# build and you debug a phantom linker error.
set(DES_SOURCES
    src/Entity.cpp
    src/RandomStream.cpp
    src/Distribution.cpp
    src/QueueRule.cpp
    src/Resource.cpp
    src/EntityQueue.cpp
    src/EventNotice.cpp
    src/FutureEventList.cpp
    src/Clock.cpp
    src/SystemState.cpp
    src/Activity.cpp
    src/Delay.cpp
    src/Statistics.cpp
    src/TerminationRule.cpp
    src/Experiment.cpp
    src/Trace.cpp
    src/Station.cpp
    src/Model.cpp
    src/SimulationSystem.cpp
)

add_executable(des main.cpp ${DES_SOURCES})
add_executable(des_tests tests/tests.cpp ${DES_SOURCES})

foreach(target des des_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

# `ctest` runs the suite.
enable_testing()
add_test(NAME unit_tests COMMAND des_tests)

# ---------------------------------------------------------------------------
# Building:
#   cmake -S . -B build && cmake --build build
#   ./build/des          # the five demonstration scenarios
#   ./build/des_tests    # the unit suite
#   cd build && ctest
#
# Or without CMake:
#   g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude main.cpp src/*.cpp -o des
#
# Sanitizers -- worth running before and after any refactor, because entities
# are destroyed at departure and a routing mistake becomes a use-after-free:
#   g++ -std=c++17 -g -O1 -fsanitize=address,undefined -Iinclude \
#       main.cpp src/*.cpp -o des_asan && ./des_asan
# ---------------------------------------------------------------------------
