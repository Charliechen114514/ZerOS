#pragma once

#include <cstddef>

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/base/callback.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/clock/durations.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/system_concept.hpp"

namespace ZerOS::clock {
template <ZerOS::system::SystemContext System> struct TimerBase {
    constexpr TimerBase() noexcept : waiter_{Ticks{0}, &TimerBase::fired} {}

    // once: fn(arg) fires after `span`, then the timer goes cold
    void oneshot(Milliseconds span, base::Callback::Fn fn, void* arg) noexcept {
        arm_me(0, span, base::Callback{fn, arg});
    }

    // forever: every `span` — until stop(), even from inside fn
    void periodic(Milliseconds span, base::Callback::Fn fn, void* arg) noexcept {
        arm_me(span.count, span, base::Callback{fn, arg});
    }

    void stop() noexcept {
        auto& s = System::self();
        ZerOS::irq::CriticalGuard g{s};
        period_ = 0;
        running_ = false;
        s.cancel_timer(&waiter_); // idempotent: not booked = false, harmless
    }

    [[nodiscard]] static TimerBase* owner_of(base::BorrowedPtr<clock::TimeWaiter> waiter) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
        return reinterpret_cast<TimerBase*>(reinterpret_cast<std::byte*>(waiter.get()) -
                                            offsetof(TimerBase, waiter_));
#pragma GCC diagnostic pop
    }

  private:
    void arm_me(Ticks::tick_t period, Milliseconds span, base::Callback callback) {
        auto& s = System::self();
        ZerOS::irq::CriticalGuard g{s}; // fields only move under the lock —
        period_ = period;               // a firing tick may be reading them
        call_ = callback;
        running_ = true;
        s.arm_timer(&waiter_, span.count);
    }

    static void fired(base::BorrowedPtr<TimeWaiter> w) {
        auto* t = owner_of(w);
        t->running_ = false;    // oneshot cools off by default
        t->call_.invoke();      // user fn (tick context!)
        if (t->period_ != 0) {  // periodic: re-book — a stop() INSIDE fn
            t->running_ = true; // already zeroed period_ and walks free
            System::self().arm_timer(w, t->period_);
        }
    }

  private:
    TimeWaiter waiter_; // born with its reaction
    base::Callback call_{};
    Ticks::tick_t period_{}; // 0 = oneshot
    bool running_{};
};
} // namespace ZerOS::clock
