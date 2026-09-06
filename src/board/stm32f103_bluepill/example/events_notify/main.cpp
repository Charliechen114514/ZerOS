/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Two-key interlock + a direct mailbox: one waiter blocks on an
 *          AND mask over two event bits; two timers act as two hands,
 *          pressing key A at 400ms and key B at 900ms. A third timer
 *          drops a letter straight into a task's mailbox at 600ms — no
 *          semaphore, no queue, one uint32 to the owner.
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/sync.hpp"
#include "ZerOS/arch/arm_cortex_m3/system.hpp"
#include "ZerOS/kernel/clock/timer.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include <cstdint>

namespace {

using ZerOS::Milliseconds;
using ZerOS::ThisTask;

constinit ZerOS::sync::Event g_keys{};
constinit ZerOS::clock::Timer g_hand_a{};
constinit ZerOS::clock::Timer g_hand_b{};
constinit ZerOS::clock::Timer g_postman{};
ZerOS::sched::TCB* g_owner = nullptr;

void press_a(void*) { // tick context: light one bit
    g_keys.set(1u << 0);
}

void press_b(void*) {
    g_keys.set(1u << 1);
}

void drop_letter(void*) { // direct-to-task: no object in between
    ZerOS::system::os().notify(g_owner, 42);
}

void key_waiter(void*) {
    auto got = g_keys.wait(0b0011, true); // wait forever, ALL bits
    ZerOS::log::print("evt: both keys in (bits=0x{})\r\n", ZerOS::log::Hex{*got});
    ThisTask::block(); // self-retire: job done
}

void mailbox_task(void*) {
    g_owner = ZerOS::system::os().current_task().get(); // the postman's target
    auto letter = ThisTask::wait_notify(Milliseconds{2000});
    if (letter.has_value()) {
        ZerOS::log::print("nt: letter says {}\r\n", ZerOS::log::Dec{*letter});
    } else {
        ZerOS::log::print("nt: mailbox stayed empty\r\n");
    }
    ThisTask::block();
}

} // namespace

int main() {
    ZerOS::demo::setup("events and notify");

    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("waiter").prio(1).words(96).entry(key_waiter, nullptr));
    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("reader").prio(1).words(96).entry(mailbox_task, nullptr));

    g_hand_a.oneshot(Milliseconds{400}, press_a, nullptr);
    g_hand_b.oneshot(Milliseconds{900}, press_b, nullptr);
    g_postman.oneshot(Milliseconds{600}, drop_letter, nullptr);

    ZerOS::arch::cortex_m3::start_scheduler();
}
