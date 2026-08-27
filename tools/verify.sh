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
# So the gate is: warnings clean under TWO independent front ends, and
# AddressSanitizer clean under MSVC. UBSan is genuinely unavailable here --
# not skipped, not silently passing. Do not add it back without checking that
# a runtime actually links.
#
# Usage:  bash tools/verify.sh            # everything
#         bash tools/verify.sh warnings   # just the two compilers
#         bash tools/verify.sh asan       # just AddressSanitizer
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

WARN_FLAGS="-std=c++17 -Wall -Wextra -Wpedantic -Iinclude -Itests"
SOURCES="main.cpp src/*.cpp"
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

warnings() {
    local out
    out="$(mktemp)"

    if [ -n "$GXX" ] && check_version "$GXX" gcc; then
        banner "GCC $("$GXX" -dumpversion) -Wall -Wextra -Wpedantic"
        # shellcheck disable=SC2086
        if "$GXX" $WARN_FLAGS $SOURCES -o "$out.exe" 2>"$out"; then
            if grep -q "warning:" "$out"; then
                grep "warning:" "$out" | head -20
                printf 'GCC: WARNINGS\n'; FAILED=1
            else
                printf 'GCC: clean\n'
            fi
        else
            head -30 "$out"; printf 'GCC: BUILD FAILED\n'; FAILED=1
        fi
    fi

    if [ -n "$CLANGXX" ] && check_version "$CLANGXX" clang; then
        banner "Clang $("$CLANGXX" -dumpversion) -Wall -Wextra -Wpedantic"
        # shellcheck disable=SC2086
        if "$CLANGXX" $WARN_FLAGS $SOURCES -o "$out.exe" 2>"$out"; then
            if grep -q "warning:" "$out"; then
                grep "warning:" "$out" | head -20
                printf 'Clang: WARNINGS\n'; FAILED=1
            else
                printf 'Clang: clean\n'
            fi
        else
            head -30 "$out"; printf 'Clang: BUILD FAILED\n'; FAILED=1
        fi
    fi

    rm -f "$out" "$out.exe"
}

asan() {
    banner "MSVC AddressSanitizer"
    # PowerShell, not bash: Git Bash rewrites /fsanitize=address into a
    # filesystem path and cl.exe then reports "cannot open source file
    # 'C:/Program'". MSYS_NO_PATHCONV would also work; this is clearer.
    powershell.exe -NoProfile -Command '
        $ErrorActionPreference = "Stop"
        cmake -S . -B cmake-build-asan -DCMAKE_CXX_FLAGS="/fsanitize=address /EHsc /Zi" | Out-Null
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

case "${1:-all}" in
    warnings) warnings ;;
    asan)     asan ;;
    all)      warnings; asan ;;
    *)        printf 'usage: %s [all|warnings|asan]\n' "$0"; exit 2 ;;
esac

banner "RESULT"
if [ "$FAILED" -eq 0 ]; then printf 'VERIFY CLEAN\n'; exit 0; fi
printf 'VERIFY FAILED\n'; exit 1
