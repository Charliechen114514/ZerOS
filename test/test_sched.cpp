#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <span>

#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include "ZerOS/kernel/sched/task.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::TimeWaiter;
using ZerOS::sched::TCB;
using ZerOS::sched::TaskPriority_t;
using ZerOS::sched::TaskState;
using ZerOS::task::named;

namespace {

using ZerOS::sched::zeros_impl::TCBKeys; // whitebox key, tests only

int g_switch_requests = 0;

struct HostSwitchPort {
    void request_switch() { ++g_switch_requests; }
    void lock() {}
    void unlock() {}
};

struct SleepPort {
    void arm(ZerOS::clock::Ticks::tick_t) {}
    void disarm() {}
    void lock() {}
    void unlock() {}
};

static_assert(ZerOS::sched::Switchable<HostSwitchPort>);
static_assert(ZerOS::sched::IsScheduler<ZerOS::sched::Scheduler<HostSwitchPort>>);
static_assert(ZerOS::clock::TimePortable<SleepPort>);

ZerOS::sched::Scheduler<HostSwitchPort>* g_sched = nullptr;

constexpr auto kNoopEntry = +[](void*) {};

// tasks are built the legal way: the same factory road the app walks
struct Rig {
    ZerOS::sched::TCBStorage storage{};
    TCB& tcb;

    explicit Rig(TaskPriority_t prio, const char* name)
        : tcb(named(name).prio(prio).stack(std::span<std::uint32_t>{})
                          .entry(kNoopEntry, nullptr)
                          .spawn_into(storage)) {}
};

constexpr auto kWake = +[](BorrowedPtr<TimeWaiter> w) {
    g_sched->ready(TCB::owner_of(w));
};

int switches_after(int before) {
    return g_switch_requests - before;
}

} // namespace

TEST_CASE("pick_next takes the highest priority; add marks Ready", "[sched]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    Rig a(1, "a"), b(3, "b"), c(0, "c");

    s.add(&a.tcb);
    s.add(&b.tcb);
    s.add(&c.tcb);
    CHECK(TCBKeys::state(a.tcb) == TaskState::Ready);
    CHECK(TCBKeys::state(b.tcb) == TaskState::Ready);
    CHECK(TCBKeys::state(c.tcb) == TaskState::Ready);

    auto t1 = s.pick_next();
    REQUIRE(t1.get() == &c.tcb);
    CHECK(TCBKeys::state(c.tcb) == TaskState::Running);
    CHECK(s.current_task().get() == &c.tcb);

    s.block(&c.tcb);
    auto t2 = s.pick_next();
    REQUIRE(t2.get() == &a.tcb);
}

TEST_CASE("same level round-robins via yield-to-tail", "[sched]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    Rig a(1, "a"), b(1, "b"), d(1, "d");

    s.add(&a.tcb);
    s.add(&b.tcb);
    s.add(&d.tcb);

    REQUIRE(s.pick_next().get() == &a.tcb);

    s.yield();
    REQUIRE(s.pick_next().get() == &b.tcb);

    s.yield();
    REQUIRE(s.pick_next().get() == &d.tcb);

    s.yield();
    REQUIRE(s.pick_next().get() == &a.tcb);
}

TEST_CASE("preempted task resumes from the head of its level", "[sched]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    Rig a(1, "a"), c(0, "c");

    s.add(&a.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);

    auto before = g_switch_requests;
    TCBKeys::set_state(c.tcb, TaskState::Blocked);
    s.ready(&c.tcb);
    CHECK(switches_after(before) == 1);

    REQUIRE(s.pick_next().get() == &c.tcb);
    CHECK(TCBKeys::state(a.tcb) == TaskState::Ready);

    s.block(&c.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);
    CHECK(TCBKeys::state(a.tcb) == TaskState::Running);
}

TEST_CASE("same-level wake joins the tail, no switch pended", "[sched]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    Rig a(0, "a"), b(0, "b");

    s.add(&a.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);

    auto before = g_switch_requests;
    TCBKeys::set_state(b.tcb, TaskState::Blocked);
    s.ready(&b.tcb);
    CHECK(switches_after(before) == 0);

    s.block(&a.tcb);
    REQUIRE(s.pick_next().get() == &b.tcb);
}

TEST_CASE("idle takes over when everything is blocked", "[sched]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    Rig a(1, "a");

    s.add(&a.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);
    s.block(&a.tcb);

    auto idle = s.pick_next();
    REQUIRE(idle.get() != nullptr);
    CHECK(TCBKeys::name(*idle) != nullptr);
    CHECK(s.current_task().get() == idle.get());

    auto again = s.pick_next();
    CHECK(again.get() == idle.get());

    s.ready(&a.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);
}

TEST_CASE("block pends a switch only for the running task", "[sched]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    Rig a(1, "a"), b(2, "b");

    s.add(&a.tcb);
    s.add(&b.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);

    auto before = g_switch_requests;
    s.block(&b.tcb);
    CHECK(switches_after(before) == 0);
    CHECK(TCBKeys::state(b.tcb) == TaskState::Blocked);

    s.block(&a.tcb);
    CHECK(switches_after(before) == 1);
}

TEST_CASE("ready of a lower priority task does not pend a switch", "[sched]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    Rig a(0, "a"), b(3, "b");

    s.add(&a.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);

    auto before = g_switch_requests;
    TCBKeys::set_state(b.tcb, TaskState::Blocked);
    s.ready(&b.tcb);
    CHECK(switches_after(before) == 0);

    s.block(&a.tcb);
    REQUIRE(s.pick_next().get() == &b.tcb);
}

TEST_CASE("a lone task yielding round-robins with itself", "[sched]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    Rig a(5, "a");

    s.add(&a.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);

    s.yield();
    REQUIRE(s.pick_next().get() == &a.tcb);
    CHECK(TCBKeys::state(a.tcb) == TaskState::Running);
}

TEST_CASE("sleep parks atomically and wakes back into Ready", "[sched][time]") {
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    g_sched = &s;

    Rig a(1, "a");
    s.add(&a.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);

    ZerOS::clock::Kernel<SleepPort> tk;
    s.sleep_for(&a.tcb, tk, 5, kWake);

    tk.on_elapsed(4);
    CHECK(TCBKeys::state(a.tcb) == TaskState::Blocked);

    tk.on_elapsed(1);
    CHECK(TCBKeys::state(a.tcb) == TaskState::Ready);

    auto before = g_switch_requests;
    REQUIRE(s.pick_next().get() == &a.tcb);
    CHECK(TCBKeys::state(a.tcb) == TaskState::Running);
    CHECK(switches_after(before) == 0);
}

TEST_CASE("a zero-span sleep still wakes: no gap between arm and block", "[sched][time]") {
    // regression: hand-rolled sleep (arm, then block) loses a wake that
    // fires in between and the task sleeps forever. The compound must
    // not have that gap, not even for span 0.
    g_switch_requests = 0;
    ZerOS::sched::Scheduler<HostSwitchPort> s;
    g_sched = &s;

    Rig a(1, "a");
    s.add(&a.tcb);
    REQUIRE(s.pick_next().get() == &a.tcb);

    ZerOS::clock::Kernel<SleepPort> tk;
    s.sleep_for(&a.tcb, tk, 0, kWake);

    tk.on_elapsed(1);
    CHECK(TCBKeys::state(a.tcb) == TaskState::Ready);

    REQUIRE(s.pick_next().get() == &a.tcb);
}
