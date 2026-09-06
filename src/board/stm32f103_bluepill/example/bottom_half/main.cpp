/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   The ISR bottom half: a timer (standing in for an interrupt)
 *          defers a heavy job every second. The worker NAPS inside the
 *          job — something an interrupt could never dare — then finishes.
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/sync.hpp"
#include "ZerOS/kernel/clock/timer.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include <cstdint>

namespace {

using ZerOS::Milliseconds;
using ZerOS::ThisTask;

constinit ZerOS::sync::Worker g_worker{};
constinit ZerOS::clock::Timer g_interrupt{};

std::uint32_t g_job_count = 0;

void heavy_job(void*) { // runs in the WORKER context: sleeping is legal here
    ++g_job_count;
    ZerOS::log::print("worker: job #{} in, napping 200ms\r\n", ZerOS::log::Dec{g_job_count});
    ThisTask::sleep_for(Milliseconds{200}); // the interrupt could never dare
    ZerOS::log::print("worker: job #{} done\r\n", ZerOS::log::Dec{g_job_count});
}

void drop_job(void*) { // tick context: defer, never do heavy work here
    static_cast<void>(g_worker.defer(heavy_job, nullptr));
}

} // namespace

int main() {
    ZerOS::demo::setup("bottom half");

    ZerOS::arch::cortex_m3::spawn(g_worker.setup(2, "worker"));
    g_interrupt.periodic(Milliseconds{1000}, drop_job, nullptr);

    ZerOS::arch::cortex_m3::start_scheduler();
}
