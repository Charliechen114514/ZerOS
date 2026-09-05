#pragma once
#include "ZerOS/arch/arm_cortex_m3/cm3_irq.hpp"
#include "ZerOS/kernel/sched/scheduler.hpp"
#include <cstdint>
#include <span>

namespace ZerOS::arch::cortex_m3 {

struct CortexM3SwitchPort : CortexM3CriticalSection { // 组合:锁 + 切换请求
    void request_switch();
};

using SystemScheduler = ZerOS::sched::Scheduler<CortexM3SwitchPort>;
extern SystemScheduler system_sched;

void spawn(ZerOS::sched::TCB& t);                          // 伪造帧 + add(装配期一次)
[[noreturn]] void start_scheduler(std::span<std::uint32_t> idle_stack); // 武装 + 首切,一去不回
// 应用面的 sleep/yield/block 见 <ZerOS/this_task.hpp>(D20);本层只剩装配
} // namespace ZerOS::arch::cortex_m3