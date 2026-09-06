/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Dynamic tick, witnessed: a task naps 500ms four times and
 *          stamps each wake with the ledger's own now(); then the tick
 *          bookkeeping tells the story — with ZEROS_TICKLESS the idle
 *          gaps fire one-shots instead of hundreds of periodic ticks,
 *          while the nap rhythm must not move by a single millisecond
 *          (可早不可晚, but the ledger keeps the beat honest).
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include <cstdint>

namespace {

using ZerOS::Milliseconds;
using ZerOS::ThisTask;
using ZerOS::log::Dec;

constexpr std::uint32_t kNaps = 4;

void napper(void*) {
    for (std::uint32_t i = 1; i <= kNaps; ++i) {
        ThisTask::sleep_for(Milliseconds{500});
        const auto now = ThisTask::now();
        ZerOS::log::print("nap {} woke @ {}ms\r\n", Dec{i}, Dec{now.tick_});
    }
    ZerOS::log::print("tickless: witnessed\r\n");
#ifdef ZEROS_TICKLESS
    ZerOS::log::print("mode: tickless idle (one-shot to next deadline)\r\n");
#else
    ZerOS::log::print("mode: periodic 1kHz (ZEROS_TICKLESS off)\r\n");
#endif
    ZerOS::log::print("periodic ticks: {}\r\n", Dec{zeros_tick_periodic});
    ZerOS::log::print("one-shot fires: {}\r\n", Dec{zeros_tick_oneshot});
    ThisTask::block();
}

} // namespace

int main() {
    ZerOS::demo::setup("tickless");

    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("napper").prio(1).words(160).entry(napper, nullptr));

    ZerOS::arch::cortex_m3::start_scheduler();
}
