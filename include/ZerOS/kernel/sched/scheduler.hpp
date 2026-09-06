#pragma once

#include "ZerOS/base/bitmap.hpp"
#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/base/helpful_macros.hpp"
#include "ZerOS/base/self_list.hpp"
#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/sched/is_scheduler.hpp"
#include "ZerOS/kernel/sched/stack.hpp"
#include "ZerOS/kernel/sched/stack_guard.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/sched/types.hpp"
#include "ZerOS/kernel/sync/sync_error.hpp"
#include <expected>
namespace ZerOS::sched {

template <typename Driver> struct Scheduler {
    // the picker only rides a Switchable driver
    static_assert(Switchable<Driver>, "Scheduler wants a Switchable driver");

    static constexpr clock::Ticks::tick_t kSliceTicks = 10;

    constexpr Scheduler() {
        idle_.task_priority_ = kIdlePrio;
        idle_.name_ = "idle";
        idle_.stack_view_ = idle_stack_; // the idle task is OURS: its stack
                                         // comes with us, callers never know
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

    [[nodiscard]] base::BorrowedPtr<TCB> current_task() const { return current_; }

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
        ticks_served_ = 0; // a fresh pick is a fresh slice: no inheriting
                           // the previous runner's leftover share
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

    template <clock::TimeKeeper Time> void sleep_for(base::BorrowedPtr<TCB> t, Time& time,
                                                     clock::Ticks::tick_t span,
                                                     clock::TimeWaiter::OnTimeAction wake) {
        time.park_for(&t->action, span, wake, [this, t] { block(t); });
    }

    void reprioritize(base::BorrowedPtr<TCB> t, TaskPriority_t new_prio) {
        irq::CriticalGuard guard{driver_};
        if (t->task_priority_ == new_prio) {
            return;
        }
        if (t->state_ == TaskState::Ready) {
            fifo_[t->task_priority_].remove(t);
            if (fifo_[t->task_priority_].empty()) {
                present_.clear(t->task_priority_);
            }
            t->task_priority_ = new_prio;
            push_back(t.get());
        } else {
            t->task_priority_ = new_prio;
        }
    }

    // a plain read; diagnostics and the boost-or-not decision both want it
    [[nodiscard]] TaskPriority_t prio(base::BorrowedPtr<TCB> t) const { return t->task_priority_; }

    // Direct-to-task mailbox drop. Task/ISR safe. Single slot, OVERWRITE:
    // the latest value wins. The target may be parked ANYWHERE (a semaphore,
    // a queue, a sleep) — the ready() below may wake it "by mistake", and
    // that is fine: every waiter family re-checks its own truth on waking
    // and simply goes back to sleep; the mailbox owner checks has_notify_.
    bool notify(base::BorrowedPtr<TCB> t, std::uint32_t value) noexcept {
        irq::CriticalGuard guard{driver_};
        if (!t || t == &idle_) {
            return false;
        }
        t->notify_ = value;
        t->has_notify_ = true;
        if (t->state_ == TaskState::Blocked) {
            ready(t); // nested guard; pends the switch, never switches here
        }
        return true;
    }

    // the current task reads its mailbox; taking the letter clears the flag.
    // ONE atomic probe only — the re-check loop lives in the system face,
    // whose sleep goes through the door the Mock can see (an inner
    // Scheduler-side loop would park via kernel-direct block and dodge it)
    std::expected<std::uint32_t, sync::SyncError> wait_notify() noexcept {
        irq::CriticalGuard guard{driver_};
        if (current_ != nullptr && current_->has_notify_) {
            current_->has_notify_ = false;
            return current_->notify_;
        }
        return std::unexpected(sync::SyncError::TimedOut);
    }

    [[nodiscard]] base::BorrowedPtr<TCB> fetch_idle_task() { return &idle_; }

    void on_tick() {
        irq::CriticalGuard g{driver_};
        if (!current_ || current_ == &idle_) {
            return;
        }
        // Ticks Served
        ++ticks_served_;

        if (ticks_served_ < kSliceTicks) {
            return; // OK, back
        }

        ticks_served_ = 0; // reset, time to sched
        if (fifo_[current_->task_priority_].empty()) {
            return; // Dont dry run self
        }

        push_back(current_);
        current_->state_ = TaskState::Ready;
        driver_.request_switch(); // Ask For a switch, wait PendSV
    }

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
    task::TaskStack<32> idle_stack_{}; // 128B, the D12 budget line
    TCB* current_{nullptr};
    base::Bitmap<kPriorityLevels> present_{};
    base::SelfList<TCB> fifo_[kPriorityLevels]{};
    [[no_unique_address]] Driver driver_{};
    std::uint32_t ticks_served_{};
};

} // namespace ZerOS::sched
