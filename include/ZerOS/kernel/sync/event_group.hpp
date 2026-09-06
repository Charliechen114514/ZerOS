#pragma once

#include "ZerOS/base/self_list.hpp"
#include "ZerOS/kernel/clock/durations.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/sync/semaphore.hpp"
#include "ZerOS/kernel/system_concept.hpp"
#include <cstdint>
#include <expected>

namespace ZerOS::sync {

// Event Group
template <ZerOS::system::SystemContext System> struct EventBase {
    struct EventWaiter : base::SelfNode<EventWaiter> {
        constexpr explicit EventWaiter(sched::TCB* task) noexcept : task_(task) {}
        sched::TCB* task_;
    };

    // wait forever for the mask (any lit bit, or all of them)
    std::expected<uint32_t, SyncError> wait(uint32_t mask, bool all_of) noexcept {
        auto& system = System::self();

        while (1) {
            irq::CriticalGuard g{system};
            if (auto hit = satisfied_or(mask, all_of); hit.has_value()) {
                return hit;
            }
            EventWaiter me{system.current_task().get()};
            waiters_.insert(&me);
            system.block(system.current_task()); // park + sleep, one lock — no gap
        }
    }

    // wait at most `timeout`; zero means a single non-blocking try
    std::expected<uint32_t, SyncError> wait(uint32_t mask, bool all_of,
                                            ZerOS::clock::Milliseconds timeout) noexcept {
        auto& system = System::self();
        EventWaiter me{system.current_task().get()};
        const auto ddl = system.now().tick_ + timeout.count;

        while (1) {
            irq::CriticalGuard g{system};
            if (auto hit = satisfied_or(mask, all_of); hit.has_value()) {
                waiters_.remove(&me);
                return hit;
            }
            const auto left = static_cast<clock::Ticks::tick_diff_t>(ddl - system.now().tick_);
            if (left <= 0) {
                waiters_.remove(&me);
                return std::unexpected(SyncError::TimedOut);
            }
            waiters_.insert(&me);
            system.sleep_for(system.current_task(), static_cast<clock::Ticks::tick_t>(left));
        }
    }

    // Light bits. TASK/ISR safe. Wakes EVERY waiter — broadcast: the setter
    // never judges who is satisfied; each woken one re-checks its own mask
    // at the loop head (a spurious wake-up costs one loop turn, nothing more).
    void set(uint32_t bits) noexcept {
        auto& system = System::self();
        irq::CriticalGuard g{system};
        bits_ |= bits;
        while (auto w = waiters_.pop_head()) {
            system.ready(w->task_);
        }
    }

    // Anyone can clear
    void clear(uint32_t bits) noexcept {
        auto& system = System::self();
        irq::CriticalGuard g{system};
        bits_ &= ~bits;
    }

  private:
    [[nodiscard]] std::expected<uint32_t, SyncError> satisfied_or(uint32_t mask,
                                                                  bool all_of) const noexcept {
        const uint32_t hit = bits_ & mask;
        if (hit != 0 && (!all_of || hit == mask)) {
            return hit;
        }
        return std::unexpected(SyncError::TimedOut); // "not satisfied yet"
    }

    uint32_t bits_{};
    base::SelfList<EventWaiter> waiters_{};
};
} // namespace ZerOS::sync
