/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Both ways to die, both leaving a testament. Run plain: a calm
 *          heartbeat. Then inject bytes and pick your death —
 *            'x' (0x78): undefined instruction → full HARD FAULT report
 *            'y' (0x79): smashed canary → STACK OVERFLOW report
 *          (inject from the Renode monitor: `usart1 WriteChar 0x78`)
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

#include "stm32f1xx.h"

namespace {

using ZerOS::Milliseconds;
using ZerOS::ThisTask;

constinit ZerOS::sync::Queue<char, 8> g_terminal{};

// 'y' smashes the runner's own canary — an honest simulation of what a
// real overflow leaves behind; the PendSV patrol reports and halts
void smash_canary() {
    auto cur = ZerOS::system::os().current_task();
    auto& view = ZerOS::sched::zeros_impl::TCBKeys::stack_view(*cur);
    view[7] = 0; // the deepest sentinel dies first in a real overflow
    view[6] = 0;
}

void heartbeat(void*) {
    for (;;) {
        ZerOS::log::print("alive\r\n");
        ThisTask::sleep_for(Milliseconds{500});
    }
}

// the terminal: reads bytes and dies on command
void terminal(void*) {
    for (;;) {
        auto got = g_terminal.receive_one(); // wait forever
        if (!got.has_value()) {
            continue;
        }
        const char c = *got;
        if (c == 'x') {
            ZerOS::log::print("term: executing an undefined instruction, goodbye\r\n");
            asm volatile("udf #0"); // CPU-level fault: no simulator leniency
        } else if (c == 'y') {
            ZerOS::log::print("term: smashing my canary\r\n");
            smash_canary();
            ThisTask::sleep_for(Milliseconds{2}); // next switch patrols
        } else {
            ZerOS::log::print("term: '{}'\r\n", c);
        }
    }
}

} // namespace

extern "C" void USART1_IRQHandler() { // strong: bytes go to the terminal
    if ((USART1->SR & USART_SR_RXNE) != 0) {
        static_cast<void>(g_terminal.post(ZerOS::board::uart1_getc()));
    }
}

int main() {
    ZerOS::demo::setup("testaments (inject 'x' or 'y')");

    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("beat").prio(2).words(64).entry(heartbeat, nullptr));
    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("term").prio(0).words(96).entry(terminal, nullptr));

    ZerOS::arch::cortex_m3::start_scheduler();
}
