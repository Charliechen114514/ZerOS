#pragma once
#include "ZerOS/arch/arm_cortex_m3/cm3_irq.hpp"
#include "ZerOS/base/callback.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include <cstdint>
#include <cstddef>

namespace ZerOS::arch::cortex_m3 {

struct CortexM3SwitchPort : CortexM3CriticalSection {
    void request_switch();
};

using SystemScheduler = ZerOS::sched::Scheduler<CortexM3SwitchPort>;

void spawn(ZerOS::sched::TCB& t);

// the C-shaped face: one line, one task. Storage slices off a wiring-time
// arena inside; the slot chain remains for hand-laid tasks
// the everyday road, builder-flavored: the chain names every field
// (a miss does not compile), storage slices off the launch arena
sched::TCB* launch(task::Launchable to_launch) noexcept;
// one road, one step: fill the box AND put the task on the scheduler.
// The factory chain stays type-state (a missing field does not compile);
// the arch privilege (frame + add) folds in here — callers see no seam.
ZerOS::sched::TCB& spawn(ZerOS::sched::TCBCreator spec,
                         ZerOS::sched::TCBStorage& box);
[[noreturn]] void start_scheduler(); // idle 栈内核自带
} // namespace ZerOS::arch::cortex_m3