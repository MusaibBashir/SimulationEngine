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
# WHY IT NO LONGER TAKES TWENTY-FIVE MINUTES (v15.2). Each figure here was
# measured on this machine -- a Ryzen 5 7235HS, 4 cores and 8 threads -- because
# the first explanation offered was only partly right, and the largest single
# cost was one nobody had named:
#
#   * The warnings leg re-parsed every file in src/ three times per compiler --
#     once each for the demo, the CLI and the test suite, as though
#     -fsyntax-only linked something. It links nothing. ~137 parses per
#     compiler, one at a time, at 1-3 s each under GCC and 1.4-5.6 s under
#     Clang. Every file is now parsed ONCE per compiler, all at once.
#
#   * The MSVC leg's cost was never its build. It was RUNNING the suite in a
#     Debug build: 327 s, against 19 s for the same suite without a sanitiser.
#     About 60% of that was the MSVC STL's iterator debugging (132 s with only
#     that switched off) and most of the rest was /Od (84 s at /O2). It now
#     builds /O2 -- with asserts KEPT: CMake's optimised flags define NDEBUG,
#     this project's asserts guard things a person can type, and the override
#     below leaves NDEBUG out, which the generated projects were read to
#     confirm. VERIFY_MSVC_CONFIG=Debug brings back the old configuration.
#
#   * Iterator debugging did not simply go. It MOVED: the WSL leg builds with
#     libstdc++'s own debug mode, _GLIBCXX_DEBUG, which catches the same class
#     of mistake -- an invalidated iterator used anyway. That costs it 84 s of
#     running instead of 32 s, and the whole suite passed under it.
#
#   * The WSL leg compiled ~50 files one after another, and a sanitised
#     Compiler.cpp alone takes ~11 s. They compile in parallel now, and an
#     object whose preprocessed source, flags and compiler are unchanged is
#     taken from a cache instead of being built again. The 9P bridge to /mnt/c
#     was a smaller cost than it looked -- 11.8 s against 10.6 s from WSL's own
#     disk for that file -- but working on a copy also keeps the suite's scratch
#     files away from the MSVC leg's.
#
#   * The three legs ran one after another. They share no files now, so they
#     run at once. With four real cores they slow each other down, and the gate
#     takes as long as its slowest leg does under that load.
#
# Usage:  bash tools/verify.sh            # everything
#         bash tools/verify.sh warnings   # just the two compilers
#         bash tools/verify.sh asan       # just MSVC AddressSanitizer
#         bash tools/verify.sh sanitisers # just WSL ASan + UBSan
#
#         VERIFY_MSVC_CONFIG=Debug bash tools/verify.sh asan
#             the MSVC leg as it was: Debug, full iterator debugging, ~4x slower
#         VERIFY_SUITE_TIMEOUT=<seconds>
#             how long a sanitised suite may run before it is called hung
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

JOBS="$(nproc 2>/dev/null || echo 4)"

export VERIFY_MSVC_CONFIG="${VERIFY_MSVC_CONFIG:-RelWithDebInfo}"
case "$VERIFY_MSVC_CONFIG" in
    RelWithDebInfo|Debug) ;;
    *) printf 'VERIFY_MSVC_CONFIG must be RelWithDebInfo or Debug, not %s\n' "$VERIFY_MSVC_CONFIG"; exit 2 ;;
esac

# How long a sanitised suite may run before it is called hung, in seconds.
#
# A HANG IS A FAILURE WITH A NAME, not a wait. Without this, one behaviour
# change that sent a test round a loop forever hung the whole gate, silently,
# until somebody noticed that nothing was happening -- which is exactly how it
# was found: sabotaging Key::control left the palette tests pressing Down for
# twenty minutes at full CPU. The suite takes 84 s on its own and about two
# minutes under the load of the other legs, so fifteen is far outside it and
# still an answer. The Debug MSVC configuration runs four times slower and
# gets three times as long.
export SUITE_TIMEOUT="${VERIFY_SUITE_TIMEOUT:-900}"
case "$SUITE_TIMEOUT" in
    ''|*[!0-9]*) printf 'VERIFY_SUITE_TIMEOUT must be a number of seconds, not %s\n' "$SUITE_TIMEOUT"; exit 2 ;;
esac

# -fsyntax-only: this leg wants DIAGNOSTICS, not a binary. Every -Wall/-Wextra
# warning that matters here is a front-end one. Link errors are caught by the
# two sanitiser legs, which both link the whole suite.
WARN_FLAGS="-std=c++17 -Wall -Wextra -Wpedantic -fsyntax-only -Iinclude -Itests"

# EVERY file, each parsed once. The globs matter: tests/document_tests.cpp was
# added in v11 to a list spelled out by hand and NOT added here, so v11's whole
# test suite went through MSVC and nothing else while this script went on
# printing VERIFY CLEAN. The reason CMakeLists.txt does not glob -- a stale cache
# silently dropping a file -- does not apply to a script that re-expands on
# every run.
#
# tui/ is included since v15.2. Until then the Windows terminal was compiled
# only by CMake, with nobody reading its warnings, and the POSIX one by nothing.
WARN_FILES=(main.cpp cli/main.cpp tui/main.cpp tui/Terminal_win32.cpp src/*.cpp tests/*.cpp)

FAILED=0
# How many legs actually RAN. Without this the script prints VERIFY CLEAN when
# no compiler is found at all: nothing to do, FAILED stays 0, and a machine
# with no toolchain reports the same result as a clean one. A gate that
# measured nothing has not passed; it has not run.
RAN=0

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

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

# --- warnings ----------------------------------------------------------------

# One compiler, one file. Runs in its own process under xargs, so it cannot
# touch FAILED or RAN; it leaves its diagnostics and its exit code in files for
# warnings() to read back. Counters bumped in a background job are lost the
# moment the job exits, and a gate that loses its counters prints CLEAN.
check_one() {
    local label="$1" file="$2" cc log
    case "$label" in
        GCC)   cc="$GXX" ;;
        Clang) cc="$CLANGXX" ;;
        *)     return 2 ;;
    esac
    log="$WORK/warn/$label/${file//\//@}.log"
    # shellcheck disable=SC2086
    "$cc" $WARN_FLAGS "$file" > "$log" 2>&1
    echo "$?" > "$log.rc"
}
export -f check_one
export GXX CLANGXX WARN_FLAGS WORK

warnings() {
    local labels=() names="" l f rcfile log rc checked bad
    if [ -n "$GXX" ] && check_version "$GXX" gcc; then
        labels+=(GCC); names="GCC $("$GXX" -dumpversion)"
    fi
    if [ -n "$CLANGXX" ] && check_version "$CLANGXX" clang; then
        labels+=(Clang); names="${names:+$names and }Clang $("$CLANGXX" -dumpversion)"
    fi
    banner "${names:-no compiler}: -Wall -Wextra -Wpedantic, every file once"
    if [ "${#labels[@]}" -eq 0 ]; then
        printf 'SKIP: no usable GCC or Clang\n'
        return
    fi

    for l in "${labels[@]}"; do mkdir -p "$WORK/warn/$l"; done
    for l in "${labels[@]}"; do
        for f in "${WARN_FILES[@]}"; do printf '%s %s\n' "$l" "$f"; done
    done | xargs -P "$JOBS" -n 2 bash -c 'check_one "$1" "$2"' _

    for l in "${labels[@]}"; do
        checked=0; bad=0
        for rcfile in "$WORK/warn/$l"/*.rc; do
            [ -e "$rcfile" ] || continue
            checked=$((checked + 1))
            log="${rcfile%.rc}"
            f="$(basename "$log" .log)"; f="${f//@//}"
            rc="$(cat "$rcfile")"
            if [ "$rc" != 0 ]; then
                head -30 "$log"
                printf '%s: %s DOES NOT COMPILE\n' "$l" "$f"; bad=1
            elif grep -q "warning:" "$log"; then
                grep "warning:" "$log" | head -20
                printf '%s: %s HAS WARNINGS\n' "$l" "$f"; bad=1
            fi
        done
        # Every file, or it did not pass. A job xargs never started leaves no
        # result behind, and "no complaints" from a file nobody compiled is the
        # same silence as a clean one.
        if [ "$checked" -ne "${#WARN_FILES[@]}" ]; then
            printf '%s: only %d of %d files were checked\n' "$l" "$checked" "${#WARN_FILES[@]}"
            bad=1
        fi
        if [ "$bad" -eq 0 ]; then
            printf '%s: clean (%d files)\n' "$l" "$checked"
            RAN=$((RAN + 1))
        else
            FAILED=1
        fi
    done
}

# --- MSVC AddressSanitizer -------------------------------------------------

asan() {
    banner "MSVC AddressSanitizer ($VERIFY_MSVC_CONFIG)"
    local full="$WORK/asan.full" rc
    # PowerShell, not bash: Git Bash rewrites /fsanitize=address into a
    # filesystem path and cl.exe then reports "cannot open source file
    # 'C:/Program'".
    powershell.exe -NoProfile -Command '
        $config = $env:VERIFY_MSVC_CONFIG
        $cache  = "cmake-build-asan/CMakeCache.txt"

        # Configure once, and again only when the cache lacks what the leg
        # depends on: /MP, which compiles a project'"'"'s files in parallel
        # instead of one at a time, and an optimised configuration WITHOUT
        # NDEBUG. A cached one that has NDEBUG back in it is reconfigured, not
        # trusted -- deleting the asserts is what would make it fast and wrong.
        $flags = Select-String -Path $cache -Pattern "^CMAKE_CXX_FLAGS:STRING=" -ErrorAction SilentlyContinue | Select-Object -First 1
        $opt   = Select-String -Path $cache -Pattern "^CMAKE_CXX_FLAGS_RELWITHDEBINFO:STRING=" -ErrorAction SilentlyContinue | Select-Object -First 1
        $fresh = $flags -and ($flags.Line -match "/MP") -and
                 $opt -and ($opt.Line -match "/O2") -and ($opt.Line -notmatch "NDEBUG")
        if (-not $fresh) {
            cmake -S . -B cmake-build-asan "-DCMAKE_CXX_FLAGS=/fsanitize=address /EHsc /Zi /MP" "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=/O2 /Ob1 /Zi" | Out-Null
        }

        # THE BUILD'"'"'S EXIT CODE IS CHECKED. It was not: a build that failed
        # printed its errors, then the old des_tests.exe from the last good build
        # ran and reported clean -- a stale binary passing the gate, the same
        # hole baseline.sh once had.
        $build = cmake --build cmake-build-asan --target des_tests --config $config --parallel 2>&1
        $code = $LASTEXITCODE
        $build | Select-String -Pattern "error C|fatal error|error LNK|warning C" | Select-Object -First 10
        if ($code -ne 0) {
            "ERROR: the ASan build failed -- not running a stale des_tests.exe"
            exit 1
        }

        $msvc = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64" -Directory -ErrorAction SilentlyContinue |
                Select-Object -First 1
        if ($msvc) { $env:PATH = "$($msvc.FullName);$env:PATH" }

        # WITH A TIME LIMIT. A test caught in a loop used to hold the gate open
        # for as long as nobody noticed; stopped, it fails the leg and names the
        # section it was in -- which is where the loop is.
        $limit = [int]$env:SUITE_TIMEOUT
        if ($config -eq "Debug") { $limit = 3 * $limit }
        $out = "cmake-build-asan\suite.out"
        $err = "cmake-build-asan\suite.err"
        $p = Start-Process -FilePath ".\cmake-build-asan\$config\des_tests.exe" -NoNewWindow -PassThru `
                           -RedirectStandardOutput $out -RedirectStandardError $err
        $null = $p.Handle
        if (-not $p.WaitForExit($limit * 1000)) {
            $p.Kill()
            Start-Sleep -Milliseconds 500
            $last = Get-Content $out | Where-Object { $_ -like "[[]*" } | Select-Object -Last 1
            "ERROR: the suite did not finish within $limit s -- it was stopped in $last"
            exit 1
        }
        Get-Content $out
        Get-Content $err
        exit $p.ExitCode
    ' > "$full" 2>&1
    rc=$?
    tail -6 "$full"
    CHECKS="$(grep -o '[0-9]* / [0-9]* checks passed' "$full" | tail -1 | awk '{print $3}')"
    if [ "$rc" -ne 0 ]; then
        printf 'ASan: FAILED\n'; FAILED=1
    else
        printf 'ASan: clean\n'
        RAN=$((RAN + 1))
    fi
}

# --- WSL ASan + UBSan --------------------------------------------------------
# The only place on this machine where UBSan actually exists. MinGW ships no
# libubsan and MSVC has none at all, but WSL's Linux GCC has both. Skipped,
# loudly, if WSL or the distro is absent.
#
# The work itself is tools/verify_wsl.sh, fed to WSL on stdin with carriage
# returns stripped -- so it runs whatever line endings git checked it out with,
# and its path never passes through Git Bash's rewriting of arguments that
# start with '/'.
sanitisers() {
    banner "WSL Linux GCC: ASan + UBSan + libstdc++ debug mode"
    if ! command -v wsl.exe > /dev/null 2>&1; then
        printf 'SKIP: no wsl.exe on PATH\n'; return
    fi
    if ! wsl.exe -d Ubuntu -e bash -lc 'command -v g++' > /dev/null 2>&1; then
        printf 'SKIP: no Ubuntu distro with g++ (wsl -l -v to check)\n'; return
    fi
    local wslroot out
    wslroot="$(MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -e wslpath -a "$(cygpath -w "$ROOT")" 2>/dev/null | tr -d '\r\000')"
    if [ -z "$wslroot" ]; then
        printf 'Sanitisers: INCONCLUSIVE -- WSL could not translate %s\n' "$ROOT"; FAILED=1
        return
    fi
    out="$(tr -d '\r' < tools/verify_wsl.sh \
           | MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -e bash -s -- "$wslroot" "$JOBS" "$SUITE_TIMEOUT" 2>&1 \
           | tr -d '\000')"
    printf '%s\n' "$out"
    CHECKS="$(printf '%s' "$out" | grep -o '[0-9]* / [0-9]* checks passed' | tail -1 | awk '{print $3}')"
    if printf '%s' "$out" | grep -qE 'runtime error|ERROR: |FAIL'; then
        printf 'Sanitisers: FAILED\n'; FAILED=1
    elif printf '%s' "$out" | grep -q 'checks passed'; then
        printf 'Sanitisers: clean (ASan + UBSan + _GLIBCXX_DEBUG)\n'
        RAN=$((RAN + 1))
    else
        printf 'Sanitisers: INCONCLUSIVE -- no result line\n'; FAILED=1
    fi
}

# --- running the legs ----------------------------------------------------------
# All at once. Each leg runs in its own background subshell, writes what it has
# to say to one file and its verdict to another, and is printed IN A FIXED ORDER
# once all of them are done -- so the report reads the same however the legs
# happened to finish, and no leg's counters are lost in a subshell.
run_legs() {
    local legs=("$@") pids=() leg i f r c s remaining finished=" "
    printf 'Running %s at once, %s threads ...\n' "${legs[*]}" "$JOBS"
    for leg in "${legs[@]}"; do
        (
            FAILED=0; RAN=0; CHECKS=""; SECONDS=0
            "$leg"
            printf '%s %s %s %s\n' "$FAILED" "$RAN" "${CHECKS:--}" "$SECONDS" > "$WORK/$leg.status"
        ) > "$WORK/$leg.out" 2>&1 &
        pids+=("$!")
    done

    remaining=${#legs[@]}
    while [ "$remaining" -gt 0 ]; do
        for i in "${!legs[@]}"; do
            leg="${legs[$i]}"
            case "$finished" in *" $leg "*) continue ;; esac
            kill -0 "${pids[$i]}" 2>/dev/null && continue
            wait "${pids[$i]}" 2>/dev/null
            finished="$finished$leg "
            remaining=$((remaining - 1))
            if [ -f "$WORK/$leg.status" ]; then
                printf '  %s finished in %ss\n' "$leg" "$(cut -d' ' -f4 "$WORK/$leg.status")"
            else
                printf '  %s stopped without a verdict\n' "$leg"
            fi
        done
        [ "$remaining" -gt 0 ] && sleep 1
    done

    local counts=()
    for leg in "${legs[@]}"; do
        cat "$WORK/$leg.out"
        if [ -f "$WORK/$leg.status" ]; then
            read -r f r c s < "$WORK/$leg.status"
            [ "$f" -ne 0 ] && FAILED=1
            RAN=$((RAN + r))
            [ "$c" != "-" ] && counts+=("$leg=$c")
        else
            printf '%s: stopped without a verdict\n' "$leg"
            FAILED=1
        fi
    done

    # MSVC and WSL run the SAME suite. If they disagree about how many checks
    # it has, one of them ran less of it -- a file its copy was missing, a
    # section that bailed out early -- and that is a failure even when both
    # report every check they did run as passing.
    if [ "${#counts[@]}" -ge 2 ]; then
        local first="${counts[0]#*=}" x
        for x in "${counts[@]}"; do
            if [ "${x#*=}" != "$first" ]; then
                printf '\nThe legs ran different numbers of checks: %s\n' "${counts[*]}"
                FAILED=1
                break
            fi
        done
    fi
}

SECONDS=0
case "${1:-all}" in
    warnings|asan|sanitisers) run_legs "$1" ;;
    all)                      run_legs warnings asan sanitisers ;;
    *) printf 'usage: %s [all|warnings|asan|sanitisers]\n' "$0"; exit 2 ;;
esac

# A FAILURE IS REPORTED BEFORE AN ABSENCE. This checked "did anything run" first,
# so a leg that ran, found a real problem and failed -- leaving nothing counted
# as clean -- was reported as "nothing was checked", which says no toolchain was
# found. The sabotage runs that proved the gate catches a warning and a broken
# build both printed that.
banner "RESULT"
if [ "$FAILED" -ne 0 ]; then
    printf 'VERIFY FAILED (%ss)\n' "$SECONDS"
    exit 1
fi
if [ "$RAN" -eq 0 ]; then
    printf 'VERIFY FAILED: nothing was checked\n'
    exit 1
fi
printf 'VERIFY CLEAN (%ss)\n' "$SECONDS"
exit 0
