#pragma once

// The last will and testament: when the chip dies, it dictates the scene
// before lying down. Declaration face only — bodies live in fault.cpp.

#include <cstdint>

namespace ZerOS::arch::cortex_m3 {

struct FaultReport {
    std::uint32_t r0, r1, r2, r3, r12, lr, pc, xpsr; // the hardware-pushed frame
    std::uint32_t cfsr, hfsr, bfar;                  // SCB fault status & address
    const char* task_name;                           // who was running ("boot" if nobody)
    std::uint8_t task_prio;
    const std::uint32_t* frame;                      // stack top, for dumping
};

using FaultSink = void (*)(const FaultReport&);
inline FaultSink fault_sink{nullptr}; // the board wires its printer here

} // namespace ZerOS::arch::cortex_m3
