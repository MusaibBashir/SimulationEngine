# ============================================================================
# CMakeLists.txt  --  five real lines
# ============================================================================
#
# [1] cmake_minimum_required(VERSION 3.10)
#
# [2] project(des CXX)
#     Naming the language explicitly stops CMake probing for a C compiler.
#
# [3] set(CMAKE_CXX_STANDARD 17)
#     set(CMAKE_CXX_STANDARD_REQUIRED ON)
#     17 gives you std::optional (for Entity::attribute later) and structured
#     bindings (pleasant when iterating a map). Bump to 20 only if you actually
#     want map::contains().
#
# [4] include_directories(include)
#     So that #include "Entity.hpp" resolves without ../include/ prefixes.
#     (The modern form is target_include_directories after the target exists.
#      Either is fine at this size; know that the modern one is preferred.)
#
# [5] add_executable(des
#         main.cpp
#         src/Entity.cpp
#         src/Resource.cpp
#         src/EntityQueue.cpp
#         src/EventNotice.cpp
#         src/FutureEventList.cpp
#         src/Clock.cpp
#         src/SystemState.cpp
#         src/Activity.cpp
#         src/Delay.cpp
#         src/Statistics.cpp
#         src/TerminationCondition.cpp
#         src/SimulationSystem.cpp
#     )
#     LIST THE SOURCES EXPLICITLY. Do not use file(GLOB ...): CMake only re-runs
#     the glob when it happens to reconfigure, so a newly added .cpp gets
#     silently left out of the build and you debug a phantom linker error.
#     You will edit this list all through v2. That is fine and intended.
#
# [6] Turn warnings on -- they are the cheapest code review available:
#     target_compile_options(des PRIVATE -Wall -Wextra -Wpedantic)
#     (MSVC uses /W4 instead; wrap in if(MSVC) if you build on Windows natively.)
#
# ---------------- BUILDING ----------------
#   cmake -S . -B build
#   cmake --build build
#   ./build/des
#
# If you would rather skip CMake for v1, a single line also works:
#   g++ -std=c++17 -Wall -Wextra -Iinclude main.cpp src/*.cpp -o des
# Use CMake anyway. Learning the build system is part of learning to ship
# software, and by v3 the one-liner will not be enough.

cmake_minimum_required(VERSION 3.10)

project(des CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(des
    main.cpp
    src/Entity.cpp
    src/Resource.cpp
    src/EntityQueue.cpp
    src/EventNotice.cpp
    src/FutureEventList.cpp
    src/Clock.cpp
    src/SystemState.cpp
    src/Activity.cpp
    src/Delay.cpp
    src/Statistics.cpp
    src/TerminationCondition.cpp
    src/SimulationSystem.cpp
)

if(MSVC)
    target_compile_options(des PRIVATE /W4)
else()
    target_compile_options(des PRIVATE -Wall -Wextra -Wpedantic)
endif()

