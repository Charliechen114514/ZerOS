#pragma once

#include "ZerOS/kernel/clock/durations.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "ZerOS/kernel/sync/semaphore.hpp"
#include "ZerOS/kernel/system_concept.hpp"
#include <expected>
namespace ZerOS::sync {
enum class QueueError { Ok, Timeout, Full };

// messages cross the ring by memcpy
template <typename M>
concept PlainMessage = std::is_trivially_copyable_v<M>;

template <ZerOS::system::SystemContext System, PlainMessage Message, std::size_t Capacity>
struct QueueBase {
    static constexpr auto capacity = Capacity;
    constexpr QueueBase() = default;

    static_assert((Capacity & (Capacity - 1)) == 0, "power of two: & beats % on M3");

    std::expected<Message, QueueError> receive_one() noexcept {
        semaphore.acquire();
        return pop_ring();
    }
    // when 0, means try receive
    static constexpr auto TRY_RECEIVE_TIME = clock::Milliseconds{0};
    std::expected<Message, QueueError> receive_one(clock::Milliseconds timeout) noexcept {
        if (semaphore.try_acquire(timeout) != SyncError::Ok) {
            return std::unexpected(QueueError::Timeout);
        }
        return pop_ring();
    }
    // Post one message
    QueueError post(const Message& m) {
        auto& sys = System::self();
        ZerOS::irq::CriticalGuard g{sys};
        if (count_ == capacity) {
            return QueueError::Full;
        }
        ring_[(head_ + count_) & (capacity - 1)] = m;
        ++count_;
        semaphore.release(); // the unit is handed out only AFTER the data landed
        return QueueError::Ok;
    }

  private:
    std::expected<Message, QueueError> pop_ring() noexcept {
        auto& system = System::self();
        ZerOS::irq::CriticalGuard g{system};
        Message out = ring_[head_];
        // pop one, we need to move our head!
        head_ = (head_ + 1) & (capacity - 1);
        --count_;
        return out; // explicit make
    }

  private:
    SemaphoreBase<System> semaphore{0}; // zero units: the ring starts empty
    Message ring_[Capacity]{};
    std::size_t head_{};
    std::size_t count_{};
};
} // namespace ZerOS::sync
