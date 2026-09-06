#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/arch.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/arch/arm_cortex_m3/trap.hpp"
#include "ZerOS/kernel/sched/stack_guard.hpp"

namespace {
auto* const kIcsr = reinterpret_cast<volatile std::uint32_t*>(0xE000ED04);  // SCB ICSR
auto* const kShpr3 = reinterpret_cast<volatile std::uint32_t*>(0xE000ED20); // SCB SHPR3
} // namespace

namespace ZerOS::arch::cortex_m3 {
using sched::zeros_impl::TCBKeys; // the naked mechanics key, see task_control_block.hpp

void CortexM3SwitchPort::request_switch() {
    *kIcsr = (1u << 28); // Set the PendSV intrs
}

void fabricate_frame(ZerOS::sched::TCB& t) {
    auto view = sched::zeros_impl::TCBKeys::stack_view(t);
    auto* top = view.data() + view.size();
    auto* hw = reinterpret_cast<ExceptionFrame*>(top) - 1;
    auto* sw = reinterpret_cast<SoftwareFrame*>(hw) - 1;

    *sw = {}; // Clear, memset set bytes, we say class clean
    *hw = ExceptionFrame{
        .r0 = reinterpret_cast<std::uint32_t>(&TCBKeys::wrapper(t)), // 任务参数
        .r1 = 0x00000000,
        .r2 = 0x00000000,
        .r3 = 0x00000000,
        .r12 = 0x00000000,
        .lr = reinterpret_cast<std::uint32_t>(&Trap), // 返回即陷阱
        .pc = reinterpret_cast<std::uint32_t>(&sched::UserTaskWrapper::InvokeUserTask), // 跳板
        .xpsr = 0x01000000u,
    };

    TCBKeys::sp(t) = reinterpret_cast<std::uint32_t*>(sw);
}

// PendSV-only probe lives in scripts/patches/pendsv-probe.patch: the hot
// path stays measurement-clean on the mainline, applying the patch adds
// two branch-free DWT snapshots (~24 cycles, see documents/notes/perf_1.md
// round 6). Building the perf demo with ZEROS_MEASURE_PENDSV without the
// patch applied fails to build — loudly, on purpose.

constinit SystemScheduler system_sched{};

// A returned task must LEAVE the cpu, not squat on it: a plain wfi here
// would only ever be pried loose by the next tick's time slice, so a
// system without SysTick wedges outright (caught live by the NO_TICK
// perf build, 2026-09-06). Block self — the scheduler moves on, and if
// nothing else is ready, the idle task's wfi is the honest terminal
// state. A stray wake re-enters the wfi loop below, equally harmless.
extern "C" [[noreturn]] void Trap() {
    system_sched.block(system_sched.current_task());
    while (true) {
        asm volatile("wfi");
    }
}

// Switch hot path stays in FLASH (not .ramfunc): real-hardware measurement
// (documents/notes/perf_1.md, round 3 & 5) shows RAM placement is a
// NEGATIVE optimization on STM32F103 @72MHz — the flash prefetch buffer
// beats zero-wait-state RAM by 16 cycles. If a future chip/config changes
// the tradeoff, re-measure before re-enabling.
// #define ZEROS_SWITCH_IN_RAM  (kept for reference; do NOT enable on F103)
#ifdef ZEROS_SWITCH_IN_RAM
#    define ZEROS_RAMFUNC __attribute__((section(".ramfunc"), noinline))
#else
#    define ZEROS_RAMFUNC
#endif

extern "C" ZEROS_RAMFUNC std::uint32_t* context_switch(std::uint32_t* saved_sp) {
    using TaskGuardHelper = task::TaskGuardHelper;
    if (saved_sp != nullptr) {
        auto prev = system_sched.current_task();
        TCBKeys::sp(*prev) = saved_sp;
        if (TaskGuardHelper::fast_check_stack(*prev)) {
            // an eaten canary with nobody listening must NOT pass in silence
            ZerOS::debug::Check(TaskGuardHelper::overflow_reporter != nullptr,
                                "overflow reporter not wired");
            TaskGuardHelper::overflow_reporter(prev, TaskGuardHelper::eaten_length(*prev));
        }
    }
    return TCBKeys::sp(*system_sched.pick_next_locked());
}

extern "C" ZEROS_RAMFUNC [[gnu::naked]] void SVC_Handler() {
    asm volatile("cpsid   i\n"
                 "movs    r0, #0\n" // first switch ever: nobody is running, nobody to save
                 "bl      context_switch\n"
                 "ldmia   r0!, {r4-r11}\n"
                 "msr     psp, r0\n"
                 "ldr     lr, =0xFFFFFFFD\n"
                 "cpsie   i\n"
                 "bx      lr\n");
}

extern "C" ZEROS_RAMFUNC [[gnu::naked]] void PendSV_Handler() {
    asm volatile("cpsid   i\n"
                 "mrs     r0, psp\n"
                 "cbz     r0, 1f\n"
                 "stmdb   r0!, {r4-r11}\n"
                 "1:\n"
                 "bl      context_switch\n"
                 "ldmia   r0!, {r4-r11}\n"
                 "msr     psp, r0\n"
                 "ldr     lr, =0xFFFFFFFD\n"
                 "cpsie   i\n"
                 "bx      lr\n");
}

void spawn(ZerOS::sched::TCB& t) {
    task::TaskGuardHelper::bury_canary(t);
    fabricate_frame(t);
    system_sched.add(&t);
}

sched::TCB& spawn(sched::TCBCreator spec, sched::TCBStorage& box) {
    auto& t = spec.spawn_into(box);
    spawn(t);
    return t;
}

void start_scheduler() {
    auto idle = system_sched.fetch_idle_task();
    TCBKeys::wrapper(*idle) = {+[](void*) {
                                   for (;;) {
                                       idle_sleep(); // the bed: plain wfi, or
                                                     // tickless-armed when built so
                                   }
                               },
                               nullptr};
    // the idle stack was wired by the Scheduler itself — nothing to feed
    task::TaskGuardHelper::bury_canary(*idle);
    fabricate_frame(*idle);

    *kShpr3 = (*kShpr3 & 0xFF00FFFFu) | (0xFFu << 16);
    asm volatile("dsb\nisb\nsvc 0");
    for (;;) {
    }
}

} // namespace ZerOS::arch::cortex_m3