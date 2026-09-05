#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/clock/timequeue.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::clock::Ticks;
using ZerOS::clock::TimeWaiter;
using ZerOS::clock::TimerQueue;

namespace {

constexpr auto kNop = +[](BorrowedPtr<TimeWaiter>) {};

struct Waiter {
    TimeWaiter node;

    explicit Waiter(std::uint32_t deadline) : node(Ticks{deadline}, kNop) {}
};

std::vector<std::uint32_t> drain_deadlines(TimerQueue& q) {
    std::vector<std::uint32_t> out;
    while (auto h = q.head()) {
        out.push_back(h->deadline_.tick_);
        static_cast<void>(q.remove(h));
    }
    return out;
}

} // namespace

TEST_CASE("deadlines sort ascending regardless of arrival order", "[timequeue]") {
    TimerQueue q;
    Waiter a(100), b(10), c(70), d(40);

    q.insert(&a.node);
    q.insert(&b.node);
    q.insert(&c.node);
    q.insert(&d.node);

    CHECK(drain_deadlines(q) == std::vector<std::uint32_t>{10, 40, 70, 100});
}

TEST_CASE("equal deadlines keep insertion order (FIFO)", "[timequeue]") {
    TimerQueue q;
    Waiter a(50), b(50), c(50);

    q.insert(&a.node);
    q.insert(&b.node);
    q.insert(&c.node);

    std::vector<const TimeWaiter*> order;
    while (auto h = q.head()) {
        order.push_back(h.get());
        static_cast<void>(q.remove(h));
    }
    CHECK(order == std::vector<const TimeWaiter*>{&a.node, &b.node, &c.node});
}

TEST_CASE("pop_due: exact boundary fires, one tick early does not", "[timequeue]") {
    TimerQueue q;
    Waiter a(100);
    q.insert(&a.node);

    CHECK_FALSE(q.pop_due(Ticks{99}));
    auto p = q.pop_due(Ticks{100});
    REQUIRE(p);
    CHECK(p->deadline_.tick_ == 100);
}

TEST_CASE("pop_due drains every due head then stops at the future", "[timequeue]") {
    TimerQueue q;
    Waiter a(10), b(20), c(30), d(100);
    q.insert(&a.node);
    q.insert(&b.node);
    q.insert(&c.node);
    q.insert(&d.node);

    Ticks now{25};
    auto p1 = q.pop_due(now);
    REQUIRE(p1);
    CHECK(p1->deadline_.tick_ == 10);
    auto p2 = q.pop_due(now);
    REQUIRE(p2);
    CHECK(p2->deadline_.tick_ == 20);
    CHECK_FALSE(q.pop_due(now));

    auto p3 = q.pop_due(Ticks{30});
    REQUIRE(p3);
    CHECK(p3->deadline_.tick_ == 30);

    CHECK_FALSE(q.pop_due(Ticks{99}));
    auto p4 = q.pop_due(Ticks{100});
    REQUIRE(p4);
    CHECK(p4->deadline_.tick_ == 100);
}

TEST_CASE("wraparound: due-check survives the rollover", "[timequeue]") {
    TimerQueue q;
    Waiter late{0xFFFFFFFCu};
    q.insert(&late.node);

    CHECK_FALSE(q.pop_due(Ticks{0xFFFFFFFBu}));
    auto p = q.pop_due(Ticks{0xFFFFFFFFu});
    REQUIRE(p);
    CHECK(p->deadline_.tick_ == 0xFFFFFFFCu);

    q.insert(&late.node);
    p = q.pop_due(Ticks{0x2u});
    REQUIRE(p);
    CHECK(p->deadline_.tick_ == 0xFFFFFFFCu);
}

TEST_CASE("wraparound: ordering across the rollover", "[timequeue]") {
    TimerQueue q;
    Waiter after_rollover(5), before_rollover{0xFFFFFFFCu};

    q.insert(&after_rollover.node);
    q.insert(&before_rollover.node);

    REQUIRE(q.head());
    CHECK(q.head()->deadline_.tick_ == 0xFFFFFFFCu);

    auto p = q.pop_due(Ticks{0x3u});
    REQUIRE(p);
    CHECK(p->deadline_.tick_ == 0xFFFFFFFCu);
    CHECK_FALSE(q.pop_due(Ticks{0x3u}));

    p = q.pop_due(Ticks{0x5u});
    REQUIRE(p);
    CHECK(p->deadline_.tick_ == 5);
}

TEST_CASE("remove keeps the rest ordered and reports re-removal", "[timequeue]") {
    TimerQueue q;
    Waiter a(10), b(20), c(30);
    q.insert(&a.node);
    q.insert(&b.node);
    q.insert(&c.node);

    CHECK(q.remove(&b.node));
    CHECK_FALSE(q.remove(&b.node));
    CHECK(drain_deadlines(q) == std::vector<std::uint32_t>{10, 30});
}

TEST_CASE("empty queue: null head, null pop_due", "[timequeue]") {
    TimerQueue q;
    CHECK_FALSE(q.head());
    CHECK_FALSE(q.pop_due(Ticks{1000}));
}
