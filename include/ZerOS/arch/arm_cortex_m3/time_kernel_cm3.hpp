#pragma once

#include "ZerOS/arch/arm_cortex_m3/cm3_time_port.hpp"
#include "ZerOS/kernel/clock/kernel.hpp"
#include <cstdint>

namespace ZerOS::arch::cortex_m3 {
using SystemTimeKernel = ZerOS::clock::Kernel<CortexM3Portable>;
extern SystemTimeKernel system_time;
void init_time(std::uint32_t cycles_per_tick);
} // namespace ZerOS::arch::cortex_m3