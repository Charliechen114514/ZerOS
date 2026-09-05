#pragma once

// The whole "upper-layer formatter" the architecture allows (kernel stays
// formatting-free). One placeholder "{}" — the TYPE says how to print, so
// there is no format-string grammar and no parser:
//
//   format_to(buf, "task={} pc=0x{}\r\n", "H", Hex{0x08000F42u});
//
#include <cstddef>
#include <span>

#include "ZerOS/log/format_impl.hpp"

namespace ZerOS::log {

// No allocation, no state, no locks → safe from tasks, ISRs and fault
// handlers. Returns the number of chars written (truncates at out.size()).
template <typename... Args>
std::size_t format_to(std::span<char> out, const char* fmt, Args... args) {
    char* cursor = out.data();
    char* limit = out.data() + out.size();
    [[maybe_unused]] auto print_arg = [&](auto&& arg) {
        if (format_impl::emit_plain(fmt, cursor, limit)) { // a placeholder for it?
            format_impl::emit_value(cursor, limit, arg);   // a plain call, one body
        }
    };
    (print_arg(args), ...);
    while (*fmt != '\0') { // trailing plain text
        format_impl::emit(cursor, limit, *fmt++);
    }
    return static_cast<std::size_t>(cursor - out.data());
}

} // namespace ZerOS::log
