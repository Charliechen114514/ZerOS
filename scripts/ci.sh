#!/usr/bin/env bash
# ZerOS CI — the four gates. Run this before every commit:
#   ./scripts/ci.sh            all gates
#   ./scripts/ci.sh host       one gate by name: host | hygiene | size | smoke
# Any gate failing turns the exit code non-zero. CI is just this script
# run on a clean checkout; locally it guards the working tree.

set -euo pipefail
cd "$(dirname "$0")/.."

TOOLCHAIN=cmake/arch/arm-none-eabi.cmake
HOST_DIR=build-host
ARM_DIR=build
NCPU=$(nproc)

# regression ceilings (bytes). Not the D12 kernel budget — that needs a
# symbol-level split — these catch WHOLE-IMAGE regressions per demo:
# baseline 2026-09-06 max was ~5.3K flash / ~2.6K ram; headroom ≈ 2x
FLASH_LIMIT=12288
RAM_LIMIT=6144

fail() { echo "GATE FAILED: $1" >&2; exit 1; }
gate() { echo; echo "====[ $1 ]===="; }

# ---------------------------------------------------------------- host ----
gate_host() {
    gate "1/4 host tests"
    cmake -B "$HOST_DIR" -DZEROS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build "$HOST_DIR" -j"$NCPU" 2>&1 | grep -E 'error|warning' && fail "host build" || true
    ctest --test-dir "$HOST_DIR" --output-on-failure
}

# -------------------------------------------------------------- hygiene ----
gate_hygiene() {
    gate "2/4 include hygiene"
    # kernel headers must not know boards or vendors
    if grep -rnE '^\s*#\s*include.*(src/board|third_party|stm32)' include/ZerOS/kernel/; then
        fail "kernel purity: board/vendor include found in include/ZerOS/kernel/"
    fi
    # kernel unit tests must not bind to any architecture
    if grep -rn 'ZerOS/arch/' test/; then
        fail "test purity: kernel tests must stay architecture-free"
    fi
    # the include/ZerOS/ root stays empty — everything files into a subsystem
    if ls include/ZerOS/*.hpp >/dev/null 2>&1; then
        fail "layout: headers found directly in include/ZerOS/ root"
    fi
    echo "clean: kernel purity / test purity / layout"
}

# ----------------------------------------------------------------- size ----
gate_size() {
    gate "3/4 cross build + size gates (flash ${FLASH_LIMIT}B / ram ${RAM_LIMIT}B)"
    cmake -B "$ARM_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" >/dev/null
    cmake --build "$ARM_DIR" -j"$NCPU" 2>&1 | grep -E 'error|warning' && fail "cross build" || true

    shopt -s nullglob
    elves=(build/src/board/stm32f103_bluepill/example/*/zeros-demo-*.elf)
    [ ${#elves[@]} -gt 0 ] || fail "no demo ELF produced"
    for elf in "${elves[@]}"; do
        read -r text data bss < <(arm-none-eabi-size "$elf" | awk 'NR==2{print $1, $2, $3}')
        flash=$((text + data)); ram=$((data + bss))
        name=$(basename "$elf" .elf)
        printf '%-28s flash=%-6d ram=%-6d\n' "$name" "$flash" "$ram"
        [ "$flash" -le "$FLASH_LIMIT" ] || fail "$name flash $flash > $FLASH_LIMIT"
        [ "$ram" -le "$RAM_LIMIT" ] || fail "$name ram $ram > $RAM_LIMIT"
    done
}

# ---------------------------------------------------------------- smoke ----
gate_smoke() {
    gate "4/4 renode behavior smoke (each demo must SAY its line)"
    # the one string each demo owes us when it works; testaments runs calm
    # (its 'x'/'y' death scripts stay manual, see docs/simulation.md)
    declare -A MARK=(
        [heartbeat]="Hello! ZerOS"
        [event_stream]="got 'a'"
        [inversion]="inheritance paid off"
        [bottom_half]="job #1 done"
        [timers]="one-shot: fired"
        [events_notify]="both keys in"
        [testaments]="alive"
        [round_robin]="rr2 alive"
    )
    for demo in "${!MARK[@]}"; do
        echo "--- run-$demo (expect: ${MARK[$demo]})"
        out=$(cmake --build "$ARM_DIR" --target "run-$demo" -j"$NCPU" 2>&1 | sed 's/\x1b\[[0-9;]*m//g')
        echo "$out" | grep -q "${MARK[$demo]}" || { echo "$out" | tail -5; fail "run-$demo missed: ${MARK[$demo]}"; }
    done
    echo "all ${#MARK[@]} demos spoke their lines"
}

case "${1:-all}" in
    host) gate_host ;;
    hygiene) gate_hygiene ;;
    size) gate_size ;;
    smoke) gate_smoke ;;
    all) gate_host; gate_hygiene; gate_size; gate_smoke
         echo; echo "ALL GATES GREEN" ;;
    *) echo "usage: $0 [host|hygiene|size|smoke|all]" >&2; exit 2 ;;
esac
