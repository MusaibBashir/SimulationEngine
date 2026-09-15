#!/usr/bin/env bash
# ============================================================================
# tools/verify_wsl.sh  --  verify.sh's Linux leg, run INSIDE WSL
# ============================================================================
# verify.sh pipes this into `wsl -d Ubuntu -e bash -s`, carriage returns
# stripped, so it works whatever line endings git checked it out with.
#
#     arguments:  <the repository, as a WSL path>
#                 [how many compiles at once]  [seconds the suite may run]
#
# It works on a COPY in WSL's own filesystem. Measured on this machine, a
# sanitised Compiler.cpp took 11.8 s through the 9P bridge from /mnt/c and
# 10.6 s from /tmp: a real cost, and much smaller than the parallelism below.
# The bigger reason is that the suite writes scratch files into its working
# directory, and from a copy it cannot collide with the MSVC leg running the
# same suite on Windows at the same moment.
#
# examples/ IS copied. The suite's decisive tests read the shipped models from
# examples/models, relative to the working directory when nothing says
# otherwise -- a copy of include, src and tests alone would build, run, and
# fail those tests for a reason that has nothing to do with the code.
#
# _GLIBCXX_DEBUG is where the iterator checking lives now. The MSVC leg used to
# get it from the Debug STL, at the price of running the suite four times more
# slowly; it builds optimised instead, and libstdc++'s debug mode catches the
# same class of mistake -- an invalidated iterator used anyway -- here.
#
# Every line that should fail the gate starts "ERROR: ", which verify.sh
# already treats as a failure alongside ASan's and UBSan's own reports.
set -u

SRC="${1:?the repository path, as WSL sees it}"
JOBS="${2:-$(nproc)}"
LIMIT="${3:-900}"
WORK=/tmp/des_verify
CACHE="$HOME/.cache/des_verify"
SAN="-std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -D_GLIBCXX_DEBUG -Iinclude -Itests"
WARN="-std=c++17 -Wall -Wextra -Wpedantic -fsyntax-only -Iinclude"
TOOLCHAIN="$(g++ --version | head -1)"

t0=$(date +%s)
rm -rf "$WORK" && mkdir -p "$WORK/obj" "$CACHE" || { echo "ERROR: cannot create $WORK or $CACHE"; exit 1; }
( cd "$SRC" && tar -cf - include src tests examples tui ) | tar -C "$WORK" -xf - \
    || { echo "ERROR: could not copy the tree from $SRC"; exit 1; }
cd "$WORK" || exit 1
t1=$(date +%s)

# THE OBJECT CACHE. Compiling everything took 156 s on four cores, every run,
# for a tree where most runs change a file or two. An object is reused when the
# compiler version, the flags and the PREPROCESSED source -- every header it
# pulls in, expanded -- are byte for byte what they were when it was built. That
# is the same thing the compiler sees, so a reused object is the object the
# compiler would have produced.
#
# Keyed on content, NOT on timestamps. make-style mtime checks are fooled by a
# file restored with an older time than its object -- `cp -p`, an archive, a
# stash -- and a gate that links a stale object has tested code that is not in
# the tree. Preprocessing still costs a second or two a file; building the
# sanitised object costs ten.
compile_one() {
    local f="$1" o="obj/${1//\//@}.o" pre key
    # shellcheck disable=SC2086
    if ! pre="$(g++ $SAN -E "$f" 2> "$o.log")"; then
        echo "ERROR: $f does not compile"
        head -20 "$o.log"
        return
    fi
    key="$(printf '%s\n%s\n%s' "$TOOLCHAIN" "$SAN" "$pre" | sha1sum | cut -c1-40)"
    if [ -f "$CACHE/$key.o" ] && cp "$CACHE/$key.o" "$o"; then
        touch "$CACHE/$key.o" "$o.hit"
        return
    fi
    # shellcheck disable=SC2086
    if ! g++ $SAN -c "$f" -o "$o" > "$o.log" 2>&1; then
        echo "ERROR: $f does not compile"
        head -20 "$o.log"
        return
    fi
    # Into the cache under a temporary name and then renamed, so a run stopped
    # half way through a copy cannot leave a truncated object to be reused.
    cp "$o" "$CACHE/$key.o.$$" && mv "$CACHE/$key.o.$$" "$CACHE/$key.o"
}
export -f compile_one
export SAN CACHE TOOLCHAIN

sources=(src/*.cpp tests/*.cpp)
stamp="$WORK/cache.stamp"
touch "$stamp"
printf '%s\n' "${sources[@]}" | xargs -P "$JOBS" -I{} bash -c 'compile_one "$1"' _ {}
built=$(find obj -name '*.o' | wc -l)
if [ "$built" -ne "${#sources[@]}" ]; then
    echo "ERROR: built $built of ${#sources[@]} objects"
    exit 1
fi
hits=$(find obj -name '*.hit' | wc -l)
# Keep only what this run used. The cache holds one tree's objects, not every
# tree's ever, so it cannot grow without bound across branches and edits.
find "$CACHE" -name '*.o' ! -newer "$stamp" -delete 2>/dev/null
t2=$(date +%s)

# The POSIX terminal and tui/main.cpp. No other leg compiles them -- the
# Windows compilers take the Win32 terminal -- so until v15.2 a mistake in
# either went unseen until somebody built on Linux.
# shellcheck disable=SC2086
posix="$(g++ $WARN tui/Terminal_posix.cpp tui/main.cpp 2>&1)"
posix_rc=$?
if [ "$posix_rc" -ne 0 ] || printf '%s' "$posix" | grep -q 'warning:'; then
    printf '%s\n' "$posix" | head -20
    echo "ERROR: the POSIX terminal does not compile cleanly"
else
    echo "POSIX terminal and tui/main.cpp: clean"
fi

g++ -fsanitize=address,undefined obj/*.o -o des_san > link.log 2>&1 \
    || { head -20 link.log; echo "ERROR: the sanitised suite did not link"; exit 1; }
t3=$(date +%s)

# WITH A TIME LIMIT, and when it runs out, the section it stopped in -- which is
# where the loop is. Without one, a test sent round a loop forever held the whole
# gate open for as long as nobody noticed.
timeout -k 30 "$LIMIT" ./des_san < /dev/null > run.log 2>&1
rc=$?
t4=$(date +%s)
grep -E 'runtime error|ERROR: |SUMMARY|checks passed|FAIL|error: attempt' run.log
if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
    echo "ERROR: the sanitised suite did not finish within ${LIMIT}s -- it was stopped in $(grep '^\[' run.log | tail -1)"
elif [ "$rc" -ne 0 ]; then
    echo "ERROR: the sanitised suite exited $rc"
fi
echo "(copy $((t1 - t0))s, compile $((t2 - t1))s with $hits of ${#sources[@]} from cache, link $((t3 - t2))s, run $((t4 - t3))s, $JOBS at a time)"
