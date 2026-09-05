#include <cstdint>
#include <cstddef>
#include <span>

#include "stm32f1xx.h"
#include "uart.hpp"

#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"

namespace {

using ZerOS::arch::cortex_m3::system_sched;
using ZerOS::arch::cortex_m3::system_time;

alignas(8) constinit std::uint32_t stack_a[64] = {};
alignas(8) constinit std::uint32_t stack_b[64] = {};
alignas(8) constinit std::uint32_t idle_stack[32] = {};

constinit ZerOS::sched::TCB tcb_a{};
constinit ZerOS::sched::TCB tcb_b{};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
ZerOS::sched::TCB* owner_of(ZerOS::base::BorrowedPtr<ZerOS::clock::TimeWaiter> w) {
    return reinterpret_cast<ZerOS::sched::TCB*>(
        reinterpret_cast<std::byte*>(w.get()) - offsetof(ZerOS::sched::TCB, action));
}
#pragma GCC diagnostic pop

void wake(ZerOS::base::BorrowedPtr<ZerOS::clock::TimeWaiter> w) {
    system_sched.ready(owner_of(w));
}

void task_sleep(std::uint32_t ms) {
    auto cur = system_sched.current_task();
    cur->action.OnTime = wake;
    system_time.call_after_span(&cur->action, ms);
    system_sched.block(cur);
}

void task_a(void*) {
    for (;;) {
        ZerOS::board::print("A\r\n");
        task_sleep(1000);
    }
}

void task_b(void*) {
    for (;;) {
        ZerOS::board::print("B\r\n");
        task_sleep(700);
    }
}

} // namespace

int main() {
    ZerOS::board::uart1_init();
    ZerOS::board::print("\r\nZerOS multi-task demo @ Blue Pill\r\n");

    tcb_a.task_priority_ = 1;
    tcb_a.name_ = "a";
    tcb_a.task_wrapper_ = {task_a, nullptr};
    tcb_a.stack_view_ = stack_a;

    tcb_b.task_priority_ = 1;
    tcb_b.name_ = "b";
    tcb_b.task_wrapper_ = {task_b, nullptr};
    tcb_b.stack_view_ = stack_b;

    ZerOS::arch::cortex_m3::spawn(tcb_a);
    ZerOS::arch::cortex_m3::spawn(tcb_b);

    ZerOS::arch::cortex_m3::init_time(SystemCoreClock / 1000);
    ZerOS::arch::cortex_m3::start_scheduler(idle_stack);
}
