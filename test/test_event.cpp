#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>

#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sync/event_group.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::Kernel;
using ZerOS::clock::Milliseconds;
using ZerOS::clock::TimeWaiter;
using ZerOS::clock::Ticks;
using ZerOS::sched::Scheduler;
using ZerOS::sched::TCB;
using ZerOS::sync::EventBase;
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

// Mock doctrine, event flavor: during_sleep runs "the world" (set bits,
// run the clock) while the waiting task is parked
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
        // re-check loop lives HERE: the sleep below runs through this mock's
        // own hooks (during_sleep), which a kernel-side loop would bypass
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
};

using Evt = EventBase<FakeSys>;

} // namespace

TEST_CASE("a try on dark bits times out; any lit bit satisfies OR", "[event]") {
    World w;
    Evt e;

    auto dark = e.wait(0b0011, false, Milliseconds{0});
    REQUIRE_FALSE(dark.has_value());
    CHECK(dark.error() == SyncError::TimedOut);

    e.set(0b0001);
    auto lit = e.wait(0b0011, false, Milliseconds{0});
    REQUIRE(lit.has_value());
    CHECK(*lit == 0b0001);
}

TEST_CASE("AND stays asleep until the WHOLE mask is lit", "[event]") {
    World w;
    Evt e;

    e.set(0b0001); // half of the mask — not enough for all_of
    auto early = e.wait(0b0011, true, Milliseconds{0});
    CHECK_FALSE(early.has_value());

    e.set(0b0010); // ...and now the pair is complete
    auto full = e.wait(0b0011, true, Milliseconds{0});
    REQUIRE(full.has_value());
    CHECK(*full == 0b0011);
}

TEST_CASE("a timed wait reports an honest timeout", "[event]") {
    World w;
    Evt e;

    FakeSys::during_sleep = [&w] { w.time.on_elapsed(5); }; // time runs out, no bit
    CHECK_FALSE(e.wait(0b0100, false, Milliseconds{5}).has_value());
}

TEST_CASE("a set() while parked broadcasts the waiter awake", "[event]") {
    World w;
    Evt e;

    FakeSys::during_sleep = [&e] { e.set(0b0100); }; // the world lights it on time
    auto got = e.wait(0b0100, false, Milliseconds{5});
    REQUIRE(got.has_value());
    CHECK(*got == 0b0100);
}

TEST_CASE("clear() turns the light off again", "[event]") {
    World w;
    Evt e;

    e.set(0b1000);
    e.clear(0b1000);
    CHECK_FALSE(e.wait(0b1000, false, Milliseconds{0}).has_value());
}

TEST_CASE("events are states, not consumptions: same bit waits twice", "[event]") {
    World w;
    Evt e;

    e.set(0b0101);
    auto first = e.wait(0b0101, true, Milliseconds{0});
    auto second = e.wait(0b0101, true, Milliseconds{0}); // nobody set anything again
    REQUIRE(first.has_value());
    REQUIRE(second.has_value()); // still lit — no take-one-lose-one here
    CHECK(*first == 0b0101);
    CHECK(*second == 0b0101);
}

TEST_CASE("forever wait returns only when the bit lights up", "[event]") {
    World w;
    Evt e;

    FakeSys::during_sleep = [&e] { e.set(0b1000); };
    auto got = e.wait(0b1000, false);
    REQUIRE(got.has_value());
    CHECK(*got == 0b1000);
}

// Multi-waiter broadcast (two tasks waking from ONE set) cannot park two
// waiters on this single-threaded Mock — left to the Renode demo / silicon.
