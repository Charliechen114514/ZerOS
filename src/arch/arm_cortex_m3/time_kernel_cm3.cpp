#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include <cstdint>

namespace ZerOS::arch::cortex_m3 {
constinit SystemTimeKernel system_time{};
}

namespace {
auto* const kSystCsr = reinterpret_cast<volatile std::uint32_t*>(0xE000E010);
auto* const kSystRvr = reinterpret_cast<volatile std::uint32_t*>(0xE000E014);
auto* const kSystCvr = reinterpret_cast<volatile std::uint32_t*>(0xE000E018);
auto* const kScbShpr3 = reinterpret_cast<volatile std::uint32_t*>(0xE000ED20);

constexpr std::uint32_t kSystEnable = 1u;
constexpr std::uint32_t kSystTickInt = 2u;
constexpr std::uint32_t kSystClkSource = 4u;
} // namespace

void ZerOS::arch::cortex_m3::init_time(std::uint32_t cycles_per_tick) {
    *kScbShpr3 = (*kScbShpr3 & 0x00FFFFFFu) | (0xF0u << 24);
    *kSystRvr = cycles_per_tick - 1;
    *kSystCvr = 0;
    *kSystCsr = kSystEnable | kSystTickInt | kSystClkSource;
}

extern "C" void SysTick_Handler() {
    ZerOS::arch::cortex_m3::system_time.on_elapsed(1);
}