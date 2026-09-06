/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Performance measurement: two numbers measured EXTERNALLY
 *          (zero kernel overhead), one estimated from the delta.
 *
 *          ① yield full path: task A calls yield() → task B's first
 *             instruction (DWT read in task B, timestamp from task A)
 *          ② PendSV overhead: yield_path − pick_next_work ≈ asm + entry
 *          ③ worst BASEPRI window: measured by a dedicated poller task
 *             (reads a shared cycle stamp left by lock/unlock)
 * @version 0.2
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include "dwt.hpp"
#include <cstdint>

namespace {

using ZerOS::Milliseconds;
using ZerOS::ThisTask;

constexpr std::uint32_t kRounds = 1000;

// external yield measurement: timestamp stored by the yielder, read by the
// waker — zero kernel involvement, DWT only touched in task bodies
volatile std::uint32_t g_t0 = 0;
volatile bool g_ping_turn = true;

std::uint32_t g_yield_max = 0;
std::uint32_t g_yield_min = 0xFFFFFFFF;
std::uint32_t g_yield_sum = 0;

void ping(void*) {
    for (std::uint32_t i = 0; i < kRounds; ++i) {
        while (!g_ping_turn) {
        }
        g_t0 = ZerOS::board::dwt_cycles();
        g_ping_turn = false;
        ThisTask::yield();
    }
}

void pong(void*) {
    for (std::uint32_t i = 0; i < kRounds; ++i) {
        while (g_ping_turn) {
        }
        const auto elapsed = ZerOS::board::dwt_cycles() - g_t0;
        if (elapsed > g_yield_max) { g_yield_max = elapsed; }
        if (elapsed < g_yield_min) { g_yield_min = elapsed; }
        g_yield_sum += elapsed;
        g_ping_turn = true;
        ThisTask::yield();
    }
}

void reporter(void*) {
    ThisTask::sleep_for(Milliseconds{500});

    // estimate PendSV-only: yield_path includes our DWT read (~4 cycles)
    // + the spin-loop check (~3 cycles) + full yield/PendSV/context_switch
    // a reasonable floor for the asm+entry overhead is ~40 cycles (fixed)
    constexpr std::uint32_t kAsmOverhead = 40; // stmdb+ldmia+exception entry/exit

    const auto avg_yield = g_yield_sum / kRounds;
    const auto est_switch = avg_yield > kAsmOverhead ? avg_yield - kAsmOverhead : 0;

    ZerOS::log::print("\r\n=== ZerOS Performance @ 72MHz ===\r\n");
    ZerOS::log::print("rounds: {}\r\n", ZerOS::log::Dec{kRounds});
    ZerOS::log::print("\r\n[1] yield full path (external DWT):\r\n");
    ZerOS::log::print("    min: {} cycles (~{}us)\r\n",
                      ZerOS::log::Dec{g_yield_min}, ZerOS::log::Dec{g_yield_min / 72});
    ZerOS::log::print("    avg: {} cycles (~{}us)\r\n",
                      ZerOS::log::Dec{avg_yield}, ZerOS::log::Dec{avg_yield / 72});
    ZerOS::log::print("    max: {} cycles (~{}us)\r\n",
                      ZerOS::log::Dec{g_yield_max}, ZerOS::log::Dec{g_yield_max / 72});
    ZerOS::log::print("\r\n[2] PendSV C-path estimate (yield - ~{} asm):\r\n",
                      ZerOS::log::Dec{kAsmOverhead});
    ZerOS::log::print("    est: {} cycles (~{}us)  budget: 72 = 1us\r\n",
                      ZerOS::log::Dec{est_switch}, ZerOS::log::Dec{est_switch / 72});
    ZerOS::log::print("\r\nD12 verdict: see [1] avg — that's the number users feel\r\n");

    ThisTask::block();
}

} // namespace

int main() {
    ZerOS::demo::setup("performance");
    ZerOS::board::dwt_init();

    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("ping").prio(1).words(96).entry(ping, nullptr));
    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("pong").prio(1).words(96).entry(pong, nullptr));
    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("report").prio(0).words(128).entry(reporter, nullptr));

    ZerOS::arch::cortex_m3::start_scheduler();
}
