#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <functional>
#include <set>

#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sync/mutex.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::Kernel;
using ZerOS::clock::TimeWaiter;
using ZerOS::clock::Ticks;
using ZerOS::sched::Scheduler;
using ZerOS::sched::TCB;
using ZerOS::sched::TaskPriority_t;
using ZerOS::sched::TaskState;
using ZerOS::sched::zeros_impl::TCBKeys;
using ZerOS::sync::MutexBase;
using ZerOS::sync::MutexError;

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

// Mock side of the D22 doctrine, mutex flavor: prio/reprioritize forward to
// the REAL scheduler (PI surgery gets exercised for real); during_sleep runs
// "the world while I park" — the host trick that makes the re-check loop
// converge without a real context switch.
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
    TaskPriority_t prio(BorrowedPtr<TCB> t) { return sched_->prio(t); }
    void reprioritize(BorrowedPtr<TCB> t, TaskPriority_t p) { sched_->reprioritize(t, p); }
    void arm_timer(BorrowedPtr<TimeWaiter> w, Ticks::tick_t span) { time_->call_after_span(w, span); }
    void cancel_timer(BorrowedPtr<TimeWaiter> w) { time_->cancel(w); }
    void lock() {}
    void unlock() {}
};

FakeSys FakeSys::the_one{};

static_assert(ZerOS::system::SystemContext<FakeSys>);

constexpr auto kNoopEntry = +[](void*) {};

struct Rig {
    ZerOS::sched::TCBStorage storage{};
    TCB& tcb;

    explicit Rig(TaskPriority_t prio, const char* name)
        : tcb(ZerOS::task::named(name).prio(prio).stack(std::span<std::uint32_t>{})
                          .entry(kNoopEntry, nullptr)
                          .spawn_into(storage)) {}
};

using Mtx = MutexBase<FakeSys>;

struct World {
    Scheduler<HostSwitchPort> sched;
    Kernel<SleepPort> time;
    Rig low{2, "low"};
    Rig mid{3, "mid"};
    Rig high{0, "high"};

    // stage model: ONE actor on stage at a time. block() is only complete
    // for the current task (it does NOT detach a ring-parked task), so the
    // invariant here is: whoever is not current is either off-stage (never
    // added) or Blocked — never silently sitting in the ready ring.
    std::set<TCB*> on_stage;

    World() {
        FakeSys::sched_ = &sched;
        FakeSys::time_ = &time;
        FakeSys::during_sleep = nullptr;
        g_switch_requests = 0;
    }

    // make `who` current: the previous actor steps off (block), `who` enters
    // (first time: add) or returns (ready)
    void become(TCB& who) {
        if (sched.current_task().get() == &who) {
            return;
        }
        if (auto cur = sched.current_task(); cur) {
            sched.block(cur); // current is off the ring already — safe
        }
        if (on_stage.insert(&who).second) {
            sched.add(&who);
        } else if (TCBKeys::state(who) == TaskState::Blocked) {
            sched.ready(&who);
        }
        REQUIRE(sched.pick_next().get() == &who);
    }
};

} // namespace

TEST_CASE("plain lock/unlock round trip", "[mutex]") {
    World w;
    Mtx m;
    w.become(w.low.tcb);

    CHECK(m.lock() == MutexError::Ok);
    CHECK(m.unlock() == MutexError::Ok);
}

TEST_CASE("self-lock and foreign-unlock are refused loudly", "[mutex]") {
    World w;
    Mtx m;
    w.become(w.low.tcb);

    CHECK(m.lock() == MutexError::Ok);
    CHECK(m.lock() == MutexError::SelfLocked); // would deadlock silently otherwise

    w.become(w.high.tcb); // someone else touches the lock while low OWNS it
    CHECK(m.unlock() == MutexError::NotYourLock);

    w.become(w.low.tcb); // the real owner settles the bill
    CHECK(m.unlock() == MutexError::Ok);
    CHECK(m.unlock() == MutexError::NotYourLock); // double-unlock is also refused
}

// The full PI story on one waiter: holder parks at low level, an urgent task
// comes for the lock → holder runs BOOSTED → holder unlocks → holder back at
// its own level, lock HANDED to the urgent one.
TEST_CASE("boost, hand-over and restore: the whole inheritance arc", "[mutex][pi]") {
    World w;
    Mtx m;

    w.become(w.low.tcb); // low (2) takes the lock...
    REQUIRE(m.lock() == MutexError::Ok);

    w.become(w.high.tcb); // ...then high (0) comes for it
    FakeSys::during_sleep = [&] {
        CHECK(TCBKeys::prio(w.low.tcb) == 0);  // holder wears my level now
        w.become(w.low.tcb);                   // world: the holder runs...
        CHECK(m.unlock() == MutexError::Ok);   // ...releases (restore + hand over)
        w.become(w.high.tcb);                  // ...and the waiter takes the CPU back
    };
    REQUIRE(m.lock() == MutexError::Ok); // handed to me while parked

    CHECK(TCBKeys::prio(w.low.tcb) == 2); // restored, not left boosted
    CHECK(m.unlock() == MutexError::Ok);  // and yes, the lock is mine now
}

// Multi-waiter urgency ordering is left to the Renode demo (real two-task
// contention cannot park two waiters on a single-threaded Mock).

TEST_CASE("reprioritize keeps the ready ring honest in all three shapes", "[mutex][pi]") {
    World w;
    // this case drives the scheduler directly: two actors on stage at once
    w.sched.add(&w.mid.tcb);
    w.sched.add(&w.high.tcb);

    // shape 1: parked in the old level's ring → detaches, reattaches at the new
    REQUIRE(w.sched.pick_next().get() == &w.high.tcb); // high running, mid parked

    w.sched.reprioritize(&w.mid.tcb, 0); // lift the parked one to the very top
    w.sched.block(&w.high.tcb);
    REQUIRE(w.sched.pick_next().get() == &w.mid.tcb); // it must preempt now

    // shape 2: running task — number changes, no ring to fix
    w.sched.reprioritize(&w.mid.tcb, 5);
    CHECK(TCBKeys::prio(w.mid.tcb) == 5);

    // shape 3: blocked task — number changes, ready() honors it later
    w.sched.block(&w.high.tcb);
    w.sched.reprioritize(&w.high.tcb, 1);
    w.sched.ready(&w.high.tcb);
    w.sched.block(&w.mid.tcb);
    REQUIRE(w.sched.pick_next().get() == &w.high.tcb); // 1 beats 5
}
