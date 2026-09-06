#pragma once

// Life channels, so do Zeros — the ONE place formatted text leaves the
// system. Formatting never knows where; a channel never knows why.
// The board registers its UART here at wiring time.

namespace ZerOS::log {
using ActLog = bool (*)(const char*);

namespace detail {
inline ActLog the_channel = nullptr; // nobody registered = text goes nowhere
}

inline void RegisterGlobalChannel(ActLog action) {
    detail::the_channel = action;
}

inline ActLog FetchGlobalChannel() {
    return detail::the_channel;
}

} // namespace ZerOS::log
