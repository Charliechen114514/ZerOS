// ZerOS 基线冒烟:banner + 1kHz SysTick —— 验收 = Renode(bluepill.resc)里两者可见
#include <cstdint>

#include "stm32f1xx.h"
#include "uart.hpp"

extern "C" void SysTick_Handler() {
    static std::uint32_t ticks = 0;
    if (++ticks % 1000 == 0) {
        ZerOS::board::print("tick ");
        ZerOS::board::printdec(ticks / 1000);
        ZerOS::board::print("\r\n");
    }
}

int main() {
    ZerOS::board::uart1_init();
    ZerOS::board::print("\r\nZerOS baseline: C++23 @ STM32F103C8T6 (Blue Pill, Renode)\r\n");
    ZerOS::board::print("cc: gcc ");
    ZerOS::board::printdec(__GNUC__);
    ZerOS::board::print("\r\n");

    SysTick_Config(SystemCoreClock / 1000); // 1 kHz
    for (;;) {
        __WFI();
    }
}
