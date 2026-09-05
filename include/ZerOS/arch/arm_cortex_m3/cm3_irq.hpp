#pragma once
#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include "stm32f1xx.h"
#include <assert.h>
#include <cstdint>

namespace ZerOS::arch::cortex_m3 {

// For simple and toy, ARM and disarm currently using systicks
struct CortexM3CriticalSection {
    void lock() {
        if (nest_++ == 0) {
            saved_basepri_ = __get_BASEPRI();
            __set_BASEPRI(kMaskLevel);
        }
    }
    void unlock() {
        if (--nest_ == 0) {
            __set_BASEPRI(saved_basepri_);
        }
    }

  private:
    static constexpr std::uint32_t kMaskLevel = 0x40;
    std::uint32_t nest_{0};
    std::uint32_t saved_basepri_{0};
};

static_assert(ZerOS::irq::CriticalSection<CortexM3CriticalSection>);

} // namespace ZerOS::arch::cortex_m3
