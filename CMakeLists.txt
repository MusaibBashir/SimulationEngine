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
    src/RunSetup.cpp
    src/RunController.cpp
    src/Regression.cpp
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
    src/Transfer.cpp
    src/Match.cpp
    src/Schedule.cpp
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
# v17: models from a description, through a small language model. A THIRD
# library, linking des_engine and never the reverse, so the engine stays
# offline and network-free for everybody who never sets a key. Portable code
# in src/ for the same reason des_ui's is: verify.sh then runs it through
# every compiler and sanitiser with no change to any gate.
# ----------------------------------------------------------------------------
add_library(des_ai STATIC
    src/Json.cpp
    src/Scenario.cpp
    src/ScenarioBuilder.cpp
    src/PreParse.cpp
    src/ModelGenerator.cpp
    src/CurlTransport.cpp
    src/Speech.cpp
)
target_link_libraries(des_ai PUBLIC des_engine)
target_compile_options(des_ai PRIVATE ${DES_WARNINGS})

# ----------------------------------------------------------------------------
# The user interface, as a SECOND library. des_ui links des_engine and never
# the reverse -- that is the whole of the layering, and it is what a repository
# boundary would otherwise have been protecting.
#
# The portable half lives in src/ deliberately: tools/verify.sh compiles
# src/*.cpp and its WSL leg compiles tests/*.cpp src/*.cpp, so putting it here
# runs the entire UI through GCC, Clang, ASan and UBSan with no change to any
# gate script. Only platform code lives in tui/, and platform code cannot be
# tested anyway.
# ----------------------------------------------------------------------------
add_library(des_ui STATIC
    src/Screen.cpp
    src/TextBuffer.cpp
    src/TuiState.cpp
    src/TuiRender.cpp
    src/TuiInput.cpp
    src/TableStore.cpp
    src/TuiTables.cpp
    src/FlowGraph.cpp
)
# v17: des_ui links des_ai for F2, describe-a-model. Still one direction:
# nothing in des_ai or des_engine knows a screen exists.
target_link_libraries(des_ui PUBLIC des_engine des_ai)
target_compile_options(des_ui PRIVATE ${DES_WARNINGS})

# ----------------------------------------------------------------------------
# Programs
# ----------------------------------------------------------------------------
add_executable(des_demo main.cpp)
target_link_libraries(des_demo PRIVATE des_engine)
target_compile_options(des_demo PRIVATE ${DES_WARNINGS})

# The CLI takes the name `des`: `des check model.des` is the product and the
# five-scenario program above is a demo.
add_executable(des cli/main.cpp)
target_link_libraries(des PRIVATE des_ai)
target_compile_options(des PRIVATE ${DES_WARNINGS})

# The platform half. WIN32 and non-WIN32 sources are mutually exclusive: each
# includes headers the other platform does not have. Terminal.hpp is not in
# des_ui.hpp -- it is the platform boundary, and only tui/ includes it.
if(WIN32)
    add_executable(des_tui tui/main.cpp tui/Terminal_win32.cpp tui/Microphone.cpp)
else()
    add_executable(des_tui tui/main.cpp tui/Terminal_posix.cpp tui/Microphone.cpp)
endif()
target_link_libraries(des_tui PRIVATE des_ui)
target_compile_options(des_tui PRIVATE ${DES_WARNINGS})

# v17: the microphone. miniaudio is one vendored header, included as a SYSTEM
# directory so its own warnings do not count against this project's -- the
# gate is about code written here. What it needs from the platform:
#   Windows  nothing at link time (WASAPI is loaded when recording starts)
#   macOS    Core Audio, linked rather than loaded (MA_NO_RUNTIME_LINKING)
#   Linux    dlopen and threads, for whichever sound server is there
function(des_use_microphone target)
    target_include_directories(${target} SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio)
    if(APPLE)
        target_link_libraries(${target} PRIVATE "-framework CoreFoundation" "-framework CoreAudio"
                                                "-framework AudioToolbox")
    elseif(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS} Threads::Threads m)
    endif()
endfunction()
des_use_microphone(des_tui)
if(WIN32)
    # shell32 and ole32 for SHGetKnownFolderPath and CoTaskMemFree, which find
    # the user's REAL Documents folder -- redirected into OneDrive on a great
    # many laptops, where %USERPROFILE%\Documents is an empty folder nobody looks
    # in. uuid holds the FOLDERID constants in MinGW. All three are Windows' own
    # libraries, so a static build still needs nothing installed.
    target_link_libraries(des_tui PRIVATE shell32 ole32 uuid)
endif()

# ----------------------------------------------------------------------------
# v17: the desktop app, in Qt 6 Widgets. OFF by default, so building the
# engine, the CLI, the terminal UI and the tests still needs no Qt at all:
#   cmake -S . -B build-gui -DDES_BUILD_GUI=ON -DCMAKE_PREFIX_PATH=<Qt>/lib/cmake
# One design on Windows and macOS: Qt's Fusion style with one palette
# (gui/Theme.cpp), not each platform's native widgets.
# ----------------------------------------------------------------------------
option(DES_BUILD_GUI "Build the Qt desktop app (DES-Simulator)" OFF)
if(DES_BUILD_GUI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
    set(DES_GUI_VERSION "17.0.0" CACHE STRING "The version the Mac bundle reports")
    # Qt 6.8's WrapOpenGL still links Apple's AGL framework, which newer
    # Xcode SDKs no longer ship ("ld: framework 'AGL' not found"). Nothing
    # here uses AGL -- it was deprecated in 10.9 -- so drop it from the link.
    if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
        get_target_property(_des_gl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
        if(_des_gl_libs)
            list(FILTER _des_gl_libs EXCLUDE REGEX "AGL")
            set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${_des_gl_libs}")
        endif()
    endif()
    add_executable(des_gui
        gui/main.cpp
        gui/MainWindow.cpp
        gui/ModelEditor.cpp
        gui/DesHighlighter.cpp
        gui/FlowView.cpp
        gui/ResultsView.cpp
        gui/AskPanel.cpp
        gui/SettingsDialog.cpp
        gui/Theme.cpp
        tui/Microphone.cpp
    )
    set_target_properties(des_gui PROPERTIES
        AUTOMOC ON
        OUTPUT_NAME "DES-Simulator"
        WIN32_EXECUTABLE ON          # a window, no console behind it
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.dessimulator.app"
        MACOSX_BUNDLE_BUNDLE_NAME "DES Simulator"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${DES_GUI_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${DES_GUI_VERSION}"
        # CMake's template plus the microphone line macOS insists on.
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in")
    target_link_libraries(des_gui PRIVATE des_ui Qt6::Widgets)
    # The engine has a Schedule::slots(); Qt's `slots`, `signals` and `emit`
    # macros would rewrite it. The GUI spells them Q_SLOTS, Q_SIGNALS, Q_EMIT.
    target_compile_definitions(des_gui PRIVATE QT_NO_KEYWORDS)
    target_compile_options(des_gui PRIVATE ${DES_WARNINGS})
    des_use_microphone(des_gui)
    if(WIN32)
        target_link_libraries(des_gui PRIVATE shell32 ole32 uuid)
    endif()
endif()

add_executable(des_tests tests/tests.cpp tests/harness.cpp tests/expression_tests.cpp tests/document_tests.cpp tests/runtime_tests.cpp tests/tui_tests.cpp tests/v16_tests.cpp tests/tables_tests.cpp tests/flowgraph_tests.cpp tests/scenario_tests.cpp tests/ask_tests.cpp)
target_link_libraries(des_tests PRIVATE des_ui)
target_compile_options(des_tests PRIVATE ${DES_WARNINGS})

# The decisive v11 test reads the shipped .des files. The suite is not always
# run from the repository root -- the sanitiser leg runs it from /tmp -- so the
# directory is compiled in rather than assumed.
target_compile_definitions(des_tests PRIVATE
    DES_MODEL_DIR=""\"${CMAKE_CURRENT_SOURCE_DIR}/examples/models\""")

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
#   ./build/des_demo         the demonstration scenarios
#   ./build/des check m.des  compile a model file and report
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
