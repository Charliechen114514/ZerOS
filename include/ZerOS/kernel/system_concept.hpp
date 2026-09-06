#pragma once

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/sync/sync_error.hpp"

#include <concepts>
#include <expected>
namespace ZerOS::system {
template <typename System>
concept SystemContext = requires(System& s, base::BorrowedPtr<sched::TCB> t,
                                 base::BorrowedPtr<clock::TimeWaiter> w,
                                 clock::Ticks::tick_t span,
                                 sched::TaskPriority_t prio) {
    { s.current_task() } -> std::same_as<base::BorrowedPtr<sched::TCB>>;
    { s.ready(t) };
    { s.block(t) };
    { s.sleep_for(t, span) };   // how a sleeper gets woken is the system's secret
    { s.yield() };
    { s.now() } -> std::same_as<clock::Ticks>;
    { s.self() } -> std::same_as<System&>;
    { s.prio(t) } -> std::same_as<sched::TaskPriority_t>;  // PI wants to read
    { s.reprioritize(t, prio) };                            // ...and to lift
    { s.arm_timer(w, span) };   // book a waiter on the clock ledger (timers)
    { s.cancel_timer(w) };      // ...and strike it off (idempotent)
    { s.notify(t, 42u) } -> std::same_as<bool>;                    // mailbox drop
    { s.wait_notify(span) } -> std::same_as<std::expected<std::uint32_t, sync::SyncError>>;
    s.lock();
    s.unlock();
};
} // namespace ZerOS::system
