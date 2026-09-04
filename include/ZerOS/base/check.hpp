#pragma once
#include "ZerOS/log/global_channel.hpp"

#include <source_location>

namespace ZerOS::debug {
inline constexpr void Check(bool ShouldBeTrue, [[maybe_unused]] const char* msg,
                            [[maybe_unused]] std::source_location loc = std::source_location::current()) {
    if (ShouldBeTrue) {
        return;
    }

    // auto global_channel = ZerOS::log::FetchGlobalChannel();
    // global_channel();

    // Oh we sucks!
    while (1) {
        // Halt here in debug mode
    }
}
} // namespace ZerOS::debug
