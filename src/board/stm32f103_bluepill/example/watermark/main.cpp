/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Stack high-water mark: one task carries real ballast (a
 *          volatile local array welded to its frame), one barely
 *          breathes; after a second of life the reporter reads both
 *          watermarks — capacity vs least-free-ever, in words, canary's
 *          8 excluded. The number to size stacks by (D30/D33 paid for
 *          this lesson three times).
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/kernel/sched/stack_guard.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include <cstdint>

namespace {

using ZerOS::Milliseconds;
using ZerOS::ThisTask;
using ZerOS::log::Dec;
using ZerOS::sched::TCB;
using ZerOS::task::TaskGuardHelper;

TCB* g_deep = nullptr;
TCB* g_shallow = nullptr;

void deep(void*) {
    // ballast: a volatile local array welded onto the frame — written
    // and read every round so nothing can optimize it away; the stack
    // genuinely gets pushed down to this depth (and further, every time
    // the sleep call chain runs through it)
    volatile std::uint32_t ballast[32];
    for (std::uint32_t round = 0;; ++round) {
        for (std::uint32_t i = 0; i < 32; ++i) {
            ballast[i] = round + i;
        }
        std::uint32_t sum = 0;
        for (std::uint32_t i = 0; i < 32; ++i) {
            sum += ballast[i];
        }
        (void)sum;
        ThisTask::sleep_for(Milliseconds{100});
    }
}

void shallow(void*) {
    for (;;) {
        ThisTask::sleep_for(Milliseconds{100});
    }
}

void report_one(const char* name, TCB* t) {
    const auto cap = TaskGuardHelper::capacity_words(*t);
    const auto free = TaskGuardHelper::watermark_free_words(*t);
    // (the mini formatter speaks plain {} only — no alignment dialect)
    ZerOS::log::print("  {}: {} cap, {} free, {} used at deepest\r\n",
                      name, Dec{cap}, Dec{free}, Dec{cap - free});
}

void reporter(void*) {
    ThisTask::sleep_for(Milliseconds{1000});

    ZerOS::log::print("watermark: read\r\n");
    ZerOS::log::print("  (words, canary's 8 excluded)\r\n");
    report_one("deep", g_deep);     // parked asleep — single core says
    report_one("shallow", g_shallow); // their stacks hold still while we read

    ThisTask::block();
}

} // namespace

int main() {
    ZerOS::demo::setup("stack watermark");

    g_deep = ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("deep").prio(1).words(96).entry(deep, nullptr));
    g_shallow = ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("shallow").prio(1).words(64).entry(shallow, nullptr));
    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("report").prio(0).words(192).entry(reporter, nullptr));

    ZerOS::arch::cortex_m3::start_scheduler();
}
