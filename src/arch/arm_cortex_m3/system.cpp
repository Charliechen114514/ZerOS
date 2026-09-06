// Bodies of the system face. This is the ONLY place that reaches for both
// singleinals at once; the extern declarations stay private to the arm
// .cpp family (definitions live in switch.cpp / time_kernel_cm3.cpp).

#include "ZerOS/arch/arm_cortex_m3/system.hpp"

#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"

namespace ZerOS::arch::cortex_m3 {

// defined in switch.cpp / time_kernel_cm3.cpp, declared no further
extern SystemScheduler system_sched;
extern SystemTimeKernel system_time;

constinit CortexM3System the_system{};

namespace {
// the wake reaction every timed sleeper on this chip shares: walk back
// from the alarm node to its owner and make it runnable again
void wake_from_sleep(ZerOS::base::BorrowedPtr<ZerOS::clock::TimeWaiter> waiter) {
    system_sched.ready(ZerOS::sched::TCB::owner_of(waiter));
}
} // namespace

CortexM3System& CortexM3System::self() {
    return the_system;
}

base::BorrowedPtr<sched::TCB> CortexM3System::current_task() {
    return system_sched.current_task();
}

void CortexM3System::ready(base::BorrowedPtr<sched::TCB> t) {
    system_sched.ready(t);
}

void CortexM3System::block(base::BorrowedPtr<sched::TCB> t) {
    system_sched.block(t);
}

void CortexM3System::sleep_for(base::BorrowedPtr<sched::TCB> t, clock::Ticks::tick_t span) {
    system_sched.sleep_for(t, system_time, span, wake_from_sleep);
}

void CortexM3System::yield() {
    system_sched.yield();
}

clock::Ticks CortexM3System::now() {
    return system_time.current();
}

sched::TaskPriority_t CortexM3System::prio(base::BorrowedPtr<sched::TCB> t) {
    return system_sched.prio(t);
}

void CortexM3System::reprioritize(base::BorrowedPtr<sched::TCB> t,
                                  sched::TaskPriority_t new_prio) {
    system_sched.reprioritize(t, new_prio);
}

void CortexM3System::arm_timer(base::BorrowedPtr<clock::TimeWaiter> w, clock::Ticks::tick_t span) {
    system_time.call_after_span(w, span);
}

void CortexM3System::cancel_timer(base::BorrowedPtr<clock::TimeWaiter> w) {
    system_time.cancel(w);
}

void CortexM3System::lock() {
    guard_.lock();
}

void CortexM3System::unlock() {
    guard_.unlock();
}

} // namespace ZerOS::arch::cortex_m3
