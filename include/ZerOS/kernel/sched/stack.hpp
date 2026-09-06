#pragma once

// The task stack as a TYPE: alignment is nailed in here, not remembered
// by the user. A bare `uint32_t arr[N]` only promises 4-byte alignment —
// a task started on it carries a misaligned SP and dies mysteriously
// (AAPCS wants SP 8-aligned at public call sites). Even word count keeps
// the stack TOP aligned too.

#include <cstdint>
#include <span>

namespace ZerOS::task {

template <std::size_t Words>
struct TaskStack {
    static_assert(Words % 2 == 0, "even word count keeps the top 8-aligned");

    alignas(8) std::uint32_t words_[Words]{};

    constexpr operator std::span<std::uint32_t>() noexcept { return words_; }
};

static_assert(alignof(TaskStack<8>) == 8);

} // namespace ZerOS::task
