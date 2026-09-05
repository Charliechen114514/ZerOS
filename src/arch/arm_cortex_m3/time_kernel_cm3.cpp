#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "core_cm3.h"

namespace ZerOS::arch::cortex_m3 {
constinit SystemTimeKernel system_time{};
}

void ZerOS::arch::cortex_m3::init_time(std::uint32_t cycles_per_tick) {
    NVIC_SetPriority(SysTick_IRQn, 0xF0); // Lowest Level
    SysTick_Config(cycles_per_tick);
}

extern "C" void SysTick_Handler() {
    ZerOS::arch::cortex_m3::system_time.on_elapsed(1);
}