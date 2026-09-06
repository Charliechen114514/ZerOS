#!/usr/bin/env bash
# ZerOS CI — runs all checks. Use before every commit:
#   ./scripts/ci.sh            everything
#   ./scripts/ci.sh smoke      just one: host | hygiene | size | smoke
# Any check failing turns the exit code non-zero. The GitHub workflow
# runs this same script on a clean checkout.

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

fail() { echo "FAILED: $1" >&2; exit 1; }
banner() { echo; echo "====[ $1 ]===="; }

# ---------------------------------------------------------------- host ----
check_host() {
    banner "1/4 host unit tests"
    # logs are printed unconditionally — success or failure, full output
    cmake -B "$HOST_DIR" -DZEROS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$HOST_DIR" -j"$NCPU" || fail "host build"
    ctest --test-dir "$HOST_DIR" --output-on-failure
}

# -------------------------------------------------------------- hygiene ----
check_hygiene() {
    banner "2/4 include hygiene"
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
check_size() {
    banner "3/4 cross build + size limits (flash ${FLASH_LIMIT}B / ram ${RAM_LIMIT}B)"
    # logs are printed unconditionally — a grep filter here once ate the
    # actual error lines and treated mere warnings as the failure itself
    cmake -B "$ARM_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
    cmake --build "$ARM_DIR" -j"$NCPU" || fail "cross build"

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
check_smoke() {
    banner "4/4 renode behavior check (each demo must SAY its line)"
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

    # pre-build all ELF targets (sequential, no lock contention)
    for demo in "${!MARK[@]}"; do
        cmake --build "$ARM_DIR" --target "zeros-demo-$demo" -j"$NCPU" \
            || fail "pre-build $demo: ELF not produced"
    done

    # simulate sequentially — a CI runner has 2-4 cores, 8 concurrent
    # Renode instances starve each other and virtual time barely advances;
    # 5s per demo × 8 = ~40s total, acceptable for CI
    local tmpdir root resc
    tmpdir=$(mktemp -d)
    root="$(pwd)"
    resc=src/board/stm32f103_bluepill/sim/renode/bluepill.resc
    for demo in "${!MARK[@]}"; do
        local elf="$ARM_DIR/src/board/stm32f103_bluepill/example/$demo/zeros-demo-$demo.elf"
        [ -f "$elf" ] || fail "ELF missing: $elf"
        echo "--- run-$demo (expect: ${MARK[$demo]})"
        (
            cd "$root" && renode --console --disable-xwt \
                -e "logFile @${tmpdir}/${demo}.log" \
                -e "\$bin=@${elf}" \
                -e "include @${resc}" \
                -e "start" -e "sleep 5" -e "quit"
        )
    done

    local failed=0
    for demo in "${!MARK[@]}"; do
        if ! grep -q "${MARK[$demo]}" "${tmpdir}/${demo}.log" 2>/dev/null; then
            echo >&2
            echo "========== $demo FAILED (expected: '${MARK[$demo]}') ==========" >&2
            echo "--- last 30 lines of renode log ---" >&2
            tail -30 "${tmpdir}/${demo}.log" 2>/dev/null >&2
            echo "=================================================" >&2
            failed=1
        fi
    done
    rm -rf "$tmpdir"
    [ "$failed" -eq 0 ] || fail "one or more demos missed their lines"
    echo "all ${#MARK[@]} demos spoke their lines"
}

case "${1:-all}" in
    host) check_host ;;
    hygiene) check_hygiene ;;
    size) check_size ;;
    smoke) check_smoke ;;
    all) check_host; check_hygiene; check_size; check_smoke
         echo; echo "ALL CHECKS PASSED" ;;
    *) echo "usage: $0 [host|hygiene|size|smoke|all]" >&2; exit 2 ;;
esac
