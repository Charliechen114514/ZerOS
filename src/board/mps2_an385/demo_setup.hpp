#pragma once

// 开机仪式的 mps2 版(与 bluepill/demo_setup.hpp 同形):
// 通道接线 + 遗言义务 + 1kHz tick。差异只在 UART 编号与主频。

#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/system.hpp"
#include "ZerOS/arch/arm_cortex_m3/testaments.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/log/print.hpp"
#include "clock.hpp"
#include "uart.hpp"

namespace ZerOS::demo {

inline void setup(const char* name) {
    board::clock_init();  // 固定 25MHz,空操作——仪式同形
    board::uart0_init();
    board::register_uart_channel(); // this board talks over UART0
    log::print("\r\nZerOS demo: {}\r\n", name);
    arch::cortex_m3::install_default_testaments(); // obligations, explicitly
    arch::cortex_m3::init_time(25'000'000 / 1000);  // 1kHz tick @25MHz
}

} // namespace ZerOS::demo
