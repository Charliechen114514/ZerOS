#pragma once

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/base/self_list.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
namespace ZerOS::clock {

struct Clock {
    [[nodiscard("Hey boy, dont throw me away!")]]
    constexpr Ticks current() const noexcept {
        return now_;
    };

    constexpr void tick_forward(Ticks::tick_t steps) { now_.forward(steps); }

  private:
    Ticks now_{};
};

// Wait... oh it's time to take action!
struct TimeWaiter : public base::SelfNode<TimeWaiter> {
    using OnTimeAction = void (*)(base::BorrowedPtr<TimeWaiter> self);
    Ticks deadline_;
    OnTimeAction OnTime;

    constexpr TimeWaiter() = default;
    constexpr TimeWaiter(Ticks deadline, OnTimeAction on_time)
        : deadline_(deadline), OnTime(on_time) {};
};

} // namespace ZerOS::clock