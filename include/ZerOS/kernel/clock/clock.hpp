#pragma once

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/base/self_list.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include <type_traits>
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

    constexpr TimeWaiter() = default;
    // Born with a reaction: this is the registration face,
    // rewriting the ledger later belongs to the kernel alone
    constexpr TimeWaiter(Ticks deadline, OnTimeAction on_time)
        : deadline_(deadline), OnTime(on_time) {};

    // reading is free for diagnosis, writing is not
    [[nodiscard]] constexpr Ticks deadline() const noexcept { return deadline_; }

  private:
    template <typename> friend struct Kernel;
    friend struct TimerQueue;

    Ticks deadline_{};
    OnTimeAction OnTime{nullptr};
};

static_assert(std::is_trivially_copyable_v<TimeWaiter>);

} // namespace ZerOS::clock