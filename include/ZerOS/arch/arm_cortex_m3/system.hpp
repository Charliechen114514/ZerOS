#pragma once

// The Cortex-M3 answer to system::SystemContext: one face over the two
// singletons (scheduler + time kernel, D15 keeps them un-merged). The
// class here is a declaration face only — every method body lives in
// system.cpp, so no register, no singleton, leaks through this header.

#include "ZerOS/arch/arm_cortex_m3/cm3_irq.hpp"
#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/clock/ticks.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"
#include "ZerOS/kernel/system_concept.hpp"

namespace ZerOS::arch::cortex_m3 {

class CortexM3System {
  public:
    // the static door generic code walks in through (SystemContext: self())
    [[nodiscard]] static CortexM3System& self();

    base::BorrowedPtr<sched::TCB> current_task();
    void ready(base::BorrowedPtr<sched::TCB> t);
    void block(base::BorrowedPtr<sched::TCB> t);
    void sleep_for(base::BorrowedPtr<sched::TCB> t, clock::Ticks::tick_t span);
    void yield();
    [[nodiscard]] clock::Ticks now();
    void lock();
    void unlock();

  private:
    // guards the system face itself; BASEPRI, nest-friendly like every
    // other lock in the family
    CortexM3CriticalSection guard_{};
};

static_assert(ZerOS::system::SystemContext<CortexM3System>);

// the one and only system on this chip; born by loading (constinit)
extern constinit CortexM3System the_system;

} // namespace ZerOS::arch::cortex_m3

namespace ZerOS::system {
// the handover point: generic code asks for THE system, never the parts
[[nodiscard]] inline arch::cortex_m3::CortexM3System& os() {
    return arch::cortex_m3::the_system;
}
} // namespace ZerOS::system
