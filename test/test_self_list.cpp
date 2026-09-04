#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/base/self_list.hpp"

using ZerOS::base::BorrowedPtr;
using ZerOS::base::SelfList;
using ZerOS::base::SelfNode;

namespace {

struct Item : SelfNode<Item> {
    int key;
    int id;

    Item(int k, int i) : key(k), id(i) {}
};

struct ByKey {
    constexpr bool operator()(const Item& a, const Item& b) const { return a.key < b.key; }
};

template <typename List>
std::vector<int> drain_ids(List& l) {
    std::vector<int> out;
    while (auto h = l.pop_head()) {
        out.push_back(h->id);
    }
    return out;
}

} // namespace

TEST_CASE("sorted insert places head, middle and tail", "[selflist]") {
    SelfList<Item, ByKey> l;
    Item a(2, 10), b(1, 20), c(3, 30), d(2, 40);

    l.insert(&a);
    l.insert(&b);
    l.insert(&c);
    l.insert(&d);

    CHECK(drain_ids(l) == std::vector<int>{20, 10, 40, 30});
}

TEST_CASE("equal keys keep insertion order (FIFO)", "[selflist]") {
    SelfList<Item, ByKey> l;
    Item a(5, 1), b(5, 2), c(5, 3), d(5, 4);

    l.insert(&a);
    l.insert(&b);
    l.insert(&c);
    l.insert(&d);

    CHECK(drain_ids(l) == std::vector<int>{1, 2, 3, 4});
}

TEST_CASE("default Less appends in arrival order", "[selflist]") {
    SelfList<Item> l;
    Item a(9, 1), b(0, 2), c(5, 3);

    l.insert(&a);
    l.insert(&b);
    l.insert(&c);

    CHECK(drain_ids(l) == std::vector<int>{1, 2, 3});
}

TEST_CASE("remove unlinks head, middle and tail", "[selflist]") {
    SelfList<Item, ByKey> l;
    Item a(1, 1), b(2, 2), c(3, 3);
    l.insert(&a);
    l.insert(&b);
    l.insert(&c);

    CHECK(l.remove(&b));
    CHECK(l.head()->id == 1);

    CHECK(l.remove(&a));
    CHECK(l.head()->id == 3);

    CHECK(l.remove(&c));
    CHECK(l.empty());
}

TEST_CASE("remove of missing or already-removed node reports false", "[selflist]") {
    SelfList<Item, ByKey> l;
    Item a(1, 1), b(2, 2);
    Item outsider(9, 9);
    l.insert(&a);
    l.insert(&b);

    CHECK_FALSE(l.remove(&outsider));
    CHECK(l.remove(&a));
    CHECK_FALSE(l.remove(&a));

    CHECK(drain_ids(l) == std::vector<int>{2});
}

TEST_CASE("pop_head detaches the node for re-insertion", "[selflist]") {
    SelfList<Item, ByKey> l;
    Item a(1, 1), b(2, 2), c(3, 3);
    l.insert(&a);
    l.insert(&b);
    l.insert(&c);

    auto h = l.pop_head();
    REQUIRE(h);
    CHECK(h->id == 1);
    CHECK(a.next_ == nullptr);
    CHECK(l.head()->id == 2);

    l.insert(&a);
    CHECK(drain_ids(l) == std::vector<int>{1, 2, 3});
}

TEST_CASE("empty list behaviors", "[selflist]") {
    SelfList<Item, ByKey> l;
    CHECK(l.empty());
    CHECK_FALSE(l.head());
    CHECK_FALSE(l.pop_head());

    Item a(1, 1);
    l.insert(&a);
    CHECK_FALSE(l.empty());
    CHECK(l.pop_head());
    CHECK(l.empty());
}

TEST_CASE("BorrowedPtr doors and equality", "[selflist][borrowed]") {
    Item a(1, 1);
    Item* raw = &a;

    BorrowedPtr<Item> empty;
    CHECK_FALSE(empty);
    CHECK(empty.get() == nullptr);

    BorrowedPtr<Item> p = raw;
    CHECK(p);
    CHECK(p.get() == raw);
    CHECK(p == raw);
    CHECK(p == BorrowedPtr<Item>(raw));
    CHECK(p != nullptr);
    CHECK(&*p == raw);
    CHECK(p->id == 1);
}
