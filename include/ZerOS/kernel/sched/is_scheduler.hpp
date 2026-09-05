#pragma once

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include <concepts>
namespace ZerOS::sched {
struct TCB;

template <typename SwitchPort>
concept Switchable =
    irq::CriticalSection<SwitchPort> && requires(SwitchPort& p) { p.request_switch(); };

template <typename Stuff>
concept IsScheduler = requires(Stuff& stuff, base::BorrowedPtr<TCB> aTask) {
    // A Scheduler can add a task
    { stuff.add(aTask) };
    // A Scheduler can tell current task is what
    { stuff.current_task() } -> std::same_as<base::BorrowedPtr<TCB>>;
    // A Scheduler can tell what task should be executed next
    { stuff.pick_next() } -> std::same_as<base::BorrowedPtr<TCB>>;
    { stuff.ready(aTask) }; // Wake up task
    { stuff.block(aTask) }; // Block the task
    { stuff.yield() };      // Yield or hang a task
};
} // namespace ZerOS::sched