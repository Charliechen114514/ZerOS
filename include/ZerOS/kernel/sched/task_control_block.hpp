#pragma once
#include "ZerOS/base/self_list.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/sched/types.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <type_traits>

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

namespace zeros_impl {
struct TCBKeys; // the keyhole, declared early so the lock knows its guard
}

class TCB : public base::SelfNode<TCB> {
  public:
    friend struct TaskGuardHelper;
    using StackPointer_t = std::uint32_t*;

    // Wait... who owns this waiter? The node sleeps inside its owner,
    // waking walks the road back from node to TCB
    [[nodiscard]] static TCB* owner_of(base::BorrowedPtr<clock::TimeWaiter> waiter) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
        return reinterpret_cast<TCB*>(reinterpret_cast<std::byte*>(waiter.get()) -
                                      offsetof(TCB, action));
#pragma GCC diagnostic pop
    }

  private:
    template <typename> friend struct Scheduler;
    friend class TCBCreator;
    friend struct zeros_impl::TCBKeys;

    StackPointer_t stack_pointer_{}; // always be the first
    TaskPriority_t task_priority_{};
    TaskState state_{};

    const char* name_{}; // Constant name's for debugings

    clock::TimeWaiter action{};
    UserTaskWrapper task_wrapper_{};
    std::span<std::uint32_t> stack_view_{};

    // the task's private mailbox: direct notifications, no kernel object.
    // Single slot, LATEST value wins (overwrite) — see Scheduler::notify.
    std::uint32_t notify_{};
    bool has_notify_{};
};

// Meaning-free RAM: zero it, park it. The only pen that writes meaning
// into it is TCBCreator::spawn_into, so a half-built task is not a thing.
struct TCBStorage {
    alignas(alignof(TCB)) std::byte bytes[sizeof(TCB)];
};

// The factory's final word: every field is in, now it may write a TCB.
// The chain fills it, spawn_into spends it.
class TCBCreator {
  public:
    // The one legal pen: fresh zero bits + meaning, hands back the handle
    TCB& spawn_into(TCBStorage& storage) const {
        auto& tcb = *new (storage.bytes) TCB{}; // private birth, but we hold the pen
        tcb.name_ = name;
        tcb.task_priority_ = prio;
        tcb.task_wrapper_ = run;
        tcb.stack_view_ = stack;
        return tcb;
    }

    // filled by the builder chain, nothing to hide in a receipt
    const char* name{};
    TaskPriority_t prio{};
    std::span<std::uint32_t> stack{};
    UserTaskWrapper run{};
};

namespace zeros_impl {
// The keyhole for naked mechanics: the PendSV path and the whitebox tests.
// If you are neither, you are just walking by, please use the scheduler.
struct TCBKeys {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
    static_assert(offsetof(TCB, stack_pointer_) == sizeof(base::SelfNode<TCB>),
                  "stack_pointer_ must stay immediately after the SelfNode base");
#pragma GCC diagnostic pop

    static TCB::StackPointer_t& sp(TCB& t) { return t.stack_pointer_; }
    static UserTaskWrapper& wrapper(TCB& t) { return t.task_wrapper_; }
    static std::span<std::uint32_t>& stack_view(TCB& t) { return t.stack_view_; }
    static TaskPriority_t prio(TCB& t) { return t.task_priority_; }
    static TaskState state(TCB& t) { return t.state_; }
    static void set_state(TCB& t, TaskState s) { t.state_ = s; }
    static const char* name(TCB& t) { return t.name_; }
};
} // namespace zeros_impl

// The storage contract: zero the bytes, hand them to the factory.
// - trivially copyable / destructible: the PendSV road swaps a TCB
//   by raw words and never runs a destructor;
// - default construction is NOT trivial (libstdc++ span() initializes
//   its members), which is fine: birth only ever happens through
//   TCBCreator's placement new, zero bits alone are never a TCB.
static_assert(std::is_trivially_copyable_v<TCB>);
static_assert(std::is_trivially_destructible_v<TCB>);
static_assert(sizeof(TCBStorage) == sizeof(TCB));
static_assert(alignof(TCBStorage) == alignof(TCB));

} // namespace ZerOS::sched
