#!/usr/bin/env bash
# ============================================================================
# tools/package_macos.sh  --  build the macOS package people can be sent
# ============================================================================
# The macOS twin of tools/package.sh. Produces, in dist/:
#
#     DES-Simulator/                               the staged folder
#     DES-Simulator-<ver>-macos-universal.zip      unzip and double-click
#     DES-Simulator-<ver>-macos-universal.dmg      the same folder as a disk image
#
# Usage:  bash tools/package_macos.sh [version]
#
# The same three refusals as the Windows script: the optimised build must pass
# the whole suite, link against nothing but the system, and start with nothing
# else on PATH. And one more that only a Mac needs: the binary must hold BOTH an
# Apple Silicon and an Intel slice, because a package sent to "a Mac" is sent to
# either, and the person receiving it does not know which they have.
set -u

VERSION="${1:-0.0.0}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

fail() { printf '\nPACKAGE FAILED: %s\n' "$1"; exit 1; }

[ "$(uname -s)" = "Darwin" ] || fail "this script builds the macOS package and must run on macOS"

BUILD="build-release-macos"
DIST="dist/DES-Simulator"
BIN="des-simulator"

# --- 1. an optimised, universal build ----------------------------------------
# -O2 WITHOUT -DNDEBUG, for the reason package.sh gives: the asserts guard
# things a person can type, and deleting them makes wrong runs, not errors.
#
# 11.0 is the oldest macOS that runs on Apple Silicon at all, and std::filesystem
# needs 10.15, so nothing older is possible and nothing newer is needed.
printf '=== release build (universal, -O2, asserts kept) ===\n'
cmake -S . -B "$BUILD" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS_RELEASE="-O2" \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 > /dev/null || fail "cmake configure"
cmake --build "$BUILD" --target des_tui des_tests -j "$(sysctl -n hw.ncpu)" 2>&1 \
    | grep -E "error|warning" | head -20
[ -x "$BUILD/des_tui" ] || fail "des_tui did not build"
[ -x "$BUILD/des_tests" ] || fail "des_tests did not build"

# --- 2. the optimised build passes the same suite -----------------------------
printf '\n=== the suite, on the build that ships ===\n'
suite="$("$BUILD/des_tests" 2>&1)"
status=$?
printf '%s\n' "$suite" | grep -E "FAIL|checks passed"
[ $status -eq 0 ] || fail "the release test suite failed (exit $status)"
printf '%s' "$suite" | grep -q "FAIL" && fail "the release test suite reported failures"
printf '%s' "$suite" | grep -q "checks passed" || fail "no result line from the suite"

# --- 3. both architectures, and nothing but the system ------------------------
printf '\n=== architectures ===\n'
archs="$(lipo -archs "$BUILD/des_tui")"
printf '  %s\n' "$archs"
case " $archs " in *" arm64 "*) ;; *) fail "no Apple Silicon slice in des_tui" ;; esac
case " $archs " in *" x86_64 "*) ;; *) fail "no Intel slice in des_tui" ;; esac

printf '\n=== linked libraries ===\n'
# Only the INDENTED lines are libraries. A universal binary prints a heading
# per architecture -- "des_tui (architecture x86_64):" -- and the first version
# of this skipped only the first heading, read the second as a library, and
# refused a binary that was fine.
libs="$(otool -L "$BUILD/des_tui" | grep -E '^[[:space:]]' | awk '{print $1}' | sort -u)"
printf '%s\n' "$libs" | sed 's/^/  /'
# libc++ and libSystem are part of macOS itself. Anything outside /usr/lib and
# /System would have to be installed on the other Mac -- Homebrew's libc++ is the
# usual way that happens without anybody noticing.
unexpected="$(printf '%s\n' "$libs" | grep -vE '^(/usr/lib/|/System/)' || true)"
[ -z "$unexpected" ] || fail "des_tui needs libraries other Macs will not have: $unexpected"
printf '  all system libraries\n'

# --- 4. stage -------------------------------------------------------------------
printf '\n=== staging %s ===\n' "$DIST"
rm -rf "$DIST"
mkdir -p "$DIST/examples" || fail "could not create $DIST"
cp "$BUILD/des_tui" "$DIST/$BIN"
# Ad-hoc signed. Apple Silicon refuses to run an arm64 binary with no signature
# at all; the linker signs it, but cp on some filesystems and any later strip
# can invalidate that, so it is signed again here, on the file that ships.
codesign --force --sign - "$DIST/$BIN" || fail "could not ad-hoc sign $BIN"
cp examples/models/*.des "$DIST/examples/"
sed "s/@VERSION@/$VERSION/" packaging/README_macos.txt > "$DIST/README.txt"

# Double-clicking a bare Unix executable in Finder opens Terminal in the home
# folder, where the program would then scatter untitled.des. A .command file is
# what Finder runs in Terminal ON PURPOSE, and it can choose the folder first --
# the same Documents/DES Models the Windows build uses.
cat > "$DIST/DES Simulator.command" <<'EOF'
#!/bin/bash
# Double-click this file to start DES Simulator in Terminal.
HERE="$(cd "$(dirname "$0")" && pwd)"
MODELS="$HOME/Documents/DES Models"
mkdir -p "$MODELS" && cd "$MODELS" || exit 1
printf '\033]0;DES Simulator\007'
exec "$HERE/des-simulator" "$@"
EOF
chmod +x "$DIST/DES Simulator.command" "$DIST/$BIN"
ls -la "$DIST" "$DIST/examples" | sed 's/^/  /'

# --- 5. it starts with nothing else near it -----------------------------------
# Copied out of the repository, run with PATH cut down to the system's own
# folders and stdin not a terminal. The correct behaviour is to say it needs a
# terminal and exit 1 -- which it can only do if the executable actually loaded.
printf '\n=== starts on a bare PATH ===\n'
bare="$(mktemp -d)"
cp "$DIST/$BIN" "$bare/"
out="$(cd "$bare" && env -i PATH="/usr/bin:/bin" ./"$BIN" < /dev/null 2>&1)"
code=$?
rm -rf "$bare"
printf '  exit %s: %s\n' "$code" "$(printf '%s' "$out" | head -1)"
[ $code -eq 1 ] || fail "the binary did not start and refuse cleanly (exit $code)"
printf '%s' "$out" | grep -qi "terminal" || fail "the binary loaded but did not say it needs a terminal"

# --- 6. the zip -------------------------------------------------------------------
# ditto, not zip: it keeps the executable bits, without which the person who
# unzips it gets a .command file Finder will not run.
ZIP="dist/DES-Simulator-$VERSION-macos-universal.zip"
printf '\n=== %s ===\n' "$ZIP"
rm -f "$ZIP"
ditto -c -k --keepParent "$DIST" "$ZIP" || fail "could not write the zip"
unzip -l "$ZIP" | sed 's/^/  /'

# --- 7. the disk image ------------------------------------------------------------
# hdiutil on a busy machine sometimes reports "Resource busy" and succeeds on a
# second try; three tries, then a real failure.
DMG="dist/DES-Simulator-$VERSION-macos-universal.dmg"
printf '\n=== %s ===\n' "$DMG"
rm -f "$DMG"
made=0
for attempt in 1 2 3; do
    if hdiutil create -quiet -volname "DES Simulator $VERSION" -srcfolder "$DIST" \
                      -ov -format UDZO "$DMG"; then
        made=1; break
    fi
    printf '  hdiutil attempt %s failed, retrying\n' "$attempt"
    sleep 5
done
[ $made -eq 1 ] || fail "could not create the disk image"
ls -la "$ZIP" "$DMG" | sed 's/^/  /'

printf '\nPACKAGE CLEAN\n'
