#pragma once

#include "ZerOS/base/bitmap.hpp"
#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/base/helpful_macros.hpp"
#include "ZerOS/base/self_list.hpp"
#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/sched/is_scheduler.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"

namespace ZerOS::sched {

template <typename Driver> struct Scheduler {
    // the picker only rides a Switchable driver
    static_assert(Switchable<Driver>, "Scheduler wants a Switchable driver");

    constexpr Scheduler() {
        idle_.task_priority_ = kIdlePrio;
        idle_.name_ = "idle";
    }

    void add(base::BorrowedPtr<TCB> t) {
        irq::CriticalGuard guard{driver_};
        if (!t) {
            return;
        }
        push_back(t.get());
        if (current_ != nullptr && t->task_priority_ < current_->task_priority_) {
            driver_.request_switch();
        }
    }

    [[nodiscard]] base::BorrowedPtr<TCB> current_task() const {
        return current_;
    }

    [[nodiscard]] base::BorrowedPtr<TCB> pick_next() {
        irq::CriticalGuard guard{driver_};
        if (current_ != nullptr && current_ != &idle_ && current_->state_ == TaskState::Running) {
            push_front(current_);
        }
        TCB* next = pop_highest();
        if (next == nullptr) {
            next = &idle_;
        }
        current_ = next;
        next->state_ = TaskState::Running;
        return next;
    }

    void ready(base::BorrowedPtr<TCB> t) {
        irq::CriticalGuard guard{driver_};
        if (!t || t->state_ != TaskState::Blocked) {
            return;
        }
        push_back(t.get());
        if (current_ != nullptr && t->task_priority_ < current_->task_priority_) {
            driver_.request_switch();
        }
    }

    void block(base::BorrowedPtr<TCB> t) {
        irq::CriticalGuard guard{driver_};
        if (!t) {
            return;
        }
        t->state_ = TaskState::Blocked;
        if (t == current_) {
            driver_.request_switch();
        }
    }

    void yield() {
        irq::CriticalGuard guard{driver_};
        if (current_ == nullptr || current_ == &idle_) {
            return;
        }
        push_back(current_);
        driver_.request_switch();
    }

    // Park the task on the clock: arm and block share ONE guard, so a
    // too-early wake always finds us already blocked — the wake then
    // lands, it never gets dropped. Hand-rolling this from call_after_span
    // + block reopens the gap (arm, wake, block, sleep forever).
    template <clock::TimeKeeper Time>
    void sleep_for(base::BorrowedPtr<TCB> t, Time& time, clock::Ticks::tick_t span,
                   clock::TimeWaiter::OnTimeAction wake) {
        time.park_for(&t->action, span, wake, [this, t] { block(t); });
    }

    [[nodiscard]] base::BorrowedPtr<TCB> fetch_idle_task() { return &idle_; }

  private:
    static constexpr TaskPriority_t kPriorityLevels = 32;
    static_assert(kPriorityLevels <= 32, "one-word bitmap fast path requires <= 32 levels");
    static constexpr TaskPriority_t kMaxPrio = kPriorityLevels;
    static constexpr TaskPriority_t kIdlePrio = 0xFF;

    DISABLE_COPY_MOVE(Scheduler);

    void push_back(TCB* t) {
        if (t->task_priority_ >= kMaxPrio) {
            return;
        }
        t->state_ = TaskState::Ready;
        fifo_[t->task_priority_].insert(t);
        present_.set(t->task_priority_);
    }

    void push_front(TCB* t) {
        if (t->task_priority_ >= kMaxPrio) {
            return;
        }
        t->state_ = TaskState::Ready;
        fifo_[t->task_priority_].push_front(t);
        present_.set(t->task_priority_);
    }

    TCB* pop_highest() {
        const auto level = present_.find_first_set();
        if (level == decltype(present_)::npos) {
            return nullptr;
        }
        auto t = fifo_[level].pop_head();
        if (fifo_[level].empty()) {
            present_.clear(level);
        }
        return t.get();
    }

    TCB idle_{};
    TCB* current_{nullptr};
    base::Bitmap<kPriorityLevels> present_{};
    base::SelfList<TCB> fifo_[kPriorityLevels]{};
    [[no_unique_address]] Driver driver_{};
};

} // namespace ZerOS::sched
