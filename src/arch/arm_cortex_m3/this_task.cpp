// The Cortex-M3 answer to the ThisTask face: owns the wiring knowledge
// (both singletons + the wake reaction), then stamps the template out.

#include "system/this_task_impl.hpp"

#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/kernel/clock/clock.hpp"
#include "ZerOS/kernel/sched/task_control_block.hpp"

namespace {

using ZerOS::arch::cortex_m3::system_sched;
using ZerOS::arch::cortex_m3::system_time;

void wake_from_sleep(ZerOS::base::BorrowedPtr<ZerOS::clock::TimeWaiter> waiter) {
    system_sched.ready(ZerOS::sched::TCB::owner_of(waiter));
}

} // namespace

namespace ZerOS::detail {

struct SystemTaskBackend {
    static void sleep_for(Milliseconds delay) noexcept {
        system_sched.sleep_for(system_sched.current_task(), system_time, delay.count,
                               wake_from_sleep);
    }

    static void yield() noexcept {
        system_sched.yield();
    }

    static void block() noexcept {
        system_sched.block(system_sched.current_task());
    }
};

} // namespace ZerOS::detail

namespace ZerOS {
// stamp the face for THIS program; business .o files link plain calls
template struct BasicThisTask<detail::SystemTaskBackend>;
} // namespace ZerOS
