#pragma once
#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include "ZerOS/kernel/clock/timequeue.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"

#include <concepts>

namespace ZerOS::clock {

template <typename Portable>
concept Armable = requires(Portable p, Ticks::tick_t ticks) {
    p.arm(ticks);
    p.disarm();
};

template <typename IsTimePort>
concept TimePortable = Armable<IsTimePort> && irq::CriticalSection<IsTimePort>;

// a keeper runs the ledger AND can park a waiter together with a
// "now block yourself" tail — the atomic compound sleep needs both
template <typename Keeper>
concept TimeKeeper = requires(Keeper& k, base::BorrowedPtr<TimeWaiter> w,
                              TimeWaiter::OnTimeAction wake, Ticks::tick_t span) {
    k.park_for(w, span, wake, [] {});
    { k.current() } -> std::same_as<Ticks>;
};

template <typename Driver> struct Kernel {
    // the ledger keeper only rides a TimePortable driver
    static_assert(TimePortable<Driver>, "Kernel wants a TimePortable driver");

    // A Timer fetch w for span; the reaction stays whatever it was born with
    void call_after_span(base::BorrowedPtr<TimeWaiter> task, Ticks::tick_t span) {
        irq::CriticalGuard guard{lowlevel_driver_};
        enqueue_locked(task, span, nullptr);
    }

    // Parking: arm AND block inside ONE guard. A wake slipping in between
    // the two steps would find the task un-blocked, drop it, and the task
    // sleeps forever — the order here IS the fix
    template <typename ParkBlock> void park_for(base::BorrowedPtr<TimeWaiter> task,
                                                Ticks::tick_t span, TimeWaiter::OnTimeAction wake,
                                                ParkBlock&& block_now) {
        irq::CriticalGuard guard{lowlevel_driver_};
        enqueue_locked(task, span, wake);
        block_now(); // still under the very same lock
    }

    // if true, we cancel success
    bool cancel(base::BorrowedPtr<TimeWaiter> task) {
        irq::CriticalGuard guard{lowlevel_driver_};
        bool result = queue_.remove(task);
        update_alarm();
        return result;
    }

    Ticks current() const { return clock_.current(); }

    // ISR side call
    void on_elapsed(Ticks::tick_t n) {
        if (n == 0) {
            return;
        }
        irq::CriticalGuard guard{lowlevel_driver_};
        clock_.tick_forward(n);
        while (auto due = queue_.pop_due(clock_.current())) {
            due->OnTime(due);
        }
        update_alarm();
    }

  private:
    // caller must hold the guard; wake is optional (nullptr keeps the born-with reaction)
    void enqueue_locked(base::BorrowedPtr<TimeWaiter> task, Ticks::tick_t span,
                        TimeWaiter::OnTimeAction wake) {
        queue_.remove(task);
        if (wake != nullptr) {
            task->OnTime = wake;
        }

        // set the deadline
        task->deadline_ = clock_.current() + span;
        queue_.insert(task);
        update_alarm();
    }

    void update_alarm() {
        auto head = queue_.head();
        if (!head) {
            if (arm_valid_) {
                lowlevel_driver_.disarm();
                arm_valid_ = false;
            }
            return;
        }

        const auto now = clock_.current().tick_;
        bool armed_pending = arm_valid_ && static_cast<Ticks::tick_diff_t>(armed_.tick_ - now) > 0;
        bool head_earlier =
            arm_valid_ && static_cast<Ticks::tick_diff_t>(armed_.tick_ - head->deadline_.tick_) > 0;

        if (armed_pending && !head_earlier) {
            return;
        }

        auto remaining = static_cast<Ticks::tick_diff_t>(head->deadline_.tick_ - now);
        if (remaining < 1) {
            remaining = 1; // Clamp to 1, call it next
        }
        lowlevel_driver_.arm(static_cast<Ticks::tick_t>(remaining));
        armed_ = head->deadline_;
        arm_valid_ = true;
    }

  private:
    Clock clock_{};
    TimerQueue queue_{};
    Ticks armed_{};
    bool arm_valid_{false};
    [[no_unique_address]] Driver lowlevel_driver_{};
};
} // namespace ZerOS::clock
