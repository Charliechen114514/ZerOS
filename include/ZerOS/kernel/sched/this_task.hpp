#pragma once
#include <cstdint>
#include <expected>

#include "ZerOS/kernel/clock/durations.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include "ZerOS/kernel/sync/sync_error.hpp"

namespace ZerOS {

// one unit for the whole house (clock::Milliseconds, durations.hpp)
using clock::Milliseconds;

namespace detail {
struct SystemTaskBackend; // neutral tag; the real one lives in a target .cpp
}

template <class Backend> struct BasicThisTask {
    BasicThisTask() = delete; // a face, not a thing: no instances, ever

    static void sleep_for(Milliseconds delay) noexcept;
    static void yield() noexcept;
    // raw "sleep until someone readies me" — the real event primitives
    // (semaphores and friends) will grow on top of this
    static void block() noexcept;
    [[nodiscard]] static ZerOS::clock::Ticks now() noexcept;
    // read the mailbox: zero = one try, otherwise wait up to `timeout`
    [[nodiscard]] static std::expected<std::uint32_t, ZerOS::sync::SyncError>
    wait_notify(ZerOS::clock::Milliseconds timeout) noexcept;
};

extern template struct BasicThisTask<detail::SystemTaskBackend>;

using ThisTask = BasicThisTask<detail::SystemTaskBackend>;

} // namespace ZerOS