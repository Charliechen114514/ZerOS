// The capture side: a naked shell that never pushes a byte (the scene must
// stay virgin), then plain C that reads the registers and hands the report
// to whoever wired a sink.

#include "ZerOS/arch/arm_cortex_m3/fault.hpp"

#include "ZerOS/arch/arm_cortex_m3/system.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"

#include <cstdint>

namespace {
auto* const kCfsr = reinterpret_cast<volatile std::uint32_t*>(0xE000ED28);
auto* const kHfsr = reinterpret_cast<volatile std::uint32_t*>(0xE000ED2C);
auto* const kBfar = reinterpret_cast<volatile std::uint32_t*>(0xE000ED38);
} // namespace

extern "C" [[gnu::naked]] void HardFault_Handler() {
    asm volatile("tst    lr, #4\n" // EXC_RETURN bit2: which stack was in use
                 "ite    eq\n"
                 "mrseq  r0, msp\n"
                 "mrsne  r0, psp\n"
                 "mov    r1, lr\n"
                 "b      hardfault_capture\n"); // tail jump — no push on the way
}

extern "C" void hardfault_capture(std::uint32_t* frame,
                                  std::uint32_t exc_return [[maybe_unused]]) noexcept {
    using ZerOS::arch::cortex_m3::FaultReport;
    using ZerOS::sched::zeros_impl::TCBKeys;

    const char* name = "boot";
    std::uint8_t prio = 0xFF;
    if (auto cur = ZerOS::system::os().current_task()) { // boot crash: nobody runs
        name = TCBKeys::name(*cur);
        prio = TCBKeys::prio(*cur);
    }

    FaultReport r{
        .r0 = frame[0],
        .r1 = frame[1],
        .r2 = frame[2],
        .r3 = frame[3],
        .r12 = frame[4],
        .lr = frame[5],
        .pc = frame[6],
        .xpsr = frame[7],
        .cfsr = *kCfsr,
        .hfsr = *kHfsr,
        .bfar = *kBfar,
        .task_name = name,
        .task_prio = prio,
        .frame = frame,
    };

    if (ZerOS::arch::cortex_m3::fault_sink != nullptr) {
        ZerOS::arch::cortex_m3::fault_sink(r);
    }
    for (;;) { // testament delivered; lie down
        asm volatile("wfi");
    }
}
