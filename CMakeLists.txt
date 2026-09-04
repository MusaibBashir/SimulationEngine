# ============================================================================
# CMakeLists.txt
# ============================================================================
cmake_minimum_required(VERSION 3.10)
project(des CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# The engine, built ONCE as a static library.
#
# v5 fix: every target used to list ${DES_SOURCES} directly, so with 12 targets
# CMake compiled all 19 engine sources 12 times -- 228 compilations for a clean
# build, and minutes of waiting. A library target compiles them once. This is
# the same advice examples/README.md gives for hand-rolled builds, and the
# project should not have been ignoring its own documentation.
# ----------------------------------------------------------------------------
add_library(des_engine STATIC
    src/Entity.cpp
    src/Value.cpp
    src/Lexer.cpp
    src/EvalContext.cpp
    src/VariableStore.cpp
    src/Expression.cpp
    src/Functions.cpp
    src/Parser.cpp
    src/ModuleSchema.cpp
    src/ModuleSchemas.cpp
    src/ModelDocument.cpp
    src/DocumentFormat.cpp
    src/Compiler.cpp
    src/RandomStream.cpp
    src/StreamTests.cpp
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
    src/Trace.cpp
    src/Station.cpp
    src/Nodes.cpp
    src/Create.cpp
    src/Node.cpp
    src/Model.cpp
    src/SimulationSystem.cpp
    src/Experiment.cpp
)
# Sources are LISTED, never file(GLOB)'d: CMake only re-runs a glob when it
# happens to reconfigure, so a newly added .cpp gets silently left out and you
# debug a phantom linker error.

# PUBLIC, so anything linking des_engine gets the include path automatically.
target_include_directories(des_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    # /EHsc is NOT optional. Without it MSVC compiles with unwind semantics
    # disabled, so a throw that has to unwind -- v10 catches a distribution
    # constructor's ModelError to turn it into a Diagnostic -- calls abort()
    # instead, and on Windows abort() opens a MODAL DIALOG that an automated
    # run has nobody to click. The engine has thrown since v5; the setting was
    # simply never stated, and the compiler had been warning C4530 about it.
    set(DES_WARNINGS /W4 /EHsc)
else()
    set(DES_WARNINGS -Wall -Wextra -Wpedantic)
endif()
target_compile_options(des_engine PRIVATE ${DES_WARNINGS})

# ----------------------------------------------------------------------------
# Programs
# ----------------------------------------------------------------------------
add_executable(des main.cpp)
target_link_libraries(des PRIVATE des_engine)
target_compile_options(des PRIVATE ${DES_WARNINGS})

add_executable(des_tests tests/tests.cpp tests/harness.cpp tests/expression_tests.cpp tests/document_tests.cpp)
target_link_libraries(des_tests PRIVATE des_engine)
target_compile_options(des_tests PRIVATE ${DES_WARNINGS})

# One executable per example, all into build/examples/.
# GLOB is used HERE and nowhere else: examples are standalone demos, so a newly
# added one silently missing is a nuisance rather than a phantom link error.
file(GLOB EXAMPLE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp)
foreach(example ${EXAMPLE_SOURCES})
    get_filename_component(name ${example} NAME_WE)
    add_executable(${name} ${example})
    target_link_libraries(${name} PRIVATE des_engine)
    target_compile_options(${name} PRIVATE ${DES_WARNINGS})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
endforeach()

enable_testing()
add_test(NAME unit_tests COMMAND des_tests)

# ---------------------------------------------------------------------------
#   cmake -S . -B build && cmake --build build
#   ./build/des              the demonstration scenarios
#   ./build/des_tests        the unit suite  (or: cd build && ctest)
#   ./build/examples/01_hello_mm1
#
# Without CMake:
#   g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude main.cpp src/*.cpp -o des
#
# Sanitizers -- worth running before and after any refactor, because entities
# are destroyed at departure and a routing mistake becomes a use-after-free:
#   g++ -std=c++17 -g -O1 -fsanitize=address,undefined -Iinclude \
#       main.cpp src/*.cpp -o des_asan && ./des_asan
# ---------------------------------------------------------------------------
