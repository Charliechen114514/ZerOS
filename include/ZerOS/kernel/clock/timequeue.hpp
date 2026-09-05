#pragma once

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
namespace ZerOS::clock {
struct TimerQueue {
    struct DeadLineOrder {
        bool operator()(const TimeWaiter& lhs, const TimeWaiter& rhs) const noexcept {
            return static_cast<Ticks::tick_diff_t>(rhs.deadline_.tick_ - lhs.deadline_.tick_) > 0;
        }
    };

    void insert(base::BorrowedPtr<TimeWaiter> timer_task) { list_.insert(timer_task); }
    [[nodiscard]] base::BorrowedPtr<TimeWaiter> head() const { return list_.head(); }
    bool remove(base::BorrowedPtr<TimeWaiter> w) { return list_.remove(w); }

    /**
     * @brief Queue works py using pop due
     *
     * @param now
     * @return base::BorrowedPtr<TimeWaiter>
     */
    [[nodiscard]] base::BorrowedPtr<TimeWaiter> pop_due(Ticks now) {
        // Do WE have task? and is there something we need to do by given NOW ticks
        if (list_.head() && list_.head()->deadline_.is_after(now)) {
            return list_.pop_head();
        }
        return nullptr;
    }

  private:
    base::SelfList<TimeWaiter, DeadLineOrder> list_;
};
} // namespace ZerOS::clock
