#pragma once

#include "ZerOS/arch/arm_cortex_m3/cm3_time_port.hpp"
#include "ZerOS/arch/arm_cortex_m3/system.hpp"
#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/clock/timer.hpp"
#include <cstdint>

namespace ZerOS::arch::cortex_m3 {
using SystemTimeKernel = ZerOS::clock::Kernel<CortexM3Portable>;
// the instance lives in time_kernel_cm3.cpp; runtime users go through
// system::os() — the extern is nobody's business now
void init_time(std::uint32_t cycles_per_tick);
} // namespace ZerOS::arch::cortex_m3

namespace ZerOS::clock {
// this chip's flavor of the timer
using Timer = TimerBase<arch::cortex_m3::CortexM3System>;
} // namespace ZerOS::clock