#pragma once

#include <type_traits>

namespace ZerOS::base {

// the bare-metal currency: a function plus its context. No closures, no
// heap — whenever something must "call back later", this is the coin.
struct Callback {
    using Fn = void (*)(void*);

    Fn fn{};
    void* arg{};

    constexpr Callback() = default;
    constexpr Callback(Fn f, void* a) noexcept : fn(f), arg(a) {}

    void invoke() const noexcept {
        if (fn != nullptr) {
            fn(arg);
        }
    }
    explicit operator bool() const noexcept { return fn != nullptr; }
};

static_assert(std::is_trivially_copyable_v<Callback>); // must ride queues and TCBs

} // namespace ZerOS::base
