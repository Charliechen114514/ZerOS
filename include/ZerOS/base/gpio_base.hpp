#pragma once

// Generic GPIO concepts — the board-agnostic face of a pin. Any target
// that satisfies these concepts can drive an LED, a button, a relay...
// The board layer provides the concrete register operations.

#include <concepts>
#include <cstdint>

namespace ZerOS::gpio {

enum class Direction : std::uint8_t { Input, Output };
enum class Pull : std::uint8_t { None, Up, Down };
enum class Polarity : std::uint8_t { ActiveHigh, ActiveLow };

template <typename Pin>
concept GpioPin = requires {
    { Pin::mask } -> std::convertible_to<std::uint32_t>;
    { Pin::direction } -> std::convertible_to<Direction>;
};

template <typename Pin>
concept GpioOutputPin = GpioPin<Pin> && requires {
    Pin::direction == Direction::Output;
    Pin::set();
    Pin::reset();
    Pin::toggle();
};

template <typename Pin>
concept GpioInputPin = GpioPin<Pin> && requires {
    Pin::direction == Direction::Input;
    { Pin::level() } -> std::convertible_to<bool>;
};

} // namespace ZerOS::gpio
