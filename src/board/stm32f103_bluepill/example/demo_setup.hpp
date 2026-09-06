#pragma once

// The opening ritual every demo shares: bring up the board, hand the log
// system its channel, install the default testaments (print-and-halt),
// say who we are. Three lines of boilerplate, one call.

#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/system.hpp"
#include "ZerOS/arch/arm_cortex_m3/testaments.hpp"
#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/log/print.hpp"
#include "uart.hpp"

namespace ZerOS::demo {

inline void setup(const char* name) {
    board::uart1_init();
    board::register_uart_channel(); // this board talks over UART1
    log::print("\r\nZerOS demo: {}\r\n", name);
    arch::cortex_m3::install_default_testaments(); // obligations, explicitly
    arch::cortex_m3::init_time(72'000'000 / 1000); // 1kHz tick
}

} // namespace ZerOS::demo
