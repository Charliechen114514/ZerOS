// launch(): the everyday face of task creation — hand it a finished
// factory chain and it slices the storage off a wiring-time arena (bump,
// frozen once the scheduler starts), fills it and puts the task on the
// scheduler. Slot/span roads remain for hand-laid storage.

#include "ZerOS/arch/arm_cortex_m3/switch.hpp"

#include "ZerOS/base/check.hpp"
#include "ZerOS/kernel/sched/arena.hpp"
#include "ZerOS/kernel/sched/task.hpp"

#include <cstddef>

namespace {
// the wiring-time task block: ~8 demo-sized tasks of (TCB + 64-word stack).
// A named number beats a spell — resize here, the Check reports if dry
constexpr std::size_t kTaskArenaBytes = 2048;

alignas(8) constinit std::byte g_task_block[kTaskArenaBytes]{};
constinit ZerOS::task::Arena g_task_arena{g_task_block, sizeof(g_task_block)};
} // namespace

namespace ZerOS::arch::cortex_m3 {

sched::TCB* launch(task::Launchable spec) noexcept {
    // even words keep the stack TOP 8-aligned — same rule TaskStack asserts
    ZerOS::debug::Check(spec.words % 2 == 0, "launch: odd stack words");

    auto* box = static_cast<sched::TCBStorage*>(
        g_task_arena.take(sizeof(sched::TCBStorage), alignof(sched::TCBStorage)));
    auto* words =
        static_cast<std::uint32_t*>(g_task_arena.take(spec.words * 4, 8));
    ZerOS::debug::Check(box != nullptr && words != nullptr, "launch: task arena dry");

    return &spawn(task::named(spec.name)
                      .prio(spec.prio)
                      .stack(std::span<std::uint32_t>{words, spec.words})
                      .entry(spec.run.function, spec.run.argument),
                  *box);
}

} // namespace ZerOS::arch::cortex_m3
