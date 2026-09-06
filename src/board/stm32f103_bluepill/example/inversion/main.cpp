/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   The priority-inversion theater: L (low) owns the lock and
 *          busy-holds it; M (middle) hogs the CPU doing unrelated work;
 *          H (high) comes for the lock 100ms late. WITHOUT inheritance M
 *          would starve L and H would wait forever; WITH it, L runs
 *          wearing H's level — watch M go silent for the whole busy window.
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/sync.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include <cstdint>

namespace {

using ZerOS::Milliseconds;
using ZerOS::ThisTask;

constinit ZerOS::sync::Mutex g_theater{};

void task_holder(void*) { // L: low, owns the lock, busy-holds 300ms
    for (;;) {
        ThisTask::sleep_for(Milliseconds{2000});
        if (g_theater.lock() != ZerOS::sync::MutexError::Ok) {
            continue;
        }
        ZerOS::log::print("L: locked, busy 300ms\r\n");
        const auto until = ThisTask::now().tick_ + 300;
        while (ThisTask::now().tick_ < until) { // busy hold: the whole point
        }
        ZerOS::log::print("L: unlocked\r\n");
        g_theater.unlock();
    }
}

void task_urgent(void*) { // H: high, arrives 100ms after L takes the lock
    for (;;) {
        ThisTask::sleep_for(Milliseconds{2100});
        ZerOS::log::print("H: wants the lock\r\n");
        if (g_theater.lock() == ZerOS::sync::MutexError::Ok) {
            ZerOS::log::print("H: LOCKED (inheritance paid off)\r\n");
            g_theater.unlock();
        }
    }
}

void task_bully(void*) { // M: middle, never touches the lock, just hogs
    for (;;) {
        ZerOS::log::print("M: running\r\n");
        ThisTask::sleep_for(Milliseconds{100});
    }
}

} // namespace

int main() {
    ZerOS::demo::setup("priority inheritance");

    auto spawn = ZerOS::arch::cortex_m3::launch;
    spawn(ZerOS::task::named("L").prio(2).words(96).entry(task_holder, nullptr));
    spawn(ZerOS::task::named("H").prio(0).words(64).entry(task_urgent, nullptr));
    spawn(ZerOS::task::named("M").prio(1).words(64).entry(task_bully, nullptr));

    ZerOS::arch::cortex_m3::start_scheduler();
}
