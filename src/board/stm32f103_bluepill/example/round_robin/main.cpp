/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Two NEVER-SLEEPING spinners on the SAME level for ~3 seconds.
 *          Without time slicing one would starve the other; the slice
 *          forces them to take turns — the interleaved output IS the
 *          feature, ten milliseconds apart.
 * @version 0.1
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
#include <cstdint>

namespace {

using ZerOS::ThisTask;

void spinner(void* tag) {
    const auto* line = reinterpret_cast<const char*>(tag);
    const auto deadline = ThisTask::now().tick_ + 3000;
    auto next_report = ThisTask::now().tick_;
    for (;;) {
        const auto now = ThisTask::now().tick_;
        if (now >= deadline) {
            break; // three seconds of proof is enough
        }
        if (now >= next_report) {
            ZerOS::log::print(line);
            next_report += 500;
        }
        // pure spinning, zero sleeps — the scheduler does the fairness
    }
    ZerOS::log::print("spinner done\r\n");
    ThisTask::block(); // self-retire: sleep with nobody to wake me
}

} // namespace

int main() {
    ZerOS::demo::setup("round robin");

    ZerOS::arch::cortex_m3::launch(ZerOS::task::named("rr1").prio(3).words(64).entry(
        spinner, const_cast<char*>("rr1 alive\r\n")));
    ZerOS::arch::cortex_m3::launch(ZerOS::task::named("rr2").prio(3).words(64).entry(
        spinner, const_cast<char*>("rr2 alive\r\n")));

    ZerOS::arch::cortex_m3::start_scheduler();
}
