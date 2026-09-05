#pragma once

#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/system_concept.hpp"
#include <cstdint>

namespace ZerOS::sync {

enum class MutexError : std::uint8_t { Ok, NotYourLock, SelfLocked };

struct MutexWaiter : base::SelfNode<MutexWaiter> {
    constexpr explicit MutexWaiter(sched::TCB* task, sched::TaskPriority_t prio) noexcept
        : task_(task), prio_(prio) {}
    sched::TCB* task_;
    sched::TaskPriority_t prio_;
};

template <ZerOS::system::SystemContext System> struct MutexBase {
    constexpr MutexBase() noexcept = default;
    MutexError lock() noexcept {
        auto& system = System::self();
        const auto current_task_ = system.current_task();

        {
            // ask ONCE at the door: re-locking what I already hold.
            // Inside the loop, owner==me can only mean "handed to me while parked"
            ZerOS::irq::CriticalGuard g{system};
            if (owner_ == current_task_.get()) {
                return MutexError::SelfLocked;
            }
        }

        MutexWaiter me{current_task_.get(), system.prio(current_task_)};

        while (1) {
            ZerOS::irq::CriticalGuard g{system};

            if (owner_ == current_task_.get()) {
                waiters_.remove(&me);  // pop_head already got me; this is belt and braces
                return MutexError::Ok; // handed over while I parked
            }

            if (!owner_) {
                owner_ = current_task_.get();
                waiters_.remove(&me);
                return MutexError::Ok;
            }

            waiters_.insert(&me);

            if (system.prio(owner_) > system.prio(current_task_)) {
                if (!boosted_) {
                    // OK, owners self level
                    saved_prio_ = system.prio(owner_);
                    boosted_ = true;
                }

                system.reprioritize(owner_, system.prio(current_task_));
            }

            system.block(current_task_);
        }
    }
    MutexError unlock() noexcept {
        auto& system = System::self();
        const auto current_task_ = system.current_task();
        ZerOS::irq::CriticalGuard g{system};

        if (owner_ != current_task_.get()) {
            return MutexError::NotYourLock;
        }
        if (boosted_) {
            system.reprioritize(current_task_, saved_prio_);
            boosted_ = false;
        }
        if (auto head = waiters_.pop_head()) {
            owner_ = head->task_;
            if (auto next = waiters_.head(); // Is the new headers request
                next != nullptr && next->prio_ < system.prio(owner_)) {
                saved_prio_ = system.prio(owner_);
                boosted_ = true;
                system.reprioritize(owner_, next->prio_);
            }
            system.ready(head->task_);
        } else {
            owner_ = nullptr;
        }
        return MutexError::Ok;
    }

  private:
    sched::TCB* owner_{};
    sched::TaskPriority_t saved_prio_{}; // saved
    bool boosted_{false};

    struct ByPrio {
        bool operator()(const MutexWaiter& fresh, const MutexWaiter& parked) const noexcept {
            return fresh.prio_ < parked.prio_;
        }
    };
    base::SelfList<MutexWaiter, ByPrio> waiters_{};
};

} // namespace ZerOS::sync