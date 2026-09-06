#include <cstdint>

#include "stm32f1xx.h"
#include "uart.hpp"

#include "ZerOS/arch/arm_cortex_m3/cm3_irq.hpp"
#include "ZerOS/arch/arm_cortex_m3/fault.hpp"
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/sync.hpp"
#include "ZerOS/arch/arm_cortex_m3/system.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/sched/stack_guard.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/format.hpp"

namespace {

// task bodies talk to the RTOS face only; silicon names stay in main()
using ZerOS::Milliseconds;
using ZerOS::ThisTask;
using ZerOS::arch::cortex_m3::spawn;
using ZerOS::arch::cortex_m3::start_scheduler;

alignas(8) constinit std::uint32_t stack_a[64] = {};
alignas(8) constinit std::uint32_t stack_b[64] = {};
alignas(8) constinit std::uint32_t stack_c[96] = {}; // canary 32B + ~350B usable: burns in 3-4 levels
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

// the RX mailbox: a real queue now — a burst of bytes survives intact,
// no slot sharing, no hand-rolled flags
constinit ZerOS::sync::Queue<char, 16> g_rx_queue{};

// the ISR bottom half: 'd' drops a JOB here — the worker naps inside the
// job (something the interrupt could never dare), then finishes it
constinit ZerOS::sync::Worker g_worker{};

void heavy_job(void*) {
    ZerOS::board::print("worker: got the job, napping 200ms\r\n");
    ThisTask::sleep_for(Milliseconds{200}); // legal HERE — this is the whole point
    ZerOS::board::print("worker: job done\r\n");
}

// 't' arms a one-shot reminder — no task is consumed while it waits
constinit ZerOS::clock::Timer g_demo_timer{};

void timer_ping(void*) {
    ZerOS::board::print("timer: fired (tick context, no task spent)\r\n");
}

// 'e' + 'f' are a two-key interlock: the waiter parks on an AND mask and
// only comes home when BOTH bits are lit — level semantics, one bit each key
constinit ZerOS::sync::Event g_keys{};
constinit ZerOS::sched::TCBStorage tcb_evt{};
alignas(8) constinit std::uint32_t stack_evt[96] = {}; // wait chain + format buffer
                                                       // ate 6 canary words at 64 — caught live
bool g_evt_spawned = false;

void key_waiter(void*) {
    auto got = g_keys.wait(0b0011, true, Milliseconds{5000});
    if (got.has_value()) {
        ZerOS::board::format("evt: both keys in (bits=0x{})\r\n", ZerOS::log::Hex{*got});
    } else {
        ZerOS::board::print("evt: nobody pressed the second key\r\n");
    }
    ThisTask::block(); // self-retire
}

// 'n' is a three-piece relay: a task parks on its mailbox, a timer fires in
// the tick context, and the timer's callback DROPS THE LETTER straight into
// the task — no semaphore, no queue, one uint32 straight to the owner
constinit ZerOS::sched::TCBStorage tcb_nt{};
alignas(8) constinit std::uint32_t stack_nt[96] = {};
ZerOS::sched::TCB* g_nt = nullptr;

void mail_filler(void*) { // runs in TICK context — notify is safe there
    ZerOS::system::os().notify(g_nt, 42);
}

void mailbox_task(void*) {
    auto letter = ThisTask::wait_notify(Milliseconds{1000});
    if (letter.has_value()) {
        ZerOS::board::format("nt: letter says {}\r\n", ZerOS::log::Dec{*letter});
    } else {
        ZerOS::board::print("nt: mailbox stayed empty\r\n");
    }
    ThisTask::block(); // self-retire
}

// 'r' spawns two NEVER-SLEEPING spinners on the SAME level for ~3 seconds.
// Without time slicing one of them would starve the other; the slice forces
// them to take turns — the interleaved output IS the feature.
constinit ZerOS::sched::TCBStorage tcb_rr1{};
constinit ZerOS::sched::TCBStorage tcb_rr2{};
alignas(8) constinit std::uint32_t stack_rr1[64] = {};
alignas(8) constinit std::uint32_t stack_rr2[64] = {};
bool g_rr_spawned = false;

void spinner(void* tag) {
    const auto* line = static_cast<const char*>(tag);
    const auto deadline = ThisTask::now().tick_ + 3000;
    auto next_report = ThisTask::now().tick_;
    for (;;) {
        const auto now = ThisTask::now().tick_;
        if (now >= deadline) {
            break; // three seconds of proof is enough
        }
        if (now >= next_report) {
            ZerOS::board::print(line);
            next_report += 500;
        }
        // pure spinning, zero sleeps — the scheduler does the fairness
    }
    ThisTask::block(); // self-retire: sleep with nobody to wake me
}

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

// 'y' smashes task C's own canary — an honest simulation of what a real
// overflow leaves behind. (Actually burning the stack with recursion is a
// fight against the optimizer: GCC turned three "unfolds" in a row into
// loops. The mechanism under test — eaten sentinel → PendSV patrol →
// report — doesn't care who ate the words.)
void smash_canary() {
    auto cur = ZerOS::system::os().current_task();
    auto& view = ZerOS::sched::zeros_impl::TCBKeys::stack_view(*cur);
    view[7] = 0; // the deepest sentinel dies first in a real overflow
    view[6] = 0;
}

// event-sleeper: woken by the RX interrupt, not by the clock.
// 'x' = kill the chip (hard fault demo), 'y' = canary demo
void task_c(void*) {
    for (;;) {
        auto got = g_rx_queue.receive_one(); // wait forever — mailbox posture
        if (!got.has_value()) {
            continue;
        }
        const char c = *got;
        if (c == 'x') {
            ZerOS::board::print("c: executing an undefined instruction, goodbye\r\n");
            asm volatile("udf #0"); // CPU-level fault: no simulator leniency here
        } else if (c == 'y') {
            ZerOS::board::print("c: smashing my canary\r\n");
            smash_canary();
            ThisTask::sleep_for(Milliseconds{2}); // next switch patrols and reports
        } else if (c == 't') {
            ZerOS::board::print("c: timer armed, 300ms\r\n");
            g_demo_timer.oneshot(Milliseconds{300}, timer_ping, nullptr);
        } else if (c == 'r') {
            if (!g_rr_spawned) {
                g_rr_spawned = true;
                auto& r1 = ZerOS::task::named("rr1")
                               .prio(3)
                               .stack(stack_rr1)
                               .entry(spinner, const_cast<char*>("rr1 alive\r\n"))
                               .spawn_into(tcb_rr1);
                auto& r2 = ZerOS::task::named("rr2")
                               .prio(3)
                               .stack(stack_rr2)
                               .entry(spinner, const_cast<char*>("rr2 alive\r\n"))
                               .spawn_into(tcb_rr2);
                spawn(r1);
                spawn(r2);
                ZerOS::board::print("c: spinner pair out, 3s of fairness\r\n");
            }
        } else if (c == 'e') {
            if (!g_evt_spawned) {
                g_evt_spawned = true;
                auto& t = ZerOS::task::named("evt")
                              .prio(3)
                              .stack(stack_evt)
                              .entry(key_waiter, nullptr)
                              .spawn_into(tcb_evt);
                spawn(t);
                ZerOS::board::print("c: waiter out, needs BOTH keys (got bit0)\r\n");
            }
            g_keys.set(1u << 0);
        } else if (c == 'f') {
            g_keys.set(1u << 1);
        } else if (c == 'n') {
            if (g_nt == nullptr) {
                g_nt = &ZerOS::task::named("nt")
                            .prio(3)
                            .stack(stack_nt)
                            .entry(mailbox_task, nullptr)
                            .spawn_into(tcb_nt);
                spawn(*g_nt);
                ZerOS::board::print("c: mailbox task out, timer writes in 300ms\r\n");
            }
            g_demo_timer.oneshot(Milliseconds{300}, mail_filler, nullptr);
        } else {
            ZerOS::board::print("got '");
            ZerOS::board::uart1_putc(c);
            ZerOS::board::print("'\r\n");
        }
    }
}

} // namespace

// RXNE (and ORE, sharing RXNEIE): drop the byte into the queue. post()
// is ISR-safe by construction and never blocks; a full ring drops the
// byte (16 deep is plenty for a human typing)
extern "C" void USART1_IRQHandler() {
    if ((USART1->SR & USART_SR_RXNE) == 0) {
        return;
    }
    const char c = ZerOS::board::uart1_getc();
    if (c == 'd') { // bottom half: the interrupt drops a JOB, not a byte
        static_cast<void>(g_worker.defer(heavy_job, nullptr));
        return;
    }
    static_cast<void>(g_rx_queue.post(c));
}

int main() {
    ZerOS::board::uart1_init();
    ZerOS::board::print("\r\nZerOS multi-task demo @ Blue Pill\r\n");

    // the two testaments: kernel catches, board decides (print + halt)
    ZerOS::task::TaskGuardHelper::overflow_reporter =
        +[](ZerOS::base::BorrowedPtr<ZerOS::sched::TCB> t, std::size_t eaten) {
            ZerOS::board::format("[STACK OVERFLOW] task={} eaten={}\r\n",
                                 ZerOS::sched::zeros_impl::TCBKeys::name(*t),
                                 ZerOS::log::Dec{eaten});
            for (;;) {
                asm volatile("wfi");
            }
        };
    ZerOS::arch::cortex_m3::fault_sink = +[](const ZerOS::arch::cortex_m3::FaultReport& r) {
        ZerOS::board::format("\r\n*** HARD FAULT *** task={} prio={}\r\n", r.task_name,
                             ZerOS::log::Dec{r.task_prio});
        ZerOS::board::format("pc=0x{} lr=0x{} xpsr=0x{}\r\n", ZerOS::log::Hex{r.pc},
                             ZerOS::log::Hex{r.lr}, ZerOS::log::Hex{r.xpsr});
        ZerOS::board::format("cfsr=0x{} hfsr=0x{} bfar=0x{}\r\n", ZerOS::log::Hex{r.cfsr},
                             ZerOS::log::Hex{r.hfsr}, ZerOS::log::Hex{r.bfar});
        ZerOS::board::print("stack:");
        for (int i = 0; i < 8; ++i) {
            ZerOS::board::format(" {}", ZerOS::log::Hex{r.frame[i]});
        }
        ZerOS::board::print("\r\n");
    };

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
    spawn(g_worker.setup(2, "worker"));

    ZerOS::arch::cortex_m3::init_time(SystemCoreClock / 1000);
    start_scheduler(idle_stack);
}
