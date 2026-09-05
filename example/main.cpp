#include <cstdint>

#include "stm32f1xx.h"
#include "uart.hpp"

#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"

namespace {

using ZerOS::arch::cortex_m3::sleep_for;
using ZerOS::arch::cortex_m3::spawn;
using ZerOS::arch::cortex_m3::start_scheduler;

alignas(8) constinit std::uint32_t stack_a[64] = {};
alignas(8) constinit std::uint32_t stack_b[64] = {};
alignas(8) constinit std::uint32_t idle_stack[32] = {};

// zero storage, meaning comes only from the factory chain
constinit ZerOS::sched::TCBStorage tcb_a{};
constinit ZerOS::sched::TCBStorage tcb_b{};

void task_a(void*) {
    for (;;) {
        ZerOS::board::print("A\r\n");
        sleep_for(1000);
    }
}

void task_b(void*) {
    for (;;) {
        ZerOS::board::print("B\r\n");
        sleep_for(700);
    }
}

} // namespace

int main() {
    ZerOS::board::uart1_init();
    ZerOS::board::print("\r\nZerOS multi-task demo @ Blue Pill\r\n");

    auto& a =
        ZerOS::task::named("a").prio(1).stack(stack_a).entry(task_a, nullptr).spawn_into(tcb_a);
    auto& b =
        ZerOS::task::named("b").prio(1).stack(stack_b).entry(task_b, nullptr).spawn_into(tcb_b);

    spawn(a);
    spawn(b);

    ZerOS::arch::cortex_m3::init_time(SystemCoreClock / 1000);
    start_scheduler(idle_stack);
}
