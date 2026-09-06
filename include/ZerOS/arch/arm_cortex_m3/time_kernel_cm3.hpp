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

// The idle task's bed: before sleeping it consults the ledger and (with
// ZEROS_TICKLESS) re-arms SysTick as a one-shot to the next deadline;
// on wake it restores the periodic 1kHz. Without the build flag it is a
// plain wfi — bit-for-bit the old behavior.
void idle_sleep();
} // namespace ZerOS::arch::cortex_m3

// tick bookkeeping for diagnostics: periodic ticks served / one-shot
// expiries fired. extern "C" so any demo can read them by bare symbol.
extern "C" {
extern volatile std::uint32_t zeros_tick_periodic;
extern volatile std::uint32_t zeros_tick_oneshot;
}

namespace ZerOS::clock {
// this chip's flavor of the timer
using Timer = TimerBase<arch::cortex_m3::CortexM3System>;
} // namespace ZerOS::clock