#include <cstdint>

#include "stm32f1xx.h"
#include "uart.hpp"

#include "ZerOS/arch/arm_cortex_m3/cm3_irq.hpp"
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/this_task.hpp"

namespace {

// task bodies talk to the RTOS face only; silicon names stay in main()
using ZerOS::Milliseconds;
using ZerOS::ThisTask;
using ZerOS::arch::cortex_m3::spawn;
using ZerOS::arch::cortex_m3::start_scheduler;
using ZerOS::arch::cortex_m3::system_sched;

alignas(8) constinit std::uint32_t stack_a[64] = {};
alignas(8) constinit std::uint32_t stack_b[64] = {};
alignas(8) constinit std::uint32_t stack_c[48] = {};
alignas(8) constinit std::uint32_t idle_stack[32] = {};

// zero storage, meaning comes only from the factory chain
constinit ZerOS::sched::TCBStorage tcb_a{};
constinit ZerOS::sched::TCBStorage tcb_b{};
constinit ZerOS::sched::TCBStorage tcb_c{};

// the one-byte mailbox: ISR writes, task C drains; BASEPRI on both hands
constinit ZerOS::arch::cortex_m3::CortexM3CriticalSection mailbox_lock{};
char g_rx_byte = 0;
bool g_rx_pending = false;
ZerOS::sched::TCB* g_task_c = nullptr; // set in main, read by the ISR

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
        ThisTask::block();
        char c;
        {
            ZerOS::irq::CriticalGuard guard{mailbox_lock};
            c = g_rx_byte;
            g_rx_pending = false;
        }
        ZerOS::board::print("got '");
        ZerOS::board::uart1_putc(c);
        ZerOS::board::print("'\r\n");
    }
}

} // namespace

// RXNE (and ORE, sharing RXNEIE): hand the byte to task C and book a
// switch — the actual context switch waits until the ISR exits (PendSV)
extern "C" void USART1_IRQHandler() {
    if ((USART1->SR & USART_SR_RXNE) == 0) {
        return;
    }
    const char c = ZerOS::board::uart1_getc();
    {
        ZerOS::irq::CriticalGuard guard{mailbox_lock};
        g_rx_byte = c;
        g_rx_pending = true;
    }
    if (g_task_c != nullptr) {
        system_sched.ready(g_task_c); // pends PendSV, never switches here
    }
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
    g_task_c = &c;

    ZerOS::arch::cortex_m3::init_time(SystemCoreClock / 1000);
    start_scheduler(idle_stack);
}
