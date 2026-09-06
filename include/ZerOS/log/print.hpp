#pragma once

// format → the registered global channel, in one breath. No hardware
// anywhere near this file: where the text lands is the board's business
// (register a channel), not the formatter's.
//
// An unregistered channel DROPS the line — logging is a convenience, not
// an obligation. (Testaments are obligations; those have their own teeth.)

#include "ZerOS/log/format.hpp"
#include "ZerOS/log/global_channel.hpp"

namespace ZerOS::log {

namespace detail {
// one line at a time; long lines truncate, they never overrun.
// 128 keeps a 64-word task stack printable (160 overflowed it — the
// canary testified twice); heavy printers should ask for 96 words
inline constexpr std::size_t kLineBuffer = 128;
}

template <typename... Args>
void print(const char* fmt, Args... args) {
    char line[detail::kLineBuffer];
    const auto n = format_to(line, fmt, args...);
    line[n < detail::kLineBuffer ? n : detail::kLineBuffer - 1] = '\0';
    if (const auto channel = FetchGlobalChannel()) {
        channel(line);
    }
}

} // namespace ZerOS::log
