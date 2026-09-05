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

void spawn(ZerOS::sched::TCB& t);                          // 伪造帧 + add
void sleep_for(std::uint32_t ms);                          // 当前任务睡:arm+block 单临界区
[[noreturn]] void start_scheduler(std::span<std::uint32_t> idle_stack); // 武装 + 首切,一去不回
} // namespace ZerOS::arch::cortex_m3