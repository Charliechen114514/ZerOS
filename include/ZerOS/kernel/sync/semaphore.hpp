#pragma once

#include "ZerOS/base/self_list.hpp"
#include "ZerOS/kernel/clock/durations.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/system_concept.hpp"
#include <cstdint>
namespace ZerOS::sync {
enum class SyncError : std::uint8_t { Ok, TimedOut };

struct SemaWaiter : base::SelfNode<SemaWaiter> {
    constexpr explicit SemaWaiter(sched::TCB* task) noexcept : task_(task) {}

    sched::TCB* task_;
};

template <ZerOS::system::SystemContext System> struct SemaphoreBase {
    explicit constexpr SemaphoreBase(std::uint32_t initial) noexcept : count_(initial) {}
    void acquire() noexcept {
        auto& system = System::self();
        const auto& current_task_ = system.current_task();
        SemaWaiter me{current_task_.get()};

        while (1) {
            ZerOS::irq::CriticalGuard g{system};
            if (count_ > 0) {
                --count_; // down self
                waiters_.remove(&me);
                return; // quit the issue
            } else {
                waiters_.insert(&me);
                system.block(current_task_);
            }
        }
    }
    SyncError try_acquire() noexcept { return try_acquire(clock::Milliseconds{0}); }
    SyncError try_acquire(const clock::Milliseconds t) noexcept {
        auto& system = System::self();
        const auto& current_task_ = system.current_task();
        SemaWaiter me{current_task_.get()};
        const auto ddl = system.now().tick_ + t.count;

        while (1) {
            ZerOS::irq::CriticalGuard g{system};
            if (count_ > 0) {
                --count_;
                waiters_.remove(&me);
                return SyncError::Ok;
            }
            const auto left = static_cast<clock::Ticks::tick_diff_t>(ddl - system.now().tick_);
            if (left <= 0) {
                waiters_.remove(&me);
                return SyncError::TimedOut;
            }
            waiters_.insert(&me);
            system.sleep_for(system.current_task(), static_cast<clock::Ticks::tick_t>(left));
        }
    }

    void release() noexcept {
        auto& system = System::self();
        ZerOS::irq::CriticalGuard g{system};
        ++count_; // the unit ALWAYS lands in the counter — woken sleepers
                  // race for it (loser re-sleeps, that is POSIX-semantics)
        if (auto head = waiters_.pop_head()) {
            // Some one is waiting!
            system.ready(head->task_);
        }
    }

  private:
    uint32_t count_{};
    base::SelfList<SemaWaiter> waiters_{};
};

} // namespace ZerOS::sync
