#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "ZerOS/kernel/mem/bitmap_allocate.hpp"

using ZerOS::memory::BitmapPool;
using ZerOS::memory::MemoryAllocationError;

TEST_CASE("distinct blocks, first-fit reuse, double free", "[pool]") {
    BitmapPool<64, 8, true> pool;

    auto a = pool.raw_allocate();
    auto b = pool.raw_allocate();
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(*a != *b); // different callers must never share a block

    REQUIRE(pool.raw_deallocate(*a) == MemoryAllocationError::Ok);
    auto c = pool.raw_allocate();
    REQUIRE(c.has_value());
    CHECK(*c == *a); // lowest freed index comes back first

    pool.raw_deallocate(*c);
    CHECK(pool.raw_deallocate(*c) == MemoryAllocationError::NotOwned);
}

TEST_CASE("wild and interior pointers are NotOwned", "[pool]") {
    BitmapPool<64, 8, true> pool;

    int dummy = 0; // a stack object living outside the pool
    CHECK(pool.raw_deallocate(&dummy) == MemoryAllocationError::NotOwned);

    auto taken = pool.raw_allocate();
    REQUIRE(taken.has_value());
    auto* interior = static_cast<std::byte*>(*taken) + 8;
    CHECK(pool.raw_deallocate(interior) == MemoryAllocationError::NotOwned);
}

TEST_CASE("100 blocks spanning 4 l2 words keep l1/l2 honest", "[pool][l1l2]") {
    // Regression for the classic trio: l2 typed with word-count bits,
    // l1 typed with L1_SIZE bits, release only clearing l1 when its
    // word became EMPTY (the old bug froze l1 bits set forever).
    BitmapPool<16, 100, false> pool;
    std::vector<void*> ps;

    for (std::size_t i = 0; i < 100; ++i) {
        auto r = pool.raw_allocate();
        REQUIRE(r.has_value());
        ps.push_back(*r);
    }
    CHECK_FALSE(pool.raw_allocate().has_value()); // exhausted

    pool.raw_deallocate(ps[33]); // the ONLY hole in the whole pool
    auto q = pool.raw_allocate();
    REQUIRE(q.has_value());
    CHECK(*q == ps[33]); // the hole must be found again through l1 -> l2
}

TEST_CASE("poison catches write-after-free", "[pool][poison]") {
    BitmapPool<64, 4, true> pool;

    auto v = pool.raw_allocate();
    REQUIRE(v.has_value());
    auto* p = static_cast<unsigned char*>(*v);
    pool.raw_deallocate(p); // poison-on-free fills the block

    p[3] ^= 0xFF;           // sneak write into a free block
    auto r = pool.raw_allocate();
    CHECK_FALSE(r.has_value());
    CHECK(r.error() == MemoryAllocationError::Poisoned);
}

TEST_CASE("a clean free reallocates without false poison", "[pool][poison]") {
    BitmapPool<64, 4, true> pool;

    auto v = pool.raw_allocate();
    REQUIRE(v.has_value());
    pool.raw_deallocate(*v); // nobody touched it since the free

    auto r = pool.raw_allocate();
    REQUIRE(r.has_value()); // ever_poisoned_ gates the check, must not misfire
    CHECK(*r == *v);
}

TEST_CASE("try_allocate reports exhaustion as nullptr", "[pool]") {
    BitmapPool<64, 2, false> pool;

    CHECK(pool.try_allocate() != nullptr);
    CHECK(pool.try_allocate() != nullptr);
    CHECK(pool.try_allocate() == nullptr); // ISR-safe surface, no expected<>
}

TEST_CASE("randomized torture: interleaved alloc/free keeps invariants", "[pool][fuzz]") {
    // Fixed seed: a failure must reproduce bit-for-bit on every machine.
    std::mt19937 rng{20260904u};

    constexpr std::size_t kBlocks = 64;
    BitmapPool<16, kBlocks, true> pool; // poison ON: exercises ever_poisoned_ gating too

    struct Live {
        void* p;
        std::uint64_t stamp;
    };
    std::vector<Live> live;
    live.reserve(kBlocks);

    std::size_t allocs = 0, frees = 0;
    constexpr std::size_t kOps = 20000;

    for (std::size_t op = 0; op < kOps; ++op) {
        // 55/45 bias towards alloc so the pool really saturates and drains;
        // forced alloc when empty keeps `live` indices well-defined.
        const bool want_alloc = live.empty() || (rng() % 100u) < 55u;

        if (want_alloc) {
            auto r = pool.raw_allocate();
            if (live.size() == kBlocks) {
                // holding every block => the pool must report exhaustion
                REQUIRE_FALSE(r.has_value());
                REQUIRE(r.error() == MemoryAllocationError::OutOfMemory);
                continue;
            }
            REQUIRE(r.has_value());

            // stamp the block: if the stamp is ever broken, two owners
            // (or a wild write) touched this block between alloc and free.
            const auto stamp = (static_cast<std::uint64_t>(op) << 32) ^ allocs;
            auto* w = static_cast<std::uint64_t*>(*r); // 16B blocks, max-aligned
            w[0] = stamp;
            w[1] = ~stamp;

            live.push_back({*r, stamp});
            ++allocs;
        } else {
            const auto idx = rng() % live.size();
            auto* w = static_cast<std::uint64_t*>(live[idx].p);
            REQUIRE(w[0] == live[idx].stamp); // still exclusively ours?
            REQUIRE(w[1] == ~live[idx].stamp);

            REQUIRE(pool.raw_deallocate(live[idx].p) == MemoryAllocationError::Ok);
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(idx));
            ++frees;
        }
    }

    // drain: every survivor must still carry an intact stamp and free cleanly
    for (const auto& b : live) {
        auto* w = static_cast<std::uint64_t*>(b.p);
        REQUIRE(w[0] == b.stamp);
        REQUIRE(w[1] == ~b.stamp);
        REQUIRE(pool.raw_deallocate(b.p) == MemoryAllocationError::Ok);
    }
    live.clear();

    // a fully-drained pool must hand out all blocks again, then report full
    std::vector<void*> refill;
    for (std::size_t i = 0; i < kBlocks; ++i) {
        auto r = pool.raw_allocate();
        REQUIRE(r.has_value());
        refill.push_back(*r);
    }
    REQUIRE_FALSE(pool.raw_allocate().has_value());

    CHECK(allocs > 100); // guard against an accidentally idle test
    CHECK(frees > 100);
}
