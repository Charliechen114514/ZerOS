// Host unit tests for ZerOS/kernel/mem/typeable.hpp:
// the Make/Destroy placement-new facade over any MemoryPool.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <vector>

#include "ZerOS/kernel/mem/bitmap_allocate.hpp"
#include "ZerOS/kernel/mem/typeable.hpp"

using ZerOS::memory::BitmapPool;
using ZerOS::memory::Destroy;
using ZerOS::memory::Make;
using ZerOS::memory::MemoryAllocationError;
using ZerOS::memory::MemoryPool;

namespace {

// The canonical pool tenant: non-trivial ctor/dtor we can observe.
struct Gadget {
    int a;
    const char* tag;

    static inline int alive = 0;

    Gadget(int a_, const char* tag_) : a(a_), tag(tag_) { ++alive; }
    ~Gadget() { --alive; }
};

} // namespace

TEST_CASE("Make constructs in place and forwards arguments", "[typeable]") {
    BitmapPool<64, 4, false> pool;
    Gadget::alive = 0;

    auto r = Make<Gadget>(pool, 41, "answer");
    REQUIRE(r.has_value());
    CHECK((*r)->a == 41);
    CHECK(std::strcmp((*r)->tag, "answer") == 0);
    CHECK(Gadget::alive == 1);

    // blocks are max-aligned; the object must be too
    CHECK(reinterpret_cast<std::uintptr_t>(*r) % alignof(Gadget) == 0);

    CHECK(Destroy(pool, *r) == MemoryAllocationError::Ok);
    CHECK(Gadget::alive == 0);
}

TEST_CASE("move-only creation arguments compile and arrive intact", "[typeable]") {
    struct MoArg {
        int v;
        explicit MoArg(int v_) : v(v_) {}
        MoArg(const MoArg&) = delete;
        MoArg& operator=(const MoArg&) = delete;
    };

    // Holder only accepts MoArg&&: if Make ever forwarded by copy,
    // this translation unit would not compile (-Werror build).
    struct Holder {
        int got;
        explicit Holder(MoArg&& a) : got(a.v) {}
    };

    BitmapPool<64, 2, false> pool;
    auto r = Make<Holder>(pool, MoArg{7});
    REQUIRE(r.has_value());
    CHECK((*r)->got == 7);
    CHECK(Destroy(pool, *r) == MemoryAllocationError::Ok);
}

TEST_CASE("Destroy runs the destructor and recycles the block", "[typeable]") {
    BitmapPool<64, 4, false> pool;
    Gadget::alive = 0;

    auto r = Make<Gadget>(pool, 1, "a");
    REQUIRE(r.has_value());
    void* block = *r;

    CHECK(Destroy(pool, *r) == MemoryAllocationError::Ok);
    CHECK(Gadget::alive == 0); // destroy_at really ran

    // first-fit: the hole we just made is the next Make's landing spot
    auto again = Make<Gadget>(pool, 2, "b");
    REQUIRE(again.has_value());
    CHECK(*again == block);
    CHECK((*again)->a == 2);
}

TEST_CASE("Destroy of nullptr is a no-op Ok", "[typeable]") {
    BitmapPool<64, 2, false> pool;
    CHECK(Destroy(pool, static_cast<Gadget*>(nullptr)) == MemoryAllocationError::Ok);
}

TEST_CASE("Make propagates pool exhaustion", "[typeable]") {
    BitmapPool<64, 2, false> pool;
    Gadget::alive = 0;

    REQUIRE(Make<Gadget>(pool, 1, "a").has_value());
    REQUIRE(Make<Gadget>(pool, 2, "b").has_value());

    auto r = Make<Gadget>(pool, 3, "c");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == MemoryAllocationError::OutOfMemory);
    CHECK(Gadget::alive == 2); // nobody was constructed to fail
}

TEST_CASE("poison pools do not misfire across Make/Destroy cycles", "[typeable][poison]") {
    // raw_deallocate refills the block with POISON_VALUE; a clean cycle
    // (nobody writes after free) must reallocate without a false alarm.
    BitmapPool<64, 2, true> pool;

    for (int i = 0; i < 8; ++i) {
        auto r = Make<Gadget>(pool, i, "cycle");
        REQUIRE(r.has_value());
        CHECK((*r)->a == i);
        CHECK(Destroy(pool, *r) == MemoryAllocationError::Ok);
    }
}

TEST_CASE("an object exactly filling the block is accepted", "[typeable]") {
    struct ExactFit {
        std::uint64_t w[8];
    };
    static_assert(sizeof(ExactFit) == 64); // == BLOCK_SIZE, the static_assert boundary

    BitmapPool<64, 2, false> pool;
    auto r = Make<ExactFit>(pool); // zero creation args -> value-initialization
    REQUIRE(r.has_value());
    (*r)->w[7] = 0xDEADBEEFull;
    CHECK((*r)->w[7] == 0xDEADBEEFull);
    CHECK(Destroy(pool, *r) == MemoryAllocationError::Ok);
}

TEST_CASE("the alignof guard admits exactly-max-aligned objects", "[typeable][align]") {
    // The boundary case: alignment equal to the block alignment is fine.
    struct MaxAligned {
        alignas(std::max_align_t) std::byte blob[16];
    };
    static_assert(alignof(MaxAligned) == alignof(std::max_align_t));

    BitmapPool<64, 2, false> pool;
    auto r = Make<MaxAligned>(pool);
    REQUIRE(r.has_value());
    CHECK(reinterpret_cast<std::uintptr_t>(*r) % alignof(MaxAligned) == 0);
    CHECK(Destroy(pool, *r) == MemoryAllocationError::Ok);
}

TEST_CASE("many objects live side by side and die in scramble order", "[typeable]") {
    struct Note {
        unsigned id;
        explicit Note(unsigned i) : id(i) {}
    };
    static_assert(sizeof(Note) <= 32);

    BitmapPool<32, 8, true> pool;

    std::vector<Note*> live;
    for (unsigned i = 0; i < 8; ++i) {
        auto r = Make<Note>(pool, i);
        REQUIRE(r.has_value());
        CHECK((*r)->id == i);
        live.push_back(*r);
    }
    REQUIRE_FALSE(Make<Note>(pool, 99u).has_value());

    // scrambled teardown: each id must be intact right up to its Destroy
    for (unsigned i : {3u, 0u, 7u, 4u, 1u, 6u, 2u, 5u}) {
        CHECK(live[i]->id == i);
        CHECK(Destroy(pool, live[i]) == MemoryAllocationError::Ok);
    }

    // all blocks recycled: the full house fits again
    for (unsigned i = 0; i < 8; ++i) {
        REQUIRE(Make<Note>(pool, i).has_value());
    }
}

TEST_CASE("a destructor may allocate from its own pool", "[typeable]") {
    // Destroy runs destroy_at BEFORE raw_deallocate. A destructor that
    // allocates must land on a DIFFERENT block, never on the block it is
    // currently standing on. (Flip the order in Destroy and this fails:
    // the freed own block is the first-fit hole -> placement new would
    // overwrite the object whose destructor is still running.)
    using Pool = BitmapPool<64, 2, false>;
    Pool pool;

    Gadget* reborn = nullptr;
    struct Resurrector {
        Pool* host;
        Gadget** out;
        explicit Resurrector(Pool* h, Gadget** o) : host(h), out(o) {}
        ~Resurrector() {
            // no Catch2 macros inside a destructor (they throw); park the
            // result and assert outside.
            if (auto r = Make<Gadget>(*host, 7, "reborn")) {
                *out = *r;
            }
        }
    };
    static_assert(sizeof(Resurrector) <= 64);

    auto r = Make<Resurrector>(pool, &pool, &reborn);
    REQUIRE(r.has_value());
    auto* self = *r;

    CHECK(Destroy(pool, self) == MemoryAllocationError::Ok);

    REQUIRE(reborn != nullptr);         // the pool had a spare block
    CHECK(reborn != reinterpret_cast<Gadget*>(self)); // and it was NOT ours
    CHECK(reborn->a == 7);
    CHECK(Destroy(pool, reborn) == MemoryAllocationError::Ok);
}

TEST_CASE("Make also accepts pools without a BLOCK_SIZE constant", "[typeable]") {
    // The size guard is `requires`-gated; a bare MemoryPool must still work.
    struct BarePool {
        alignas(std::max_align_t) std::byte storage[64];
        bool used = false;

        std::expected<void*, MemoryAllocationError> raw_allocate() {
            if (used) {
                return std::unexpected(MemoryAllocationError::OutOfMemory);
            }
            used = true;
            return static_cast<void*>(storage);
        }
        MemoryAllocationError raw_deallocate(void* p) {
            if (p != storage || !used) {
                return MemoryAllocationError::NotOwned;
            }
            used = false;
            return MemoryAllocationError::Ok;
        }
        void* try_allocate() {
            auto r = raw_allocate();
            return r ? *r : nullptr;
        }
    };
    static_assert(MemoryPool<BarePool>);

    BarePool pool;
    auto r = Make<Gadget>(pool, 5, "bare");
    REQUIRE(r.has_value());
    CHECK((*r)->a == 5);
    CHECK(Destroy(pool, *r) == MemoryAllocationError::Ok);
}
