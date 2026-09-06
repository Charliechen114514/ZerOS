#pragma once

// LED — a pin with an opinion about which level means "lit".
// The polarity dissolves at compile time: zero runtime cost.

#include "ZerOS/base/gpio_base.hpp"

namespace ZerOS::gpio {

template <GpioOutputPin Pin, Polarity POLARITY = Polarity::ActiveHigh>
struct LED {
    static void on() {
        if constexpr (POLARITY == Polarity::ActiveHigh) {
            Pin::set();
        } else {
            Pin::reset();
        }
    }

    static void off() {
        if constexpr (POLARITY == Polarity::ActiveLow) {
            Pin::set();
        } else {
            Pin::reset();
        }
    }

    static void toggle() { Pin::toggle(); }
};

} // namespace ZerOS::gpio
