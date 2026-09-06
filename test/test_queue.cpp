#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>

#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sync/msg_queue.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::Kernel;
using ZerOS::clock::Milliseconds;
using ZerOS::clock::TimeWaiter;
using ZerOS::clock::Ticks;
using ZerOS::sched::Scheduler;
using ZerOS::sched::TCB;
using ZerOS::sched::TaskState;
using ZerOS::sync::QueueBase;
using ZerOS::sync::QueueError;

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

// same Mock doctrine as test_sync/test_mutex: during_sleep runs the world
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
        REQUIRE(sched.pick_next().get() == &task); // task is current now
    }
};

using Q = QueueBase<FakeSys, char, 4>;

} // namespace

TEST_CASE("empty queue: try times out, nothing sleeps forever", "[queue]") {
    World w;
    Q q;

    auto miss = q.receive_one(Milliseconds{0});
    REQUIRE_FALSE(miss.has_value());
    CHECK(miss.error() == QueueError::Timeout);
}

TEST_CASE("post/receive keep FIFO order", "[queue]") {
    World w;
    Q q;

    REQUIRE(q.post('a') == QueueError::Ok);
    REQUIRE(q.post('b') == QueueError::Ok);
    auto first = q.receive_one(Milliseconds{0});
    auto second = q.receive_one(Milliseconds{0});
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first == 'a');
    CHECK(*second == 'b');
}

TEST_CASE("full ring is refused with Full, nothing is overwritten", "[queue]") {
    World w;
    Q q;

    for (char c : {'1', '2', '3', '4'}) {
        REQUIRE(q.post(c) == QueueError::Ok);
    }
    CHECK(q.post('5') == QueueError::Full); // the fifth knocks, ring says no

    // the four that made it in are still the ORIGINAL four, in order:
    for (char c : {'1', '2', '3', '4'}) {
        auto got = q.receive_one(Milliseconds{0});
        REQUIRE(got.has_value());
        CHECK(*got == c);
    }
}

TEST_CASE("head wraps around the ring without losing anything", "[queue]") {
    World w;
    Q q;

    for (char c : {'1', '2', '3', '4'}) {
        REQUIRE(q.post(c) == QueueError::Ok);
    }
    REQUIRE(*q.receive_one(Milliseconds{0}) == '1'); // free two slots
    REQUIRE(*q.receive_one(Milliseconds{0}) == '2');
    REQUIRE(q.post('5') == QueueError::Ok); // these land across the seam
    REQUIRE(q.post('6') == QueueError::Ok);

    for (char c : {'3', '4', '5', '6'}) { // and come out in order anyway
        REQUIRE(*q.receive_one(Milliseconds{0}) == c);
    }
    CHECK(q.receive_one(Milliseconds{0}).error() == QueueError::Timeout); // drained
}

TEST_CASE("a burst of bytes survives intact — no slot sharing", "[queue]") {
    World w;
    Q q;

    for (char c : {'x', 'y', 'z'}) {
        REQUIRE(q.post(c) == QueueError::Ok);
    }
    for (char c : {'x', 'y', 'z'}) {
        auto got = q.receive_one(Milliseconds{0});
        REQUIRE(got.has_value());
        CHECK(*got == c);
    }
}

TEST_CASE("timed receive wakes up when the world delivers while parked", "[queue]") {
    World w;
    Q q;

    FakeSys::during_sleep = [&q] { q.post('!'); }; // the world delivers on time
    auto got = q.receive_one(Milliseconds{5});
    REQUIRE(got.has_value());
    CHECK(*got == '!');
}

TEST_CASE("forever receive returns only when handed a message", "[queue]") {
    World w;
    Q q;

    FakeSys::during_sleep = [&q] { q.post('k'); };
    auto got = q.receive_one();
    REQUIRE(got.has_value());
    CHECK(*got == 'k');
}

TEST_CASE("post on a full ring stays non-blocking and returns Full", "[queue]") {
    World w;
    Q q;

    for (char c : {'1', '2', '3', '4'}) {
        REQUIRE(q.post(c) == QueueError::Ok);
    }
    CHECK(q.post('5') == QueueError::Full); // the ISR-grade behavior, unchanged
}

TEST_CASE("post_for on a full ring times out honestly", "[queue]") {
    World w;
    Q q;

    for (char c : {'1', '2', '3', '4'}) {
        REQUIRE(q.post(c) == QueueError::Ok);
    }
    FakeSys::during_sleep = [&w] { w.time.on_elapsed(5); }; // time runs out, no room
    CHECK(q.post_for('5', Milliseconds{5}) == QueueError::Timeout);
}

TEST_CASE("post_for sleeps until a receive frees a slot (back pressure)", "[queue]") {
    World w;
    Q q;

    for (char c : {'1', '2', '3', '4'}) {
        REQUIRE(q.post(c) == QueueError::Ok);
    }
    FakeSys::during_sleep = [&q] {
        // the world drains one message while the sender sleeps — a slot frees
        auto got = q.receive_one(Milliseconds{0});
        REQUIRE(got.has_value());
        CHECK(*got == '1');
    };
    CHECK(q.post_for('5', Milliseconds{50}) == QueueError::Ok);

    // the ring now holds 2,3,4,5 in order — the sleeper kept its place:
    for (char c : {'2', '3', '4', '5'}) {
        auto got = q.receive_one(Milliseconds{0});
        REQUIRE(got.has_value());
        CHECK(*got == c);
    }
}
