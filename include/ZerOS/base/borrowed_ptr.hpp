#pragma once

#include "ZerOS/base/check.hpp"
#include <type_traits>
namespace ZerOS::base {
/**
 * @brief Bro I borrowed this :)
 *
 * @tparam BorrowedSrc
 */
template <typename BorrowedSrc> struct BorrowedPtr {
    constexpr BorrowedPtr() = default;
    constexpr BorrowedPtr(BorrowedSrc* src) noexcept : source(src) {}

    constexpr explicit operator bool() const { return source != nullptr; }
    constexpr BorrowedSrc* operator->() const { return source; }
    [[nodiscard]] constexpr BorrowedSrc* get() const { return source; }
    constexpr BorrowedSrc& operator*() const {
        debug::Check(source, "Deref NulPtr");
        return *source;
    }

    friend constexpr bool operator==(BorrowedPtr a, BorrowedSrc* b) { return a.source == b; }
    friend constexpr bool operator==(BorrowedPtr a, BorrowedPtr b) { return a.source == b.source; }

  private:
    BorrowedSrc* source{nullptr};
};

// Make A Compile Time Assumptions that we COST NOTHING
static_assert(sizeof(BorrowedPtr<int>) == sizeof(int*));
static_assert(std::is_trivially_copyable_v<BorrowedPtr<int>>);

} // namespace ZerOS::base
