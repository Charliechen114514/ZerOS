// mps2_an385 时钟:QEMU 固定 25MHz(info qtree:cpuclk/TIMCLK/pclk 全 25MHz),
// 无时钟树可配。clock_init 保留空操作——demo_setup 的开机仪式各板同形。
#pragma once

#include <cstdint>

namespace ZerOS::board {

inline constexpr std::uint32_t kCoreClockHz = 25'000'000;

inline void clock_init() {
    // 上电即 25MHz,无事可做
}

} // namespace ZerOS::board
