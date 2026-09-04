#pragma once

#include <cstdint>
namespace ZerOS::clock {
/*
    Well Well, Clock Ticks everytime, how long is it?
    or we say, how should we define a second :)
*/
struct Ticks {
  public:
    using tick_t = uint32_t;
    using tick_diff_t = int32_t;

    constexpr Ticks() = default;
    constexpr Ticks(tick_t tick) : tick_(tick) {}

    // Or, we say is_due, but i think this is much hard to understand :(
    constexpr bool is_after(Ticks now) const noexcept {
        // We say, if the now ROLL back to 0(Register can be Overflow)
        return static_cast<tick_diff_t>(now.tick_ - tick_) >= 0;
    }

    void forward(tick_t step) noexcept { tick_ += step; }

  public:
    tick_t tick_;
};

constexpr Ticks operator+(Ticks ticks, Ticks::tick_t move_steps) {
    return {ticks.tick_ + move_steps};
}

} // namespace ZerOS::clock
