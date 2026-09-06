#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>

#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sync/semaphore.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::Kernel;
using ZerOS::clock::Milliseconds;
using ZerOS::clock::TimeWaiter;
using ZerOS::clock::Ticks;
using ZerOS::sched::Scheduler;
using ZerOS::sched::TCB;
using ZerOS::sched::TaskState;
using ZerOS::sync::SemaphoreBase;
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

// The Mock side of the D22 doctrine: satisfies SystemContext, links its own
// state, and — host specialty — turns "sleep" into "run the world's business
// for the sleeping span, then come back". On real silicon that business runs
// while PendSV parked us; here it runs inside the block/sleep_for hook.
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

struct Rig {
    ZerOS::sched::TCBStorage storage{};
    TCB& tcb;

    explicit Rig(ZerOS::sched::TaskPriority_t prio, const char* name)
        : tcb(ZerOS::task::named(name).prio(prio).stack(std::span<std::uint32_t>{})
                          .entry(kNoopEntry, nullptr)
                          .spawn_into(storage)) {}
};

using Sem = SemaphoreBase<FakeSys>;

struct World { // per-case scheduler+clock+mock wiring
    Scheduler<HostSwitchPort> sched;
    Kernel<SleepPort> time;
    Rig task{1, "t"};

    World() {
        FakeSys::sched_ = &sched;
        FakeSys::time_ = &time;
        FakeSys::during_sleep = nullptr;
        g_switch_requests = 0;
        sched.add(&task.tcb);
        REQUIRE(sched.pick_next().get() == &task.tcb); // task is current now
    }
};

} // namespace

TEST_CASE("release first: acquire takes the fast path, nobody sleeps", "[sync]") {
    World w;
    Sem sem{0};

    sem.release();
    CHECK(sem.try_acquire() == SyncError::Ok);
    sem.release();
    CHECK(sem.try_acquire(Milliseconds{7}) == SyncError::Ok); // fast path ignores time
    CHECK(sem.try_acquire() == SyncError::TimedOut);           // drained
}

TEST_CASE("zero-timeout try never sleeps", "[sync]") {
    World w;
    Sem sem{0};

    CHECK(sem.try_acquire(Milliseconds{0}) == SyncError::TimedOut);
    CHECK(ZerOS::sched::zeros_impl::TCBKeys::state(w.task.tcb) == TaskState::Running);
}

TEST_CASE("acquire sleeps, release lands while asleep, wake and take", "[sync]") {
    World w;
    Sem sem{0};

    FakeSys::during_sleep = [&sem] { sem.release(); }; // the world releases while I park
    sem.acquire();                                     // returns only after the world gave a unit
    // host: the task object sits Ready until someone picks it again
    REQUIRE(w.sched.pick_next().get() == &w.task.tcb);
    CHECK(ZerOS::sched::zeros_impl::TCBKeys::state(w.task.tcb) == TaskState::Running);
}

TEST_CASE("timed acquire times out through the alarm", "[sync]") {
    World w;
    Sem sem{0};

    FakeSys::during_sleep = [&w] { w.time.on_elapsed(5); }; // the clock runs out
    CHECK(sem.try_acquire(Milliseconds{5}) == SyncError::TimedOut);
}

TEST_CASE("stale alarm does not corrupt the next timed wait", "[sync]") {
    // the re-arm race: event wins while an alarm is still booked, task wins,
    // immediately waits on ANOTHER timeout — the old alarm node must be
    // forgotten before the new one is inserted (enqueue_locked's remove)
    World w;
    Sem sem{0};

    FakeSys::during_sleep = [&sem] { sem.release(); };
    CHECK(sem.try_acquire(Milliseconds{10}) == SyncError::Ok); // event beat the alarm

    FakeSys::during_sleep = [&w] { w.time.on_elapsed(3); }; // fresh alarm, fresh wait
    CHECK(sem.try_acquire(Milliseconds{3}) == SyncError::TimedOut); // must be sane
}

TEST_CASE("event wins the race against the alarm, verdict comes from the counter", "[sync]") {
    World w;
    Sem sem{0};

    FakeSys::during_sleep = [&sem, &w] {
        sem.release();          // the unit arrives
        w.time.on_elapsed(10);  // ...and the alarm fires right after
    };
    CHECK(sem.try_acquire(Milliseconds{10}) == SyncError::Ok); // counter is the truth
}

TEST_CASE("forever acquire books no alarm at all", "[sync]") {
    World w;
    Sem sem{0};
    int phase = 0;

    FakeSys::during_sleep = [&] {
        if (phase++ == 0) {
            w.time.on_elapsed(100); // a century passes — no alarm was booked,
            // so nothing may have woken me:
            CHECK(ZerOS::sched::zeros_impl::TCBKeys::state(w.task.tcb) == TaskState::Blocked);
        } else {
            sem.release(); // eventually the world hands a unit over
        }
    };
    sem.acquire(); // returns only via the release, never via time
    CHECK(phase == 2);
}
