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

# One executable per example, all in build/examples/.
file(GLOB EXAMPLE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp)
foreach(example ${EXAMPLE_SOURCES})
    get_filename_component(name ${example} NAME_WE)
    add_executable(${name} ${example} ${DES_SOURCES})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
# GLOB is used HERE and nowhere else: examples are standalone demos, so a newly
# added one silently missing from the build is a nuisance rather than a phantom
# linker error. The engine's own sources are still listed explicitly above.

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
