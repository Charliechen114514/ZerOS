#pragma once
#include "ZerOS/kernel/clock/kernel.hpp"
#include "ZerOS/kernel/irq/critical_section.hpp"
#include <cstdint>

namespace ZerOS::arch::cortex_m3 {

inline std::uint32_t read_basepri() {
    std::uint32_t value = 0;
    asm volatile("mrs %0, basepri" : "=r"(value));
    return value;
}

inline void write_basepri(std::uint32_t value) {
    asm volatile("msr basepri, %0" ::"r"(value) : "memory");
}

// For simple and toy, ARM and disarm currently using systicks
struct CortexM3CriticalSection {
    void lock() {
        if (nest_++ == 0) {
            saved_basepri_ = read_basepri();
            write_basepri(kMaskLevel);
        }
    }
    void unlock() {
        if (--nest_ == 0) {
            write_basepri(saved_basepri_);
        }
    }

  private:
    static constexpr std::uint32_t kMaskLevel = 0x40;
    std::uint32_t nest_{0};
    std::uint32_t saved_basepri_{0};
};

static_assert(ZerOS::irq::CriticalSection<CortexM3CriticalSection>);

} // namespace ZerOS::arch::cortex_m3
