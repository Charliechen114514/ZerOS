// stm32f103_bluepill DWT 周期计数器 — Cortex-M3 内置的免费秒表
// 72MHz 下 1µs = 72 counts;精度足够测上下文切换/D12 预算
#pragma once

#include <cstdint>

namespace ZerOS::board {

inline void dwt_init() {
    auto* const demcr     = reinterpret_cast<volatile std::uint32_t*>(0xE000EDFC);
    auto* const dwt_ctrl  = reinterpret_cast<volatile std::uint32_t*>(0xE0001000);
    auto* const dwt_cyccnt = reinterpret_cast<volatile std::uint32_t*>(0xE0001004);

    *demcr |= (1u << 24);    // TRCENA: unlock the DWT block
    *dwt_cyccnt = 0;          // reset the counter
    *dwt_ctrl |= 1u;          // CYCCNTENA: start counting
}

[[nodiscard]] inline std::uint32_t dwt_cycles() {
    return *reinterpret_cast<volatile std::uint32_t*>(0xE0001004);
}

} // namespace ZerOS::board
