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
alignas(8) constinit std::uint32_t stack_holder[96] = {};
alignas(8) constinit std::uint32_t stack_urgent[64] = {};
alignas(8) constinit std::uint32_t stack_bully[64] = {};
alignas(8) constinit std::uint32_t idle_stack[32] = {};

// zero storage, meaning comes only from the factory chain
constinit ZerOS::sched::TCBStorage tcb_a{};
constinit ZerOS::sched::TCBStorage tcb_b{};
constinit ZerOS::sched::TCBStorage tcb_c{};
constinit ZerOS::sched::TCBStorage tcb_holder{};
constinit ZerOS::sched::TCBStorage tcb_urgent{};
constinit ZerOS::sched::TCBStorage tcb_bully{};

// the event side lives in the semaphore (a unit arriving BEFORE the task
// sleeps survives — the hand-rolled ready() flag could not do that);
// the byte itself is only the payload, still a one-slot mailbox
constinit ZerOS::sync::Semaphore g_rx_sem{0};
constinit ZerOS::arch::cortex_m3::CortexM3CriticalSection mailbox_lock{};
char g_rx_byte = 0;

// ---- the priority-inversion theater ----
// L (prio 2) owns the lock and busy-holds it; M (prio 1) hogs the CPU doing
// unrelated work; H (prio 0) comes for the lock 100ms late. WITHOUT
// inheritance M would starve L and H would wait forever-ish; WITH it, L runs
// wearing H's level — watch M go silent for the whole busy window.
constinit ZerOS::sync::Mutex g_theater{};

void task_holder(void*) { // L
    for (;;) {
        ThisTask::sleep_for(Milliseconds{2000});
        if (g_theater.lock() != ZerOS::sync::MutexError::Ok) {
            continue;
        }
        ZerOS::board::print("L: locked, busy 300ms\r\n");
        const auto until = ThisTask::now().tick_ + 300;
        while (ThisTask::now().tick_ < until) { // busy hold: the whole point
        }
        ZerOS::board::print("L: unlocked\r\n");
        g_theater.unlock();
    }
}

void task_urgent(void*) { // H — always arrives 100ms after L takes the lock
    for (;;) {
        ThisTask::sleep_for(Milliseconds{2100});
        ZerOS::board::print("H: wants the lock\r\n");
        if (g_theater.lock() == ZerOS::sync::MutexError::Ok) {
            ZerOS::board::print("H: LOCKED (inheritance paid off)\r\n");
            g_theater.unlock();
        }
    }
}

void task_bully(void*) { // M — never touches the lock, just hogs the CPU
    for (;;) {
        ZerOS::board::print("M: running\r\n");
        ThisTask::sleep_for(Milliseconds{100});
    }
}

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
    auto& holder = ZerOS::task::named("L")
                       .prio(2)
                       .stack(stack_holder)
                       .entry(task_holder, nullptr)
                       .spawn_into(tcb_holder);
    auto& urgent = ZerOS::task::named("H")
                       .prio(0)
                       .stack(stack_urgent)
                       .entry(task_urgent, nullptr)
                       .spawn_into(tcb_urgent);
    auto& bully = ZerOS::task::named("M")
                      .prio(1)
                      .stack(stack_bully)
                      .entry(task_bully, nullptr)
                      .spawn_into(tcb_bully);

    spawn(a);
    spawn(b);
    spawn(c);
    spawn(holder);
    spawn(urgent);
    spawn(bully);

    ZerOS::arch::cortex_m3::init_time(SystemCoreClock / 1000);
    start_scheduler(idle_stack);
}
