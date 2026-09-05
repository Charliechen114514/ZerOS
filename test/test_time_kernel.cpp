#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "ZerOS/kernel/clock/kernel.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::Kernel;
using ZerOS::clock::Ticks;
using ZerOS::clock::TimeWaiter;

namespace {

std::vector<Ticks::tick_t> g_arms;
int g_disarms = 0;
std::vector<const TimeWaiter*> g_fired_nodes;

struct HostTimePort {
    void arm(Ticks::tick_t d) { g_arms.push_back(d); }
    void disarm() { ++g_disarms; }
    void lock() {}
    void unlock() {}
};

static_assert(ZerOS::clock::TimePortable<HostTimePort>);

constexpr auto recorder = +[](BorrowedPtr<TimeWaiter> w) { g_fired_nodes.push_back(w.get()); };

struct Waiter {
    TimeWaiter node;

    Waiter() : node(Ticks{0}, recorder) {}
};

void reset_logs() {
    g_arms.clear();
    g_disarms = 0;
    g_fired_nodes.clear();
}

std::vector<std::uint32_t> fired_deadlines() {
    std::vector<std::uint32_t> out;
    for (auto* n : g_fired_nodes) {
        out.push_back(n->deadline_.tick_);
    }
    return out;
}

} // namespace

TEST_CASE("fires in deadline order", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a, b, c;
    k.call_after_span(&a.node, 3);
    k.call_after_span(&b.node, 1);
    k.call_after_span(&c.node, 2);

    k.on_elapsed(1);
    CHECK(fired_deadlines() == std::vector<std::uint32_t>{1});

    k.on_elapsed(2);
    CHECK(fired_deadlines() == std::vector<std::uint32_t>{1, 2, 3});
}

TEST_CASE("exact boundary fires, one tick early does not", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a;
    k.call_after_span(&a.node, 5);

    k.on_elapsed(4);
    CHECK(g_fired_nodes.empty());

    k.on_elapsed(1);
    CHECK(fired_deadlines() == std::vector<std::uint32_t>{5});
}

TEST_CASE("single wake: cancel before due prevents fire", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a;
    k.call_after_span(&a.node, 10);

    k.on_elapsed(5);
    CHECK(k.cancel(&a.node));

    k.on_elapsed(100);
    CHECK(g_fired_nodes.empty());
}

TEST_CASE("single wake: cancel after fire reports late", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a;
    k.call_after_span(&a.node, 10);

    k.on_elapsed(10);
    CHECK_FALSE(k.cancel(&a.node));
    CHECK(fired_deadlines() == std::vector<std::uint32_t>{10});
}

TEST_CASE("greedy arm: only a strictly earlier head re-arms", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a, b, c;
    k.call_after_span(&a.node, 100);
    REQUIRE(g_arms.back() == 100);

    k.call_after_span(&b.node, 50);
    REQUIRE(g_arms.back() == 50);

    auto arms_before = g_arms.size();
    k.call_after_span(&c.node, 70);
    CHECK(g_arms.size() == arms_before);

    CHECK(k.cancel(&b.node));
    CHECK(g_arms.size() == arms_before);
}

TEST_CASE("consumed alarm re-arms for the next head", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a, b;
    k.call_after_span(&a.node, 50);
    k.call_after_span(&b.node, 70);

    REQUIRE(g_arms.size() == 1);

    k.on_elapsed(50);
    CHECK(fired_deadlines() == std::vector<std::uint32_t>{50});
    CHECK(g_arms.size() == 2);
    CHECK(g_arms.back() == 20);
}

TEST_CASE("drain to empty disarms exactly once", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a;
    k.call_after_span(&a.node, 10);
    REQUIRE(g_disarms == 0);

    CHECK(k.cancel(&a.node));
    CHECK(g_disarms == 1);

    CHECK_FALSE(k.cancel(&a.node));
    CHECK(g_disarms == 1);
}

TEST_CASE("same deadline fires in insertion order", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a, b, c;
    k.call_after_span(&a.node, 7);
    k.call_after_span(&b.node, 7);
    k.call_after_span(&c.node, 7);

    k.on_elapsed(7);
    CHECK(g_fired_nodes == std::vector<const TimeWaiter*>{&a.node, &b.node, &c.node});
}

TEST_CASE("on_elapsed(0) is a no-op", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    Waiter a;
    k.call_after_span(&a.node, 5);

    k.on_elapsed(0);
    CHECK(g_fired_nodes.empty());
    CHECK(k.current().tick_ == 0);
}

TEST_CASE("wraparound across the rollover", "[timekernel]") {
    reset_logs();
    Kernel<HostTimePort> k;
    k.on_elapsed(0xFFFFFF00u);

    Waiter a;
    k.call_after_span(&a.node, 0x80u);

    k.on_elapsed(0x7Fu);
    CHECK(g_fired_nodes.empty());

    k.on_elapsed(0x01u);
    CHECK(fired_deadlines() == std::vector<std::uint32_t>{0xFFFFFF80u});
}
