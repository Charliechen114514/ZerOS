#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/arch.hpp"
#include "ZerOS/arch/arm_cortex_m3/trap.hpp"

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

constinit SystemScheduler system_sched{};

extern "C" std::uint32_t* context_switch(std::uint32_t* saved_sp) {
    if (saved_sp != nullptr) {
        TCBKeys::sp(*system_sched.current_task()) = saved_sp;
    }
    return TCBKeys::sp(*system_sched.pick_next());
}

extern "C" [[gnu::naked]] void SVC_Handler() {
    asm volatile("cpsid   i\n"
                 "movs    r0, #0\n" // first switch ever: nobody is running, nobody to save
                 "bl      context_switch\n"
                 "ldmia   r0!, {r4-r11}\n"
                 "msr     psp, r0\n"
                 "ldr     lr, =0xFFFFFFFD\n"
                 "cpsie   i\n"
                 "bx      lr\n");
}

extern "C" [[gnu::naked]] void PendSV_Handler() {
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
    fabricate_frame(t);
    system_sched.add(&t);
}

void start_scheduler(std::span<std::uint32_t> idle_stack) {
    auto idle = system_sched.fetch_idle_task();
    TCBKeys::wrapper(*idle) = {+[](void*) {
                                   for (;;) {
                                       asm volatile("wfi");
                                   }
                               },
                               nullptr};
    TCBKeys::stack_view(*idle) = idle_stack;
    fabricate_frame(*idle);

    *kShpr3 = (*kShpr3 & 0xFF00FFFFu) | (0xFFu << 16);
    asm volatile("dsb\nisb\nsvc 0");
    for (;;) {
    }
}

} // namespace ZerOS::arch::cortex_m3