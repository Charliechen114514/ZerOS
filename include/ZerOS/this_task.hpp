#pragma once

// The public face of "the task that is calling me": sleep, yield, block.
// Business code includes ONLY this header — which silicon answers the
// call is wiring's secret (see src/system + src/arch/<target>).

#include <cstdint>

namespace ZerOS {

// plain unit, zero cost, keeps milliseconds from being confused with ticks
struct Milliseconds {
    std::uint32_t count;
};

namespace detail {
struct SystemTaskBackend; // neutral tag; the real one lives in a target .cpp
}

template <class Backend>
struct BasicThisTask {
    BasicThisTask() = delete; // a face, not a thing: no instances, ever

    static void sleep_for(Milliseconds delay) noexcept;
    static void yield() noexcept;
    // raw "sleep until someone readies me" — the real event primitives
    // (semaphores and friends) will grow on top of this
    static void block() noexcept;
};

extern template struct BasicThisTask<detail::SystemTaskBackend>;

using ThisTask = BasicThisTask<detail::SystemTaskBackend>;

} // namespace ZerOS
