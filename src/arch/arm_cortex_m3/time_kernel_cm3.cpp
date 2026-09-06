#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include <cstdint>

namespace ZerOS::arch::cortex_m3 {
constinit SystemTimeKernel system_time{};
extern SystemScheduler system_sched; // lives in switch.cpp; the tick counts
                                     // its slices, that is wiring's business
} // namespace ZerOS::arch::cortex_m3

namespace {
auto* const kSystCsr = reinterpret_cast<volatile std::uint32_t*>(0xE000E010);
auto* const kSystRvr = reinterpret_cast<volatile std::uint32_t*>(0xE000E014);
auto* const kSystCvr = reinterpret_cast<volatile std::uint32_t*>(0xE000E018);
auto* const kScbShpr3 = reinterpret_cast<volatile std::uint32_t*>(0xE000ED20);
auto* const kNvicIcpr = reinterpret_cast<volatile std::uint32_t*>(0xE000E280);

constexpr std::uint32_t kSystEnable = 1u;
constexpr std::uint32_t kSystTickInt = 2u;
constexpr std::uint32_t kSystClkSource = 4u;
constexpr std::uint32_t kSystCountFlag = 1u << 16;
constexpr std::uint32_t kSystickIrqBit = 1u << 15; // SysTick = IRQ 15

std::uint32_t cycles_per_tick_ = 0; // init_time's bookkeeping

// one-shot in flight: >0 means SysTick runs as a one-shot that owes the
// ledger exactly this many ticks when it fires. The ONLY truth both the
// handler and idle_sleep's arming/wake paths agree on.
volatile std::uint32_t one_shot_ticks_ = 0;

// below this distance a one-shot buys nothing — the periodic tick (which
// IS the "arm split into 1-tick installments" extreme) wakes us anyway
[[maybe_unused]] constexpr std::uint32_t kTicklessThreshold = 2;

void periodic_table_restore() {
    if (cycles_per_tick_ == 0) {
        return;
    }
    *kSystRvr = cycles_per_tick_ - 1;
    *kSystCvr = 0; // any VAL write restarts the counter
    *kSystCsr = kSystEnable | kSystTickInt | kSystClkSource;
}

// Drain a possibly-pending SysTick expiry: withdraw the NVIC pending bit
// and, if the counter really wrapped, settle that elapsed time on the
// ledger by hand. Without this, an expiry racing our (atomic) re-arm
// would be misread: the handler would see one_shot_ticks_ set and settle
// a whole one-shot for what was a single periodic tick.
[[maybe_unused]] void drain_pending_expiry(std::uint32_t settle_ticks) {
    const bool wrapped = (*kSystCsr & kSystCountFlag) != 0; // read-clears
    *kNvicIcpr = kSystickIrqBit;                           // un-pend it
    if (wrapped && settle_ticks != 0) {
        ZerOS::arch::cortex_m3::system_time.on_elapsed(settle_ticks);
    }
}
} // namespace

extern "C" {
volatile std::uint32_t zeros_tick_periodic = 0;
volatile std::uint32_t zeros_tick_oneshot = 0;
}

void ZerOS::arch::cortex_m3::init_time(std::uint32_t cycles_per_tick) {
    // same level with PendSV
    *kScbShpr3 = (*kScbShpr3 & 0x00FFFFFFu) | (0xFFu << 24);
    cycles_per_tick_ = cycles_per_tick;
    *kSystRvr = cycles_per_tick - 1;
    *kSystCvr = 0;
    *kSystCsr = kSystEnable | kSystTickInt | kSystClkSource;
}

void ZerOS::arch::cortex_m3::idle_sleep() {
#ifndef ZEROS_TICKLESS
    asm volatile("wfi");
#else
    if (cycles_per_tick_ == 0) {
        asm volatile("wfi"); // time never started — nothing to consult
        return;
    }

    // ---- arming: one atomic section, races settled by draining ----
    asm volatile("cpsid i");
    drain_pending_expiry(1); // a periodic tick may have expired on our way in

    std::uint32_t due = 0;
    const bool has_deadline = system_time.next_due_in(due);
    if (has_deadline && due > kTicklessThreshold) {
        // clamp to the 24-bit LOAD width; longer spans are split by the
        // idle loop itself — each expiry wakes us once to re-consult
        const std::uint32_t max_per_shot = 0xFFFFFFu / cycles_per_tick_;
        if (due > max_per_shot) {
            due = max_per_shot;
        }
        one_shot_ticks_ = due;
        *kSystRvr = due * cycles_per_tick_ - 1;
        *kSystCvr = 0;
        *kSystCsr = kSystEnable | kSystTickInt | kSystClkSource;
    } else if (!has_deadline) {
        *kSystCsr = kSystClkSource; // stop: nothing ever comes due
    }
    // else: due in 1..threshold ticks — leave the periodic table, wfi
    // wakes within one tick anyway
    asm volatile("cpsie i");

    asm volatile("wfi");

    // ---- wake: restore the periodic table, settle what we withdrew ----
    asm volatile("cpsid i");
    if (one_shot_ticks_ != 0) {
        // in flight (a peripheral woke us first, or it never fired):
        // settle honestly if it had fired, then take it off the books
        drain_pending_expiry(one_shot_ticks_);
        one_shot_ticks_ = 0;
    } else {
        // the handler already settled our one-shot — but a fresh
        // periodic tick may be pending since the restore
        drain_pending_expiry(1);
    }
    periodic_table_restore();
    asm volatile("cpsie i");
#endif
}

extern "C" void SysTick_Handler() {
    if (one_shot_ticks_ != 0) {
        // one-shot expiry: settle the whole span, back to periodic. No
        // on_tick() — the only runnable thing here is idle (nobody to
        // slice); if the settlement readied someone, PendSV is already
        // pending and the handover happens on our way out
        const auto n = one_shot_ticks_;
        one_shot_ticks_ = 0;
        periodic_table_restore();
        zeros_tick_oneshot = zeros_tick_oneshot + 1;
        ZerOS::arch::cortex_m3::system_time.on_elapsed(n);
        return;
    }
    zeros_tick_periodic = zeros_tick_periodic + 1;
    ZerOS::arch::cortex_m3::system_time.on_elapsed(1); // report the time...
    ZerOS::arch::cortex_m3::system_sched.on_tick();    // ...and the beat (slice)
}
