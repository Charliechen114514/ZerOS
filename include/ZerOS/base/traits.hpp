#pragma once

namespace ZerOS::traits {
struct AlwaysFalse {
    template <typename T> constexpr bool operator()(const T&, const T&) const { return false; }
};
} // namespace ZerOS::traits