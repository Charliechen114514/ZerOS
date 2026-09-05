#pragma once
#include "ZerOS/arch/arm_cortex_m3/cm3_irq.hpp"
#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include <cstdint>

namespace ZerOS::arch::cortex_m3 {

// For simple and toy, ARM and disarm currently using systicks
struct CortexM3Armable {
    void arm([[maybe_unused]] ZerOS::clock::Ticks::tick_t f) { return; }
    void disarm() {}
};

struct CortexM3Portable : public CortexM3Armable, CortexM3CriticalSection {};
static_assert(ZerOS::clock::TimePortable<CortexM3Portable>);

} // namespace ZerOS::arch::cortex_m3