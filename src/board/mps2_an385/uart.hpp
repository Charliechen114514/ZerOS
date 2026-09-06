// mps2_an385 板级 UART0(CMSDK APB UART @ 0x40004000,轮询输出)
// 寄存器形态与 STM32 USART 完全不同:DATA/STATE/CTRL/BAUDDIV 扁平四件套
#pragma once

#include <cstdint>

namespace ZerOS::board {

// 波特率分频就绪(QEMU 下 BAUDDIV=1 全速直通;真硬件再算 25MHz/16/baud)
void uart0_init();

// 轮询发一字节(锁等待 STATE.TXBF 清零)——任务/ISR/遗言上下文皆可
void uart0_putc(char c);

void print(const char* s);

void register_uart_channel();

} // namespace ZerOS::board
