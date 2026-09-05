#!/usr/bin/env bash
# ============================================================================
# tools/baseline.sh  --  the byte-identical gate
# ============================================================================
# This project's strongest claim is that a refactor which changes behaviour
# shows up as a DIFF rather than as a slightly-off average. v10 rewrites how
# every duration and condition is evaluated, so that claim is the whole safety
# net: fifteen example programs must print exactly what they printed under v9.
#
# capture   run every example, store its output under tests/baseline/
# check     run every example, diff against the stored output
#
# Run `capture` ONCE, on v9, before any v10 code exists. After that only ever
# run `check` -- re-capturing after a change is how a gate silently stops
# being a gate.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

BASELINE_DIR="tests/baseline"

# ---------------------------------------------------------------------------
# EXCLUDED, AND WHY. Not a convenience list -- read before adding to it.
# ---------------------------------------------------------------------------
# 12_shared_resources produces a DIFFERENT result on every run despite being
# seeded with 2718u. Six consecutive runs gave six distinct outputs, while
# every other example is byte-stable across runs.
#
# What is known, measured on v9 before any v10 code existed:
#   * Lines 1-17 -- the model description, visit ratios and the stability
#     check -- are IDENTICAL between runs. The setup is reproducible.
#   * Divergence begins at line 18, the first run result. The SIMULATION
#     diverges, not the analysis around it.
#   * So it is not the unordered_map<const INode*, double> in VisitRatios,
#     which was the obvious suspect: pointer-keyed iteration order varies with
#     ASLR, but it would have shown up above line 18, and does not.
#   * The queue disciplines are deterministic (QueueRule.cpp resolves ties to
#     the earliest index, deliberately), and Resource::m_users is a vector in
#     construction order, so neither explains it.
#
# This VIOLATES one of the project's own design rules -- "One source of
# randomness. Same seed, same run, or you cannot debug it." It is a v9 bug,
# predating v10, and it needs its own investigation. An uninitialised read is
# the leading hypothesis and would fit: it varies per process, and neither
# AddressSanitizer nor any sanitiser available on this machine detects one.
#
# It is excluded rather than quietly tolerated, and `check` PRINTS the
# exclusion every time, because a gate that silently skips a case is how a
# gate stops being a gate.
EXCLUDED="12_shared_resources"
# ---------------------------------------------------------------------------
# WHERE THE BINARIES ARE, and why this is not a constant.
# ---------------------------------------------------------------------------
# This was hardcoded to build/examples/Debug, which is where the Visual Studio
# generator puts them. The build directory was later reconfigured with MinGW
# Makefiles, which puts them in build/examples -- and the Visual Studio output
# from before the switch was still sitting there. So the gate went on running
# binaries built weeks earlier, printed BASELINE CLEAN, and proved nothing
# about the code that had changed since.
#
# A gate pointed at a stale artefact is the same failure as a test that cannot
# fail: it reports a success it never measured. Hence both of the following --
# ask the generator where it puts things, and refuse to run binaries older than
# the sources they were built from.
EXAMPLE_DIR="build/examples"
if grep -qi 'CMAKE_GENERATOR:INTERNAL=Visual Studio' build/CMakeCache.txt 2>/dev/null; then
    EXAMPLE_DIR="build/examples/Debug"
fi

usage() { printf 'usage: %s [capture|check]\n' "$0"; exit 2; }

# Refuse to gate a build that predates the code. The newest engine or example
# source against the newest binary: anything older cannot contain the change
# being gated.
assert_fresh() {
    local newest_src newest_exe
    newest_src="$(ls -t include/*.hpp src/*.cpp examples/*.cpp 2>/dev/null | head -1)"
    newest_exe="$(ls -t "$EXAMPLE_DIR"/*.exe 2>/dev/null | head -1)"
    [ -n "$newest_src" ] && [ -n "$newest_exe" ] || return 0
    if [ "$newest_src" -nt "$newest_exe" ]; then
        printf 'STALE BUILD: %s is newer than %s\n' "$newest_src" "$newest_exe" >&2
        printf 'Rebuild before gating:  cmake --build build\n' >&2
        exit 1
    fi
}

examples() {
    if [ ! -d "$EXAMPLE_DIR" ]; then
        printf 'No examples at %s -- build first:\n' "$EXAMPLE_DIR" >&2
        printf '  cmake --build build\n' >&2
        exit 1
    fi
    ls "$EXAMPLE_DIR"/*.exe 2>/dev/null
}

# Examples are seeded and deterministic, but a few write trace files into the
# working directory. Run each in its own scratch directory so one example's
# leftovers cannot change another's output.
run_one() {
    local exe="$1" out="$2" work
    work="$(mktemp -d)"
    ( cd "$work" && "$ROOT/$exe" ) > "$out" 2>&1
    rm -rf "$work"
}

is_excluded() {
    case " $EXCLUDED " in *" $1 "*) return 0 ;; esac
    return 1
}

capture() {
    assert_fresh
    mkdir -p "$BASELINE_DIR"
    local n=0 skipped=0
    for exe in $(examples); do
        local name; name="$(basename "$exe" .exe)"
        if is_excluded "$name"; then
            printf 'EXCLUDED (not reproducible run-to-run): %s\n' "$name"
            skipped=$((skipped + 1))
            continue
        fi
        run_one "$exe" "$BASELINE_DIR/$name.txt"
        n=$((n + 1))
    done
    printf 'captured %d baselines into %s (%d excluded)\n' "$n" "$BASELINE_DIR" "$skipped"
}

check() {
    assert_fresh
    if [ ! -d "$BASELINE_DIR" ]; then
        printf 'No baselines. Run: bash tools/baseline.sh capture\n' >&2
        exit 1
    fi
    local failed=0 checked=0 missing=0 skipped=0
    local tmp; tmp="$(mktemp)"
    for exe in $(examples); do
        local name; name="$(basename "$exe" .exe)"
        if is_excluded "$name"; then
            # Printed EVERY run, never silent. See the note beside EXCLUDED.
            printf 'EXCLUDED (v9 bug: not reproducible run-to-run): %s\n' "$name"
            skipped=$((skipped + 1))
            continue
        fi
        local want="$BASELINE_DIR/$name.txt"
        if [ ! -f "$want" ]; then
            printf 'NO BASELINE: %s (new example -- capture it deliberately)\n' "$name"
            missing=$((missing + 1))
            continue
        fi
        run_one "$exe" "$tmp"
        if ! diff -q "$want" "$tmp" > /dev/null 2>&1; then
            printf 'DIFFERS: %s\n' "$name"
            diff "$want" "$tmp" | head -12
            failed=$((failed + 1))
        fi
        checked=$((checked + 1))
    done
    rm -f "$tmp"

    printf '\n%d examples checked, %d differ, %d without a baseline, %d excluded\n' \
           "$checked" "$failed" "$missing" "$skipped"
    # A gate that measured nothing must never print CLEAN. The first
    # version of assert_fresh proved the point by accident: it called
    # exit inside $(examples), which killed only the subshell, so the
    # loop below ran over an empty list and this reported CLEAN over
    # zero examples.
    if [ "$checked" -eq 0 ]; then
        printf 'BASELINE FAILED: nothing was checked\n'
        return 1
    fi
    [ "$failed" -eq 0 ] || { printf 'BASELINE FAILED\n'; return 1; }
    printf 'BASELINE CLEAN\n'
}

case "${1:-}" in
    capture) capture ;;
    check)   check ;;
    *)       usage ;;
esac
