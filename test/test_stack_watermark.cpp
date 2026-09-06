// 栈水位(host 白盒):涂刷基线、自底向上扫描、最深使用读数
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <span>

#include "ZerOS/kernel/sched/stack_guard.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"

using ZerOS::sched::TCB;
using ZerOS::sched::TCBStorage;
using ZerOS::sched::zeros_impl::TCBKeys;
using ZerOS::task::TaskGuardHelper;

namespace {

constexpr auto kNoopEntry = +[](void*) {};

struct Pinned {
    TCBStorage storage{};
    std::uint32_t stack[32]; // 32 words: 8 canary + 24 usable
    TCB& task;

    Pinned()
        : task(ZerOS::task::named("t")
                   .prio(1)
                   .stack(std::span<std::uint32_t>{stack})
                   .entry(kNoopEntry, nullptr)
                   .spawn_into(storage)) {
        TaskGuardHelper::bury_canary(task);
    }

    std::span<std::uint32_t> view() { return TCBKeys::stack_view(task); }
};

} // namespace

TEST_CASE("paint covers the whole stack, not just the canary", "[watermark]") {
    Pinned p;
    for (std::size_t i = 0; i < p.view().size(); ++i) {
        REQUIRE(p.view()[i] == ZerOS::task::kCanaryWord);
    }
}

TEST_CASE("untouched task reports full capacity free", "[watermark]") {
    Pinned p;
    REQUIRE(TaskGuardHelper::capacity_words(p.task) == 24);
    REQUIRE(TaskGuardHelper::watermark_free_words(p.task) == 24);
}

TEST_CASE("pushing from the top eats the margin, deepest push wins", "[watermark]") {
    Pinned p;
    auto v = p.view();

    // a few frames near the top: [size-4, size) written
    for (std::size_t i = v.size() - 4; i < v.size(); ++i) {
        v[i] = 0;
    }
    REQUIRE(TaskGuardHelper::watermark_free_words(p.task) == 24 - 4);

    // deeper push later in life: down to word 12 (inclusive) — the
    // watermark only ever goes DOWN, old shallower writes stay counted
    for (std::size_t i = 12; i < v.size(); ++i) {
        v[i] = 1;
    }
    REQUIRE(TaskGuardHelper::watermark_free_words(p.task) == 12 - 8);
    REQUIRE(TaskGuardHelper::capacity_words(p.task) == 24);

    // and the canary zone itself is never part of the margin
    REQUIRE(TaskGuardHelper::fast_check_stack(p.task) == false);
}

TEST_CASE("two ways to be wrong, both bounded and known", "[watermark]") {
    SECTION("a hole above the deepest write is invisible to the scan") {
        Pinned p;
        auto v = p.view();
        // frames wrote [12, size) but never touched word 13 — the scan
        // stops at 12 (first non-paint) before ever reaching the hole:
        // the reading stays exact for the written-word semantics
        for (std::size_t i = 12; i < v.size(); ++i) {
            v[i] = (i == 13) ? ZerOS::task::kCanaryWord : 1;
        }
        REQUIRE(TaskGuardHelper::watermark_free_words(p.task) == 12 - 8);
    }

    SECTION("the only flattery: the deepest frame's bottom word unwritten") {
        Pinned p;
        auto v = p.view();
        // the frame REACHED down to word 11 (SP semantics: free == 3)
        // but only words [12, size) were actually stored to — the
        // written-word scan reports 4: one word optimistic, bounded by
        // the frame's own alignment slop, the trade every RTOS takes
        for (std::size_t i = 12; i < v.size(); ++i) {
            v[i] = 1;
        }
        REQUIRE(TaskGuardHelper::watermark_free_words(p.task) == 12 - 8);
    }

    SECTION("a lone stray word at the very bottom makes it pessimistic") {
        Pinned p;
        p.view()[8] = 0xDEADBEEF; // out-of-bounds single write below everything
        REQUIRE(TaskGuardHelper::watermark_free_words(p.task) == 0); // safe direction
    }
}
