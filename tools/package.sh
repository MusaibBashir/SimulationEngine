#!/usr/bin/env bash
# ============================================================================
# tools/package.sh  --  build the Windows package people can be sent
# ============================================================================
# Produces, in dist/:
#
#     DES-Simulator/                       the staged folder
#     DES-Simulator-<ver>-windows-x64.zip  unzip anywhere and double-click
#     DES-Simulator-<ver>-setup.exe        an installer, if Inno Setup 6 is here
#
# Usage:  bash tools/package.sh [version]
#
# It REFUSES to produce a package unless the optimised build passes the whole
# test suite, links against nothing a stranger's laptop lacks, and starts with
# no compiler anywhere on PATH. A zip that has not been through those three is
# a zip that has not been checked, and this project's gates exist because
# unchecked things were wrong.
set -u

VERSION="${1:-15.1.0}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

fail() { printf '\nPACKAGE FAILED: %s\n' "$1"; exit 1; }

# --- the toolchain -----------------------------------------------------------
if [ -z "${WINLIBS_BIN:-}" ]; then
    WINLIBS_BIN="$(ls -d "$LOCALAPPDATA"/Microsoft/WinGet/Packages/*WinLibs*/mingw64/bin 2>/dev/null | head -1)"
fi
[ -x "$WINLIBS_BIN/g++.exe" ] || fail "WinLibs GCC not found; set WINLIBS_BIN"

# WinLibs FIRST on PATH. This machine also has an older C:\MinGW\bin earlier on
# PATH, whose libstdc++-6.dll lacks std::filesystem -- so a dynamically linked
# test binary refused to start with no message at all, exit 127. That is the
# very failure this script exists to keep off other people's laptops, and it is
# why the package itself is linked statically below.
export PATH="$WINLIBS_BIN:$PATH"

BUILD="build-release"
DIST="dist/DES-Simulator"

# --- 1. an optimised, static build --------------------------------------------
# -O2 WITHOUT -DNDEBUG. CMake's Release flags define NDEBUG, which deletes every
# assert -- and V15_READLOG is a record of asserts guarding things a person can
# type. Deleted, they do not become errors; they become silently wrong runs.
# Kept, the cost is a few comparisons per event.
#
# -static puts libstdc++, libgcc and winpthread INSIDE the exe. Without it the
# program needs libstdc++-6.dll and libgcc_s_seh-1.dll, which exist only in a
# MinGW install, and double-clicking it anywhere else reports a missing DLL.
printf '=== release build (static, -O2, asserts kept) ===\n'
cmake -S . -B "$BUILD" -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS_RELEASE="-O2" \
      -DCMAKE_EXE_LINKER_FLAGS="-static" > /dev/null || fail "cmake configure"
cmake --build "$BUILD" --target des_tui des_tests -j 4 2>&1 \
    | grep -E "error|warning" | head -20
[ -x "$BUILD/des_tui.exe" ] || fail "des_tui.exe did not build"
[ -x "$BUILD/des_tests.exe" ] || fail "des_tests.exe did not build"

# --- 2. the optimised build passes the same suite -----------------------------
# Optimisation is where undefined behaviour that happened to work at -O0 stops
# working, so the binary that ships is tested as itself, not by proxy.
printf '\n=== the suite, on the build that ships ===\n'
suite="$("$BUILD/des_tests.exe" 2>&1)"
status=$?
printf '%s\n' "$suite" | grep -E "FAIL|checks passed"
[ $status -eq 0 ] || fail "the release test suite failed (exit $status)"
printf '%s' "$suite" | grep -q "FAIL" && fail "the release test suite reported failures"
printf '%s' "$suite" | grep -q "checks passed" || fail "no result line from the suite"

# --- 3. nothing a stranger's laptop does not already have ---------------------
printf '\n=== imported DLLs ===\n'
dlls="$("$WINLIBS_BIN/objdump.exe" -p "$BUILD/des_tui.exe" | sed -n 's/.*DLL Name: //p' | tr -d '\r')"
printf '%s\n' "$dlls" | sed 's/^/  /'
# Windows' own libraries and the Universal C Runtime, which ships with Windows
# 10 and 11. Anything else would have to be installed on the other laptop.
unexpected="$(printf '%s\n' "$dlls" | grep -viE '^(kernel32|user32|shell32|ole32|advapi32|ws2_32|ucrtbase|api-ms-win-crt-[a-z0-9-]+)\.dll$' || true)"
[ -z "$unexpected" ] || fail "des_tui.exe needs DLLs other laptops will not have: $unexpected"
printf '  all system libraries\n'

# --- 4. stage -------------------------------------------------------------------
printf '\n=== staging %s ===\n' "$DIST"
rm -rf "$DIST"
mkdir -p "$DIST/examples" || fail "could not create $DIST"
cp "$BUILD/des_tui.exe" "$DIST/DES-Simulator.exe"
cp examples/models/*.des "$DIST/examples/"
sed "s/@VERSION@/$VERSION/" packaging/README.txt | sed 's/$/\r/' > "$DIST/README.txt"
ls -la "$DIST" "$DIST/examples" | sed 's/^/  /'

# --- 5. it starts with no compiler anywhere near it ----------------------------
# Copied OUT of the repository and run with PATH cut down to Windows' own
# folders, so no MinGW DLL can be found by accident. stdin is not a console
# here, so the correct behaviour is to say it needs one and exit 1 -- which it
# can only do if the executable actually loaded.
printf '\n=== starts on a bare PATH ===\n'
bare="$(mktemp -d)"
cp "$DIST/DES-Simulator.exe" "$bare/"
out="$(cd "$bare" && PATH="/c/Windows/System32:/c/Windows" ./DES-Simulator.exe < /dev/null 2>&1)"
code=$?
rm -rf "$bare"
printf '  exit %s: %s\n' "$code" "$(printf '%s' "$out" | head -1)"
[ $code -ne 127 ] || fail "the exe did not load on a bare PATH (exit 127: a missing DLL)"
printf '%s' "$out" | grep -qi "console" || fail "the exe loaded but did not say it needs a console"

# --- 6. the zip -------------------------------------------------------------------
# Windows' own bsdtar, by its full path -- in Git Bash a bare `tar` is GNU tar,
# which cannot write zip files. NOT Compress-Archive, and NOT .NET's ZipFile:
# on this machine both wrote entry names with BACKSLASHES, which the zip format
# does not allow. Windows Explorer copes, so it looked fine; unzip on a Mac or a
# Linux box extracts "DES-Simulator\DES-Simulator.exe" as ONE file with a
# backslash in its name, so a zip forwarded through anybody else's computer
# arrived broken. Hence the check after it, not just the switch.
ZIP="dist/DES-Simulator-$VERSION-windows-x64.zip"
TAR="/c/Windows/System32/tar.exe"
printf '\n=== %s ===\n' "$ZIP"
[ -x "$TAR" ] || fail "no $TAR -- Windows 10 (1803) and later ship it"
rm -f "$ZIP"
( cd dist && MSYS_NO_PATHCONV=1 "$TAR" -a -c -f "$(basename "$ZIP")" DES-Simulator ) \
    || fail "could not write the zip"
# Read from the zip's OWN BYTES, not through `tar -tf`: bsdtar turns backslashes
# into forward slashes when it LISTS an archive, so the first version of this
# check listed a Compress-Archive zip that was full of them and stayed quiet.
# Entry names are stored uncompressed, so a folder name followed by a backslash
# is either in the file or it is not.
zip_stores_backslashes() { grep -a -F -q 'DES-Simulator\' "$1"; }
if zip_stores_backslashes "$ZIP"; then
    fail "the zip stores backslashes in its entry names"
fi
MSYS_NO_PATHCONV=1 "$TAR" -tf "$ZIP" | sed 's/^/  /'
ls -la "$ZIP" | sed 's/^/  /'

# --- 7. the installer, when Inno Setup is installed ---------------------------
ISCC="/c/Program Files (x86)/Inno Setup 6/ISCC.exe"
if [ -x "$ISCC" ]; then
    printf '\n=== installer ===\n'
    # MSYS_NO_PATHCONV: Git Bash rewrites any argument that starts with '/' into
    # a Windows path, so "/DAppVersion=15.1.0" reached ISCC as a SECOND script
    # file name -- "You may not specify more than one script filename". The
    # same trap verify.sh documents for MSVC's /fsanitize, met again.
    MSYS_NO_PATHCONV=1 "$ISCC" /Q "/DAppVersion=$VERSION" installer/des-simulator.iss \
        || fail "Inno Setup could not compile the installer"
    ls -la "dist/DES-Simulator-$VERSION-setup.exe" | sed 's/^/  /'
else
    printf '\n(no Inno Setup 6 found -- skipping the installer; the zip is complete on its own)\n'
fi

printf '\nPACKAGE CLEAN\n'
