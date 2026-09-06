#pragma once

// The opt-in default testaments: print-and-halt for both death styles.
// Explicitly installed (nobody silently gets defaults), printing through
// the registered log channel — this file never touches a UART itself.

#include "ZerOS/arch/arm_cortex_m3/fault.hpp"
#include "ZerOS/base/check.hpp"
#include "ZerOS/kernel/sched/stack_guard.hpp"
#include "ZerOS/log/print.hpp"

namespace ZerOS::arch::cortex_m3 {

inline void install_default_testaments() noexcept {
    ZerOS::task::TaskGuardHelper::overflow_reporter =
        +[](base::BorrowedPtr<sched::TCB> t, std::size_t eaten) {
            ZerOS::log::print("[STACK OVERFLOW] task={} eaten={}\r\n",
                              sched::zeros_impl::TCBKeys::name(*t), ZerOS::log::Dec{eaten});
            for (;;) {
                asm volatile("wfi");
            }
        };

    fault_sink = +[](const FaultReport& r) {
        ZerOS::log::print("\r\n*** HARD FAULT *** task={} prio={}\r\n", r.task_name,
                          ZerOS::log::Dec{r.task_prio});
        ZerOS::log::print("pc=0x{} lr=0x{} xpsr=0x{}\r\n", ZerOS::log::Hex{r.pc},
                          ZerOS::log::Hex{r.lr}, ZerOS::log::Hex{r.xpsr});
        ZerOS::log::print("cfsr=0x{} hfsr=0x{} bfar=0x{}\r\n", ZerOS::log::Hex{r.cfsr},
                          ZerOS::log::Hex{r.hfsr}, ZerOS::log::Hex{r.bfar});
        ZerOS::log::print("stack:");
        for (int i = 0; i < 8; ++i) {
            ZerOS::log::print(" {}", ZerOS::log::Hex{r.frame[i]});
        }
        ZerOS::log::print("\r\n");
    };
}

} // namespace ZerOS::arch::cortex_m3
