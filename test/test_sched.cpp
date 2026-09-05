#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::TimeWaiter;
using ZerOS::sched::IsScheduler;
using ZerOS::sched::Scheduler;
using ZerOS::sched::Switchable;
using ZerOS::sched::TCB;
using ZerOS::sched::TaskPriority_t;
using ZerOS::sched::TaskState;

namespace {

int g_switch_requests = 0;

struct HostSwitchPort {
    void request_switch() { ++g_switch_requests; }
};

struct SleepPort {
    void arm(ZerOS::clock::Ticks::tick_t) {}
    void disarm() {}
    void lock() {}
    void unlock() {}
};

static_assert(Switchable<HostSwitchPort>);
static_assert(IsScheduler<Scheduler<HostSwitchPort>>);
static_assert(ZerOS::clock::TimePortable<SleepPort>);

Scheduler<HostSwitchPort>* g_sched = nullptr;

TCB task(TaskPriority_t prio, const char* name) {
    TCB t;
    t.task_priority_ = prio;
    t.name_ = name;
    return t;
}

int switches_after(int before) {
    return g_switch_requests - before;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
TCB* owner_of(BorrowedPtr<TimeWaiter> w) {
    return reinterpret_cast<TCB*>(reinterpret_cast<std::byte*>(w.get()) - offsetof(TCB, action));
}
#pragma GCC diagnostic pop

} // namespace

TEST_CASE("pick_next takes the highest priority; add marks Ready", "[sched]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    TCB a = task(1, "a"), b = task(3, "b"), c = task(0, "c");

    s.add(&a);
    s.add(&b);
    s.add(&c);
    CHECK(a.state_ == TaskState::Ready);
    CHECK(b.state_ == TaskState::Ready);
    CHECK(c.state_ == TaskState::Ready);

    auto t1 = s.pick_next();
    REQUIRE(t1.get() == &c);
    CHECK(c.state_ == TaskState::Running);
    CHECK(s.current_task().get() == &c);

    s.block(&c);
    auto t2 = s.pick_next();
    REQUIRE(t2.get() == &a);
}

TEST_CASE("same level round-robins via yield-to-tail", "[sched]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    TCB a = task(1, "a"), b = task(1, "b"), d = task(1, "d");

    s.add(&a);
    s.add(&b);
    s.add(&d);

    REQUIRE(s.pick_next().get() == &a);

    s.yield();
    REQUIRE(s.pick_next().get() == &b);

    s.yield();
    REQUIRE(s.pick_next().get() == &d);

    s.yield();
    REQUIRE(s.pick_next().get() == &a);
}

TEST_CASE("preempted task resumes from the head of its level", "[sched]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    TCB a = task(1, "a"), c = task(0, "c");

    s.add(&a);
    REQUIRE(s.pick_next().get() == &a);

    auto before = g_switch_requests;
    c.state_ = TaskState::Blocked;
    s.ready(&c);
    CHECK(switches_after(before) == 1);

    REQUIRE(s.pick_next().get() == &c);
    CHECK(a.state_ == TaskState::Ready);

    s.block(&c);
    REQUIRE(s.pick_next().get() == &a);
    CHECK(a.state_ == TaskState::Running);
}

TEST_CASE("same-level wake joins the tail, no switch pended", "[sched]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    TCB a = task(0, "a"), b = task(0, "b");

    s.add(&a);
    REQUIRE(s.pick_next().get() == &a);

    auto before = g_switch_requests;
    b.state_ = TaskState::Blocked;
    s.ready(&b);
    CHECK(switches_after(before) == 0);

    s.block(&a);
    REQUIRE(s.pick_next().get() == &b);
}

TEST_CASE("idle takes over when everything is blocked", "[sched]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    TCB a = task(1, "a");

    s.add(&a);
    REQUIRE(s.pick_next().get() == &a);
    s.block(&a);

    auto idle = s.pick_next();
    REQUIRE(idle.get() != nullptr);
    CHECK(idle->name_ != nullptr);
    CHECK(s.current_task().get() == idle.get());

    auto again = s.pick_next();
    CHECK(again.get() == idle.get());

    s.ready(&a);
    REQUIRE(s.pick_next().get() == &a);
}

TEST_CASE("block pends a switch only for the running task", "[sched]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    TCB a = task(1, "a"), b = task(2, "b");

    s.add(&a);
    s.add(&b);
    REQUIRE(s.pick_next().get() == &a);

    auto before = g_switch_requests;
    s.block(&b);
    CHECK(switches_after(before) == 0);
    CHECK(b.state_ == TaskState::Blocked);

    s.block(&a);
    CHECK(switches_after(before) == 1);
}

TEST_CASE("ready of a lower priority task does not pend a switch", "[sched]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    TCB a = task(0, "a"), b = task(3, "b");

    s.add(&a);
    REQUIRE(s.pick_next().get() == &a);

    auto before = g_switch_requests;
    b.state_ = TaskState::Blocked;
    s.ready(&b);
    CHECK(switches_after(before) == 0);

    s.block(&a);
    REQUIRE(s.pick_next().get() == &b);
}

TEST_CASE("a lone task yielding round-robins with itself", "[sched]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    TCB a = task(5, "a");

    s.add(&a);
    REQUIRE(s.pick_next().get() == &a);

    s.yield();
    REQUIRE(s.pick_next().get() == &a);
    CHECK(a.state_ == TaskState::Running);
}

TEST_CASE("sleep via TimeKernel wakes the TCB back into Ready", "[sched][time]") {
    g_switch_requests = 0;
    Scheduler<HostSwitchPort> s;
    g_sched = &s;

    TCB a = task(1, "a");
    a.action.OnTime = +[](BorrowedPtr<TimeWaiter> w) { g_sched->ready(owner_of(w)); };

    s.add(&a);
    REQUIRE(s.pick_next().get() == &a);

    ZerOS::clock::Kernel<SleepPort> tk;
    s.block(&a);
    tk.call_after_span(&a.action, 5);

    tk.on_elapsed(4);
    CHECK(a.state_ == TaskState::Blocked);

    tk.on_elapsed(1);
    CHECK(a.state_ == TaskState::Ready);

    auto before = g_switch_requests;
    REQUIRE(s.pick_next().get() == &a);
    CHECK(a.state_ == TaskState::Running);
    CHECK(switches_after(before) == 0);
}
