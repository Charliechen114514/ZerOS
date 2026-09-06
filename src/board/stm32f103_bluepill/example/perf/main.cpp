/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Performance measurement. Two independent rulers over the SAME
 *          switch event (the one that wakes pong):
 *
 *          ① yield full path, external DWT (zero kernel overhead):
 *             ping stamps CYCCNT before yielding; pong reads it on wake.
 *          ② PendSV handler only, in-handler probe (build gate
 *             ZEROS_MEASURE_PENDSV): branch-free DWT snapshots inside the
 *             naked asm; reported raw and net of the ~24-cycle probe cost.
 *
 *          So ① − ② = API + queue + pend + exception entry/exit + the
 *          waker's first instructions — where every cycle of [1] goes.
 *
 *          ZEROS_PERF_NO_TICK (CMake option): no SysTick at all. The
 *          reporter is woken by notify, nothing depends on time, so the
 *          comparison with/without tick isolates the max-sample story.
 * @version 0.3
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/system.hpp"
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

// probe cost, see switch.cpp: two branch-free DWT snapshots ride the
// measured path; subtract to get the handler's own number
constexpr std::uint32_t kProbeOverhead = 24;
// a sample this far above the running average counts as a stray (tick
// interference, or whatever else elbowed in) — the count tells whether
// max is an outlier or a habit
constexpr std::uint32_t kOutlierSlack = 128;
// 10 minutes — far past this demo's lifetime. NOT the full 0xFFFFFFFF:
// the kernel arms a deadline as now + span, and that addition wraps
// around to "strictly in the past", firing the timeout at once (caught
// live on silicon + Renode). In ZEROS_PERF_NO_TICK builds the clock never
// advances anyway — the notify from pong is the only way out
constexpr Milliseconds kWaitForPong{600'000};

// external yield measurement: timestamp stored by the yielder, read by
// the waker — zero kernel involvement, DWT only touched in task bodies
volatile std::uint32_t g_t0 = 0;
volatile bool g_ping_turn = true;

ZerOS::sched::TCB* g_reporter = nullptr;

std::uint32_t g_yield_max = 0;
std::uint32_t g_yield_min = 0xFFFFFFFF;
std::uint32_t g_yield_sum = 0;
std::uint32_t g_outliers = 0;

std::uint32_t g_pendsv_max = 0;
std::uint32_t g_pendsv_min = 0xFFFFFFFF;
std::uint32_t g_pendsv_sum = 0;

void tally(std::uint32_t& mn, std::uint32_t& mx, std::uint32_t& sum,
           std::uint32_t v) {
    if (v < mn) { mn = v; }
    if (v > mx) { mx = v; }
    sum += v;
}

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
        // the switch that woke us is the one both rulers measure
        const auto elapsed = ZerOS::board::dwt_cycles() - g_t0;
        tally(g_yield_min, g_yield_max, g_yield_sum, elapsed);
        // this division runs after the sample was taken — it lives in
        // pong's own execution slice, never inside a measured window
        if (elapsed > g_yield_sum / (i + 1) + kOutlierSlack) { ++g_outliers; }

#ifdef ZEROS_MEASURE_PENDSV
        // same switch, second ruler: handler enter→exit stamps (the symbols
        // only exist with scripts/patches/pendsv-probe.patch applied)
        const auto handler = zeros_pendsv_exit - zeros_pendsv_enter;
        tally(g_pendsv_min, g_pendsv_max, g_pendsv_sum, handler);
#endif

        g_ping_turn = true;
        ThisTask::yield();
    }
    ZerOS::system::os().notify(g_reporter, 1); // last round done — wake the scribe
}

std::uint32_t sat_sub(std::uint32_t v, std::uint32_t by) {
    return v > by ? v - by : 0;
}

void reporter(void*) {
    if (!ThisTask::wait_notify(kWaitForPong).has_value()) {
        // ~49.7 days without a letter: nothing sensible left to say
        ZerOS::log::print("perf: reporter timed out\r\n");
        ThisTask::block();
    }

    const auto avg_yield = g_yield_sum / kRounds;
    ZerOS::log::print("\r\n=== ZerOS Performance @ 72MHz ===\r\n");
    ZerOS::log::print("rounds: {}\r\n", ZerOS::log::Dec{kRounds});
#ifdef ZEROS_PERF_NO_TICK
    ZerOS::log::print("tick: OFF (no SysTick)\r\n");
#endif
    ZerOS::log::print("\r\n[1] yield full path (external DWT):\r\n");
    ZerOS::log::print("    min: {} cycles (~{}us)\r\n",
                      ZerOS::log::Dec{g_yield_min}, ZerOS::log::Dec{g_yield_min / 72});
    ZerOS::log::print("    avg: {} cycles (~{}us)\r\n",
                      ZerOS::log::Dec{avg_yield}, ZerOS::log::Dec{avg_yield / 72});
    ZerOS::log::print("    max: {} cycles (~{}us)\r\n",
                      ZerOS::log::Dec{g_yield_max}, ZerOS::log::Dec{g_yield_max / 72});
    ZerOS::log::print("    outliers(>avg+{}): {} of {}\r\n",
                      ZerOS::log::Dec{kOutlierSlack}, ZerOS::log::Dec{g_outliers},
                      ZerOS::log::Dec{kRounds});

    if (g_pendsv_max > 0) { // the probe only stamps when built with it
        const auto avg_raw = g_pendsv_sum / kRounds;
        const auto avg_net = sat_sub(avg_raw, kProbeOverhead);
        ZerOS::log::print("\r\n[2] PendSV handler (probe, {} cycles subtracted):\r\n",
                          ZerOS::log::Dec{kProbeOverhead});
        ZerOS::log::print("    net min: {} cycles (~{}us)\r\n",
                          ZerOS::log::Dec{sat_sub(g_pendsv_min, kProbeOverhead)},
                          ZerOS::log::Dec{sat_sub(g_pendsv_min, kProbeOverhead) / 72});
        ZerOS::log::print("    net avg: {} cycles (~{}us)  budget: 72 = 1us\r\n",
                          ZerOS::log::Dec{avg_net}, ZerOS::log::Dec{avg_net / 72});
        ZerOS::log::print("    net max: {} cycles (~{}us)\r\n",
                          ZerOS::log::Dec{sat_sub(g_pendsv_max, kProbeOverhead)},
                          ZerOS::log::Dec{sat_sub(g_pendsv_max, kProbeOverhead) / 72});
        ZerOS::log::print("    raw avg: {} cycles\r\n", ZerOS::log::Dec{avg_raw});
        ZerOS::log::print("\r\n[3] accounting: yield[1] - handler[2] = {} cycles\r\n",
                          ZerOS::log::Dec{sat_sub(avg_yield, avg_net)});
        ZerOS::log::print("    (API + queue + pend + exception entry/exit + waker prologue)\r\n");
    } else {
#ifdef ZEROS_MEASURE_PENDSV
        // built but every stamp reads zero — DWT not modeled (simulator?)
        ZerOS::log::print("\r\n[2] PendSV handler: probe built, CYCCNT reads zero\r\n");
#else
        ZerOS::log::print("\r\n[2] PendSV handler: probe not built (ZEROS_MEASURE_PENDSV off)\r\n");
#endif
    }

    ZerOS::log::print("\r\nD12 verdict: see [1] avg — that's the number users feel\r\n");

    ThisTask::block();
}

} // namespace

int main() {
#ifdef ZEROS_PERF_NO_TICK
    ZerOS::demo::setup("performance (no tick)", false);
#else
    ZerOS::demo::setup("performance");
#endif
    ZerOS::board::dwt_init();

    g_reporter = ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("report").prio(0).words(160).entry(reporter, nullptr));
    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("ping").prio(1).words(96).entry(ping, nullptr));
    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("pong").prio(1).words(96).entry(pong, nullptr));

    ZerOS::arch::cortex_m3::start_scheduler();
}
