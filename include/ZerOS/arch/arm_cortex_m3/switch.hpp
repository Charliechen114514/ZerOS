#pragma once
#include "ZerOS/arch/arm_cortex_m3/cm3_irq.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include <cstdint>
#include <span>

namespace ZerOS::arch::cortex_m3 {

struct CortexM3SwitchPort : CortexM3CriticalSection {
    void request_switch();
};

using SystemScheduler = ZerOS::sched::Scheduler<CortexM3SwitchPort>;

void spawn(ZerOS::sched::TCB& t);
[[noreturn]] void start_scheduler(std::span<std::uint32_t> idle_stack);
} // namespace ZerOS::arch::cortex_m3