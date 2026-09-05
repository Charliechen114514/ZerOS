#pragma once
#include "ZerOS/base/self_list.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/sched/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace ZerOS::sched {

struct UserTaskWrapper {
    using UserTask = void (*)(void*);
    UserTask function;
    void* argument;

    // When Marking Static, it is free, plain ABI
    static void InvokeUserTask(UserTaskWrapper& wrapper) {
        if (wrapper.function) {
            wrapper.function(wrapper.argument);
        }
    }
};

struct TCB : public base::SelfNode<TCB> {
    using StackPointer_t = std::uint32_t*;
    StackPointer_t stack_pointer_{}; // always be the first

    TaskPriority_t task_priority_{};
    TaskState state_{};

    const char* name_{}; // Constant name's for debugings

    clock::TimeWaiter action{};
    UserTaskWrapper task_wrapper_{};
    std::span<std::uint32_t> stack_view_{};
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(TCB, stack_pointer_) == sizeof(base::SelfNode<TCB>),
              "stack_pointer_ must stay immediately after the SelfNode base");
#pragma GCC diagnostic pop

} // namespace ZerOS::sched