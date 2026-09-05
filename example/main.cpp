#include <cstdint>

#include "stm32f1xx.h"
#include "uart.hpp"

#include "ZerOS/arch/arm_cortex_m3/cm3_irq.hpp"
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/sync.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"

namespace {

// task bodies talk to the RTOS face only; silicon names stay in main()
using ZerOS::Milliseconds;
using ZerOS::ThisTask;
using ZerOS::arch::cortex_m3::spawn;
using ZerOS::arch::cortex_m3::start_scheduler;

alignas(8) constinit std::uint32_t stack_a[64] = {};
alignas(8) constinit std::uint32_t stack_b[64] = {};
alignas(8) constinit std::uint32_t stack_c[48] = {};
alignas(8) constinit std::uint32_t idle_stack[32] = {};

// zero storage, meaning comes only from the factory chain
constinit ZerOS::sched::TCBStorage tcb_a{};
constinit ZerOS::sched::TCBStorage tcb_b{};
constinit ZerOS::sched::TCBStorage tcb_c{};

// the event side lives in the semaphore (a unit arriving BEFORE the task
// sleeps survives — the hand-rolled ready() flag could not do that);
// the byte itself is only the payload, still a one-slot mailbox
constinit ZerOS::sync::Semaphore g_rx_sem{0};
constinit ZerOS::arch::cortex_m3::CortexM3CriticalSection mailbox_lock{};
char g_rx_byte = 0;

void task_a(void*) {
    for (;;) {
        ZerOS::board::print("A\r\n");
        ThisTask::sleep_for(Milliseconds{1000});
    }
}

void task_b(void*) {
    for (;;) {
        ZerOS::board::print("B\r\n");
        ThisTask::sleep_for(Milliseconds{700});
    }
}

// event-sleeper: woken by the RX interrupt, not by the clock
void task_c(void*) {
    for (;;) {
        g_rx_sem.acquire();
        char c;
        {
            ZerOS::irq::CriticalGuard guard{mailbox_lock};
            c = g_rx_byte;
        }
        ZerOS::board::print("got '");
        ZerOS::board::uart1_putc(c);
        ZerOS::board::print("'\r\n");
    }
}

} // namespace

// RXNE (and ORE, sharing RXNEIE): park the byte, hand the event over.
// release() is ISR-safe by construction (BASEPRI inside, switch pended,
// never switched here)
extern "C" void USART1_IRQHandler() {
    if ((USART1->SR & USART_SR_RXNE) == 0) {
        return;
    }
    const char c = ZerOS::board::uart1_getc();
    {
        ZerOS::irq::CriticalGuard guard{mailbox_lock};
        g_rx_byte = c;
    }
    g_rx_sem.release();
}

int main() {
    ZerOS::board::uart1_init();
    ZerOS::board::print("\r\nZerOS multi-task demo @ Blue Pill\r\n");

    auto& a =
        ZerOS::task::named("a").prio(1).stack(stack_a).entry(task_a, nullptr).spawn_into(tcb_a);
    auto& b =
        ZerOS::task::named("b").prio(1).stack(stack_b).entry(task_b, nullptr).spawn_into(tcb_b);
    auto& c =
        ZerOS::task::named("c").prio(0).stack(stack_c).entry(task_c, nullptr).spawn_into(tcb_c);

    spawn(a);
    spawn(b);
    spawn(c);

    ZerOS::arch::cortex_m3::init_time(SystemCoreClock / 1000);
    start_scheduler(idle_stack);
}
