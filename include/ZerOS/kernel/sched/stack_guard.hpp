#pragma once
#include "ZerOS/kernel/sched/task_control_block.hpp"

#include <cstdint>

namespace ZerOS::task {
constexpr inline std::uint32_t kCanaryWord = 0xEF114514;
constexpr inline std::size_t kCanarySize = 8;

struct TaskGuardHelper {
    static void bury_canary(sched::TCB& t) {
        auto& stack_view = sched::zeros_impl::TCBKeys::stack_view(t);
        // word-wise: memset fills BYTES, it would chop the pattern to 0x14
        for (std::size_t i = 0; i < kCanarySize && i < stack_view.size(); ++i) {
            stack_view[i] = kCanaryWord;
        }
    }

    static bool fast_check_stack(sched::TCB& t) {
        // the deepest sentinel is the first thing an overflow eats
        return sched::zeros_impl::TCBKeys::stack_view(t)[kCanarySize - 1] != kCanaryWord;
    }

    static std::size_t eaten_length(sched::TCB& t) {
        auto& stack_view = sched::zeros_impl::TCBKeys::stack_view(t);
        std::size_t length = 0;
        for (std::size_t i = 0; i < kCanarySize; ++i) {
            if (stack_view[i] != kCanaryWord) {
                length++;
            }
        }
        return length;
    }

    using OverflowReport = void (*)(base::BorrowedPtr<sched::TCB>, std::size_t eaten);
    static inline OverflowReport overflow_reporter{nullptr};
};

} // namespace ZerOS::task
