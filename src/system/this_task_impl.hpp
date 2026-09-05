#pragma once

// Private: the template bodies behind the ThisTask face. Only target
// .cpps and internal tests include this; the concept fires HERE, where
// the backend is complete — never on the public declaration.

#include "ZerOS/kernel/sched/this_task.hpp"

#include <concepts>

namespace ZerOS {

namespace detail {
template <class B>
concept TaskBackend = requires(Milliseconds delay) {
    { B::sleep_for(delay) } noexcept -> std::same_as<void>;
    { B::yield() } noexcept -> std::same_as<void>;
    { B::block() } noexcept -> std::same_as<void>;
};
} // namespace detail

template <class B>
void BasicThisTask<B>::sleep_for(Milliseconds delay) noexcept {
    static_assert(detail::TaskBackend<B>);
    B::sleep_for(delay);
}

template <class B>
void BasicThisTask<B>::yield() noexcept {
    static_assert(detail::TaskBackend<B>);
    B::yield();
}

template <class B>
void BasicThisTask<B>::block() noexcept {
    static_assert(detail::TaskBackend<B>);
    B::block();
}

} // namespace ZerOS
