#!/usr/bin/env bash
# ============================================================================
# tools/verify.sh  --  the verification gate, in one command
# ============================================================================
# Through v9 this was a line in README.md that people retyped:
#
#     g++ -std=c++17 -g -O1 -fsanitize=address,undefined ...
#
# On this machine that command cannot work, and v10 is where that stopped
# being ignorable. The reasons are worth writing down, because "the sanitiser
# said nothing" and "the sanitiser did not run" look identical in a terminal.
#
#   * The MinGW GCC 6.3 that was on PATH has no <variant> and no <optional>,
#     so it cannot compile v10 at all.
#   * MinGW GCC 14.2 compiles it, but MinGW ships no libasan and no libubsan.
#   * MinGW Clang 19 compiles it, but has no sanitiser runtime for the
#     x86_64-w64-windows-gnu target either.
#   * MSVC has AddressSanitizer and it works. MSVC has no UBSan.
#
#   * WSL's Linux GCC has BOTH sanitiser runtimes. That is where UBSan lives.
#
# So the gate is: warnings clean under two independent front ends,
# AddressSanitizer clean under MSVC, and ASan + UBSan clean under WSL. The
# Windows-side UBSan gap is real and is worked around, not ignored -- if WSL is
# missing the leg SKIPS loudly rather than passing silently.
#
# Usage:  bash tools/verify.sh            # everything
#         bash tools/verify.sh warnings   # just the two compilers
#         bash tools/verify.sh asan       # just MSVC AddressSanitizer
#         bash tools/verify.sh sanitisers # just WSL ASan + UBSan
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

# --- find the compilers ----------------------------------------------------
# WINLIBS_BIN may be set to override. Otherwise look where winget puts it,
# then fall back to whatever is on PATH.
if [ -z "${WINLIBS_BIN:-}" ]; then
    WINLIBS_BIN="$(ls -d "$LOCALAPPDATA"/Microsoft/WinGet/Packages/*WinLibs*/mingw64/bin 2>/dev/null | head -1)"
fi
GXX="${WINLIBS_BIN:+$WINLIBS_BIN/g++.exe}"
CLANGXX="${WINLIBS_BIN:+$WINLIBS_BIN/clang++.exe}"
[ -x "$GXX" ]     || GXX="$(command -v g++ || true)"
[ -x "$CLANGXX" ] || CLANGXX="$(command -v clang++ || true)"

# -fsyntax-only: this leg wants DIAGNOSTICS, not a binary. Skipping codegen and
# linking makes it roughly three times faster, and every -Wall/-Wextra warning
# that matters here is a front-end one. Link errors are still caught -- the
# CMake/MSVC build does that, and the ASan leg links the whole suite.
WARN_FLAGS="-std=c++17 -Wall -Wextra -Wpedantic -fsyntax-only -Iinclude -Itests"
# TWO link units, because both have a main(): the demo program and the test
# suite. Compiling only the first would leave tests/ unchecked by GCC and
# Clang -- and from v10 on, tests/ is where most of the new code lives.
SOURCES="main.cpp src/*.cpp"
TEST_SOURCES="tests/tests.cpp tests/harness.cpp tests/expression_tests.cpp src/*.cpp"
FAILED=0

banner() { printf '\n=== %s ===\n' "$1"; }

check_version() {
    # Refuse to run a compiler too old for the C++17 library. GCC 6 reports
    # -std=c++17 as supported and then fails on <variant>, which is exactly
    # the kind of half-truth this script exists to catch.
    local cc="$1" name="$2"
    local major
    major="$("$cc" -dumpversion 2>/dev/null | cut -d. -f1)"
    case "$name" in
        gcc)   [ "${major:-0}" -ge 7 ]  && return 0 ;;
        clang) [ "${major:-0}" -ge 5 ]  && return 0 ;;
    esac
    printf 'SKIP: %s is version %s -- too old for the C++17 library\n' "$cc" "${major:-unknown}"
    return 1
}

# One compiler, one link unit. $3 is a label so a failure names which of the
# two builds broke.
compile_unit() {
    local cc="$1" label="$2" unit="$3" sources="$4"
    local out; out="$(mktemp)"
    # shellcheck disable=SC2086
    if "$cc" $WARN_FLAGS $sources 2>"$out"; then
        if grep -q "warning:" "$out"; then
            grep "warning:" "$out" | head -20
            printf '%s (%s): WARNINGS\n' "$label" "$unit"; FAILED=1
        else
            printf '%s (%s): clean\n' "$label" "$unit"
        fi
    else
        head -30 "$out"
        printf '%s (%s): BUILD FAILED\n' "$label" "$unit"; FAILED=1
    fi
    rm -f "$out"
}

warnings() {
    if [ -n "$GXX" ] && check_version "$GXX" gcc; then
        banner "GCC $("$GXX" -dumpversion) -Wall -Wextra -Wpedantic"
        compile_unit "$GXX" GCC engine "$SOURCES"
        compile_unit "$GXX" GCC tests  "$TEST_SOURCES"
    fi

    if [ -n "$CLANGXX" ] && check_version "$CLANGXX" clang; then
        banner "Clang $("$CLANGXX" -dumpversion) -Wall -Wextra -Wpedantic"
        compile_unit "$CLANGXX" Clang engine "$SOURCES"
        compile_unit "$CLANGXX" Clang tests  "$TEST_SOURCES"
    fi
}

asan() {
    banner "MSVC AddressSanitizer"
    # PowerShell, not bash: Git Bash rewrites /fsanitize=address into a
    # filesystem path and cl.exe then reports "cannot open source file
    # 'C:/Program'". MSYS_NO_PATHCONV would also work; this is clearer.
    powershell.exe -NoProfile -Command '
        $ErrorActionPreference = "Stop"
        # Configure ONCE. A full reconfigure per run cost more than the build
        # it was preparing for; CMake re-runs itself when CMakeLists.txt
        # changes, so reusing the cache is safe.
        if (-not (Test-Path "cmake-build-asan/CMakeCache.txt")) {
            cmake -S . -B cmake-build-asan -DCMAKE_CXX_FLAGS="/fsanitize=address /EHsc /Zi" | Out-Null
        }
        cmake --build cmake-build-asan --target des_tests --config Debug 2>&1 |
            Select-String -Pattern "error|warning C" | Select-Object -First 10
        $msvc = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64" -Directory -ErrorAction SilentlyContinue |
                Select-Object -First 1
        if ($msvc) { $env:PATH = "$($msvc.FullName);$env:PATH" }
        & ".\cmake-build-asan\Debug\des_tests.exe"
        exit $LASTEXITCODE
    ' 2>&1 | tail -6
    if [ "${PIPESTATUS[0]}" -ne 0 ]; then
        printf 'ASan: FAILED\n'; FAILED=1
    else
        printf 'ASan: clean\n'
    fi
}

# The only place on this machine where UBSan actually exists. MinGW ships no
# libubsan and MSVC has none at all, but WSL's Linux GCC has both sanitisers --
# so the check the README has claimed since v2 can finally be run for real.
# Skipped, loudly, if WSL or the distro is absent.
sanitisers() {
    banner "WSL Linux GCC: AddressSanitizer + UndefinedBehaviorSanitizer"
    if ! command -v wsl.exe > /dev/null 2>&1; then
        printf 'SKIP: no wsl.exe on PATH\n'; return
    fi
    if ! wsl.exe -d Ubuntu -e bash -lc 'command -v g++' > /dev/null 2>&1; then
        printf 'SKIP: no Ubuntu distro with g++ (wsl -l -v to check)\n'; return
    fi
    local out
    out="$(wsl.exe -d Ubuntu -e bash -lc "cd '$(wslpath -a "$ROOT" 2>/dev/null || echo .)' && \
        g++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
            -Iinclude -Itests tests/*.cpp src/*.cpp -o /tmp/des_san 2>&1 | head -20 && \
        cd /tmp && ./des_san 2>&1 | grep -E 'runtime error|ERROR: |SUMMARY|checks passed|FAIL'" \
        2>&1 | tr -d '\000')"
    printf '%s\n' "$out"
    if printf '%s' "$out" | grep -qE 'runtime error|ERROR: |FAIL'; then
        printf 'Sanitisers: FAILED\n'; FAILED=1
    elif printf '%s' "$out" | grep -q 'checks passed'; then
        printf 'Sanitisers: clean (ASan + UBSan)\n'
    else
        printf 'Sanitisers: INCONCLUSIVE -- no result line\n'; FAILED=1
    fi
}

case "${1:-all}" in
    warnings)   warnings ;;
    asan)       asan ;;
    sanitisers) sanitisers ;;
    all)        warnings; asan; sanitisers ;;
    *)          printf 'usage: %s [all|warnings|asan|sanitisers]\n' "$0"; exit 2 ;;
esac

banner "RESULT"
if [ "$FAILED" -eq 0 ]; then printf 'VERIFY CLEAN\n'; exit 0; fi
printf 'VERIFY FAILED\n'; exit 1
