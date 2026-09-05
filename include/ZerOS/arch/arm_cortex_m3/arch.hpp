#pragma once
#include <cstdint>
namespace ZerOS::arch::cortex_m3 {

struct ExceptionFrame {
    std::uint32_t r0;
    std::uint32_t r1;
    std::uint32_t r2;
    std::uint32_t r3;
    std::uint32_t r12;
    std::uint32_t lr;
    std::uint32_t pc;
    std::uint32_t xpsr;
};

struct SoftwareFrame {
    std::uint32_t r4;
    std::uint32_t r5;
    std::uint32_t r6;
    std::uint32_t r7;
    std::uint32_t r8;
    std::uint32_t r9;
    std::uint32_t r10;
    std::uint32_t r11;
};

static_assert(sizeof(ExceptionFrame) == 8 * sizeof(std::uint32_t));
static_assert(sizeof(SoftwareFrame) == 8 * sizeof(std::uint32_t));
} // namespace ZerOS::arch::cortex_m3