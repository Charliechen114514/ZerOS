#pragma once

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"

#include <concepts>
namespace ZerOS::system {
template <typename System>
concept SystemContext = requires(System& s, base::BorrowedPtr<sched::TCB> t,
                                 clock::Ticks::tick_t span) {
    { s.current_task() } -> std::same_as<base::BorrowedPtr<sched::TCB>>;
    { s.ready(t) };
    { s.block(t) };
    { s.sleep_for(t, span) };   // how a sleeper gets woken is the system's secret
    { s.yield() };
    { s.now() } -> std::same_as<clock::Ticks>;
    { s.self() } -> std::same_as<System&>;
    s.lock();
    s.unlock();
};
} // namespace ZerOS::system
