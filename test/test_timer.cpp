#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>

#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/clock/timer.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include "ZerOS/kernel/sched/task.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::Kernel;
using ZerOS::clock::Milliseconds;
using ZerOS::clock::TimeWaiter;
using ZerOS::clock::Ticks;
using ZerOS::clock::TimerBase;
using ZerOS::sched::Scheduler;
using ZerOS::sched::TCB;

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

// Mock doctrine, timer flavor: a timer never touches tasks, so no stage
// model here — just bind the clock and drive virtual time by hand
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
    void lock() {}
    void unlock() {}
};

FakeSys FakeSys::the_one{};

static_assert(ZerOS::system::SystemContext<FakeSys>);

int g_fires = 0;

struct World {
    Scheduler<HostSwitchPort> sched;
    Kernel<SleepPort> time;

    World() {
        FakeSys::sched_ = &sched;
        FakeSys::time_ = &time;
        FakeSys::during_sleep = nullptr;
        g_switch_requests = 0;
        g_fires = 0;
    }
};

using Tmr = TimerBase<FakeSys>;

constexpr auto kCounting = +[](void*) { ++g_fires; };

} // namespace

TEST_CASE("oneshot fires exactly on time, not a tick earlier", "[timer]") {
    World w;
    Tmr t;

    t.oneshot(Milliseconds{5}, kCounting, nullptr);
    w.time.on_elapsed(4);
    CHECK(g_fires == 0); // one tick early: silence
    w.time.on_elapsed(1);
    CHECK(g_fires == 1); // right on the deadline
}

TEST_CASE("a fired oneshot goes cold — time keeps running, it does not", "[timer]") {
    World w;
    Tmr t;

    t.oneshot(Milliseconds{3}, kCounting, nullptr);
    w.time.on_elapsed(3);
    w.time.on_elapsed(100); // a century later
    CHECK(g_fires == 1);
}

TEST_CASE("periodic keeps firing every period", "[timer]") {
    World w;
    Tmr t;

    t.periodic(Milliseconds{7}, kCounting, nullptr);
    for (int round = 1; round <= 3; ++round) {
        w.time.on_elapsed(7);
        CHECK(g_fires == round);
    }
}

TEST_CASE("stop strikes the booking off the ledger", "[timer]") {
    World w;
    Tmr t;

    t.periodic(Milliseconds{5}, kCounting, nullptr);
    t.stop();
    w.time.on_elapsed(50); // nothing booked, nothing fires
    CHECK(g_fires == 0);
}

TEST_CASE("a periodic may stop ITSELF from inside its own fn", "[timer]") {
    World w;
    Tmr t;

    constexpr auto self_destruct = +[](void* self) {
        ++g_fires;
        static_cast<Tmr*>(self)->stop(); // suicide note, honored
    };
    t.periodic(Milliseconds{4}, self_destruct, &t);

    w.time.on_elapsed(4);
    CHECK(g_fires == 1);
    w.time.on_elapsed(40); // dead by its own hand: no further fires
    CHECK(g_fires == 1);
}

TEST_CASE("a cold oneshot can be armed again", "[timer]") {
    World w;
    Tmr t;

    t.oneshot(Milliseconds{2}, kCounting, nullptr);
    w.time.on_elapsed(2);
    CHECK(g_fires == 1);

    t.oneshot(Milliseconds{9}, kCounting, nullptr); // second life
    w.time.on_elapsed(9);
    CHECK(g_fires == 2);
}
