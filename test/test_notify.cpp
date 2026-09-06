#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>

#include "ZerOS/kernel/clock/durations.hpp"
#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/system_concept.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::Kernel;
using ZerOS::clock::Milliseconds;
using ZerOS::clock::TimeWaiter;
using ZerOS::clock::Ticks;
using ZerOS::sched::Scheduler;
using ZerOS::sched::TCB;
using ZerOS::sync::SyncError;

namespace {

int g_switch_requests = 0;

struct HostSwitchPort {
    void request_switch() { ++g_switch_requests; }
    void lock() {}
    void unlock() {}
};

struct SleepPort {
    void arm(Ticks::tick_t) {}
    void disarm() {}
    void lock() {}
    void unlock() {}
};

// Mock doctrine, mailbox flavor
struct FakeSys {
    static inline Scheduler<HostSwitchPort>* sched_;
    static inline Kernel<SleepPort>* time_;
    static inline std::function<void()> during_sleep = nullptr;

    static FakeSys& self() { return the_one; }
    static FakeSys the_one;

    BorrowedPtr<TCB> current_task() { return sched_->current_task(); }
    void ready(BorrowedPtr<TCB> t) { sched_->ready(t); }
    void block(BorrowedPtr<TCB> t) {
        sched_->block(t);
        if (during_sleep != nullptr) {
            during_sleep();
        }
    }
    void sleep_for(BorrowedPtr<TCB> t, Ticks::tick_t span) {
        constexpr auto wake = +[](BorrowedPtr<TimeWaiter> w) {
            sched_->ready(TCB::owner_of(w));
        };
        sched_->sleep_for(t, *time_, span, wake);
        if (during_sleep != nullptr) {
            during_sleep();
        }
    }
    void yield() { sched_->yield(); }
    Ticks now() { return time_->current(); }
    ZerOS::sched::TaskPriority_t prio(BorrowedPtr<TCB> t) { return sched_->prio(t); }
    void reprioritize(BorrowedPtr<TCB> t, ZerOS::sched::TaskPriority_t p) {
        sched_->reprioritize(t, p);
    }
    void arm_timer(BorrowedPtr<TimeWaiter> w, Ticks::tick_t span) { time_->call_after_span(w, span); }
    void cancel_timer(BorrowedPtr<TimeWaiter> w) { time_->cancel(w); }
    bool notify(BorrowedPtr<TCB> t, std::uint32_t v) { return sched_->notify(t, v); }
    std::expected<std::uint32_t, ZerOS::sync::SyncError> wait_notify(Ticks::tick_t span) {
        // the re-check loop lives HERE, sleeping through this mock's hooks
        const auto ddl = time_->current().tick_ + span;
        for (;;) {
            if (auto letter = sched_->wait_notify(); letter.has_value()) {
                return letter;
            }
            const auto left = static_cast<Ticks::tick_diff_t>(ddl - time_->current().tick_);
            if (left <= 0) {
                return std::unexpected(ZerOS::sync::SyncError::TimedOut);
            }
            sleep_for(current_task(), static_cast<Ticks::tick_t>(left));
        }
    }
    void lock() {}
    void unlock() {}
};

FakeSys FakeSys::the_one{};

static_assert(ZerOS::system::SystemContext<FakeSys>);

constexpr auto kNoopEntry = +[](void*) {};

struct World {
    Scheduler<HostSwitchPort> sched;
    Kernel<SleepPort> time;
    ZerOS::sched::TCBStorage storage{};
    TCB& task;

    World()
        : task(ZerOS::task::named("t")
                   .prio(1)
                   .stack(std::span<std::uint32_t>{})
                   .entry(kNoopEntry, nullptr)
                   .spawn_into(storage)) {
        FakeSys::sched_ = &sched;
        FakeSys::time_ = &time;
        FakeSys::during_sleep = nullptr;
        g_switch_requests = 0;
        sched.add(&task);
        REQUIRE(sched.pick_next().get() == &task);
    }

    std::expected<std::uint32_t, SyncError> wait_notify(Milliseconds t) {
        return FakeSys::self().wait_notify(t.count);
    }
};

} // namespace

TEST_CASE("an empty mailbox try comes back empty-handed", "[notify]") {
    World w;
    auto miss = w.wait_notify(Milliseconds{0});
    REQUIRE_FALSE(miss.has_value());
    CHECK(miss.error() == SyncError::TimedOut);
}

TEST_CASE("taking the letter clears the flag: second read is empty", "[notify]") {
    World w;

    REQUIRE(FakeSys::sched_->notify(&w.task, 42));
    auto got = w.wait_notify(Milliseconds{0});
    REQUIRE(got.has_value());
    CHECK(*got == 42);

    auto again = w.wait_notify(Milliseconds{0}); // the flag went with the letter
    CHECK_FALSE(again.has_value());
}

TEST_CASE("single slot, latest wins: two drops, one read, the NEWEST value", "[notify]") {
    World w;

    REQUIRE(FakeSys::sched_->notify(&w.task, 1));
    REQUIRE(FakeSys::sched_->notify(&w.task, 2)); // overwrites — by design
    auto got = w.wait_notify(Milliseconds{0});
    REQUIRE(got.has_value());
    CHECK(*got == 2);
}

TEST_CASE("a notify while parked lands the letter and wakes the owner", "[notify]") {
    World w;

    FakeSys::during_sleep = [&w] {
        REQUIRE(FakeSys::sched_->notify(&w.task, 7)); // the world drops a letter
    };
    auto got = w.wait_notify(Milliseconds{5});
    REQUIRE(got.has_value());
    CHECK(*got == 7);
}

TEST_CASE("notifying nobody is refused, not ignored silently", "[notify]") {
    World w;
    CHECK_FALSE(FakeSys::sched_->notify({}, 9));
}
